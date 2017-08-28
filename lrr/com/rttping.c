/*
* Copyright (C) Actility, SA. All Rights Reserved.
* DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 only, as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* General Public License version 2 for more details (a copy is
* included at /legal/license.txt).
*
* You should have received a copy of the GNU General Public License
* version 2 along with this work; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*
* Please contact Actility, SA.,  4, rue Ampere 22300 LANNION FRANCE
* or visit www.actility.com if you need additional
* information or have any questions.
*/

/*! @file netitf.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>
#ifndef MACOSX
#include <malloc.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <dirent.h>

#include "rtlbase.h"
#include "rtlimsg.h"
#include "rtllist.h"
#include "rtlhtbl.h"

#include "semtech.h"

#include "xlap.h"
#include "infrastruct.h"
#include "struct.h"

#include "headerloramac.h"

typedef unsigned char u8;
typedef unsigned short u16;
u16 crc_ccitt(u16 crc, const u8 *buffer, int len);

// Not needed here #include "_whatstr.h"
#include "define.h"
#include "cproto.h"
#include "extern.h"

static	int	DoPing(t_wan_itf *itf,char *targetaddr)
{
	char	cmd[1024];
	char	resp[1024];
	FILE	*pp;
	int	status;
	int	lg;

	int	ok = 0;
	int	rtt = 0;
	int	line = 0;

	if	(!targetaddr || !*targetaddr)
		return	-1;

	cmd[0]	= '\0';

	strcat	(cmd,"ping ");
#if	1
	// when using -sX where X < 4 we can not get RTT infos ...
	if	(itf->it_type == 1)
	{
		strcat	(cmd,"-s8 ");
	}
	lg	= strlen(cmd);
#else
	lg	= 0;	
#endif

#if	defined(WIRMAV2) || defined(IR910) || defined(MTAC) || defined (MTAC_USB) || defined(MTCAP) || defined(NATRBPI)
	sprintf	(cmd+lg," -w%d -W%d -c1 -I%s %s 2>&1",
					10,10,itf->it_name,targetaddr);
#else
	sprintf	(cmd+lg," -c1 -I%s %s 2>&1",itf->it_name,targetaddr);
#endif
	RTL_TRDBG(3,"%s\n",cmd);

	pp	= popen(cmd,"r");
	if	(!pp)
		return	-1;

	while	(fgets(resp,sizeof(resp)-10,pp) != NULL && line < 10)
	{
		char	*pt;

		line++;
		pt	= strstr(resp,"time=");
		if	(!pt)		continue;
		pt	+= 5;
		if	(!pt || !*pt)	continue;
		ok	= 1;
		rtt	= atoi(pt);
	}

	status	= pclose(pp);
	if	(status == -1)
	{
RTL_TRDBG(1,"end popen command cmd='%s' cannot get exitstatus err=%s\n",
				cmd,STRERRNO);
		return	-1;
	}
	status	= WEXITSTATUS(status);

	sprintf	(cmd,"ping on %s for %s => status=%d ok=%d tms=%d",
			itf->it_name,targetaddr,status,ok,rtt);
	RTL_TRDBG(3,"%s\n",cmd);

	if	(status != 0)
	{
		// this appends if itf is down or DNS resolution failure
		return	0;
	}

	if	(ok == 0)
	{
		// this appends if the dest addr does not answer or packet lost
		return	0;
	}
	if	(rtt == 0)
		rtt	= 1;	//not less than 1 ms
	return	rtt;
}

static	int	ComputePingRtt(t_wan_itf *itf,char *targetaddr)
{
	int	reportPeriod	= WanRefresh;
	int	rtt;

	rtt	= DoPing(itf,targetaddr);
	if	(rtt < 0)	
	{
		return	-1;
	}
	itf->it_sentprtt++;
	if	(rtt == 0)
	{
		itf->it_lostprtt++;
		return	0;
	}
	if	(rtt > 0)
	{
		// separate thread and low frequency we have time to 
		// compute avdv after each new value, so the main thread
		// has always fresh data
		itf->it_okayprtt++;
		AvDvUiAdd(&itf->it_avdvprtt,rtt,Currtime.tv_sec);
		itf->it_nbpkprtt = 
		AvDvUiCompute(&itf->it_avdvprtt,reportPeriod,Currtime.tv_sec);
		return	rtt;
	}
	return	rtt;
}

static	void	*LoopItfThread(void *pitf)
{
	t_wan_itf	*itf		= (t_wan_itf *)pitf;
	char		*targetaddr	= CfgStr(HtVarLrr,"laplrc",0,"addr","");
	//int		rtt;

	RTL_TRDBG(0,"thread itf idx=%d name='%s' is looping\n",
			itf->it_idx,itf->it_name);

	itf->it_lastprtt	= Currtime.tv_sec;
	itf->it_lastavdv	= Currtime.tv_sec;

	while	(1)
	{
		sleep	(1);
		if	(PingRttPeriod && 
			ABS(Currtime.tv_sec - itf->it_lastprtt) > PingRttPeriod)
		{
			itf->it_lastprtt	= Currtime.tv_sec;
			// warning: variable ‘rtt’ set but not used [-Wunused-but-set-variable]
			// rtt = ComputePingRtt(...
			ComputePingRtt(itf,targetaddr);
		}
		if	(WanRefresh &&
			ABS(Currtime.tv_sec - itf->it_lastavdv) > WanRefresh)
		{
			itf->it_lastavdv	= Currtime.tv_sec;
	RTL_TRDBG(1,"ping thd %s st=%u ok=%u ls=%u nb=%d av=%u dv=%u mx=%u\n",
			itf->it_name,itf->it_sentprtt,itf->it_okayprtt,
			itf->it_lostprtt,
			itf->it_nbpkprtt,
			itf->it_avdvprtt.ad_aver,
			itf->it_avdvprtt.ad_sdev,
			itf->it_avdvprtt.ad_vmax);
		}
	}
	return	NULL;
}

void	StartItfThread()
{
	int	i;
	pthread_attr_t	threadAt;
	pthread_t	*thread;
	t_wan_itf	*itf;

	if	(pthread_attr_init(&threadAt))
	{
		RTL_TRDBG(0,"cannot init thread itf err=%s\n",STRERRNO);
		return;
	}

	for	(i = 0; i < NB_ITF_PER_LRR ; i++)
	{
		if	(!TbItf[i].it_enable)	continue;
		if	(!TbItf[i].it_name)	continue;

		itf		= &TbItf[i];
		itf->it_idx	= i;
		thread		= &(itf->it_thread);
		if(pthread_create(thread,&threadAt,LoopItfThread,(void *)itf))
		{
			RTL_TRDBG(0,"cannot create thread itf err=%s\n",STRERRNO);
			continue;
		}
		RTL_TRDBG(0,"thread itf idx=%d name='%s' is started\n",
			i,TbItf[i].it_name);
	}
}
