
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "rtlbase.h"
#include "rtllist.h"

#include "rtlimsg.h"

#define	ABS(x)		((x) > 0 ? (x) : -(x))

#define	IM_DEF		0

#define	IM_TIMER_GEN	100
#define	IM_TIMER_GEN_V	10000

#define	IM_CLEAR_SCR	200

int	TraceLevel	= 1;
int	TraceDebug	= 0;

int	main(int argc,char *argv[])
{
	time_t	lasttimems	= 0;
	time_t	now		= 0;
	time_t	delta		= 0;
	unsigned int loop = 0;
	void	*IQ;
	t_imsg	*msg;
	int	tmt = 10000;
	int	i;
	unsigned int	nbmsg = 0;

	rtl_tracelevel(TraceLevel);
RTL_TRDBG(1,"expected values diffms=%d delta=%u loop=~%u\n",0,tmt,tmt);


	IQ	= rtl_imsgInitIq();
	rtl_imsgAdd(IQ,rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,tmt,NULL,0));
	rtl_imsgAdd(IQ,rtl_imsgAlloc(IM_DEF,IM_CLEAR_SCR,NULL,0));

	lasttimems	= rtl_tmmsmono();
	while (rtl_imsgCount(IQ) > 0 || rtl_timerCount(IQ) > 0)
	{
		// get all internal messages if any
		while ((msg= rtl_imsgGet(IQ,0)) != NULL)
		{
			sleep(1);
			rtl_imsgFree(msg);
			nbmsg++;
		}

		// wait for exernal events (poll())
		usleep(1000);	// 1ms
		loop++;

		// do timers if any
		while ((msg= rtl_imsgGet(IQ,1)) != NULL)
		{
			now		= rtl_tmmsmono();
			delta		= ABS(lasttimems - now);
			lasttimems	= now;
RTL_TRDBG(1,"timer diffms=%d delta=%u loop=%u nbmsg=%d\n",
				msg->im_diffms,delta,loop,nbmsg);
			rtl_imsgFree(msg);
			rtl_imsgAdd(IQ,rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,tmt,NULL,0));
			loop	= 0;
			rtl_imsgAdd(IQ,rtl_imsgAlloc(IM_DEF,IM_CLEAR_SCR,NULL,0));
		}
	}

	return	0;
}
