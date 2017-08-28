
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

#define	IM_DEF		0

#define	IM_TIMER_GEN	100
#define	IM_TIMER_GEN_V	500	// 500ms

#define	IM_MSG_INFO	1000
#define	IM_MSG_RESP	1001
#define	IM_MSG_START	1002
#define	IM_MSG_STOP	1003
#define	IM_MSG_NOP	1004
#define	IM_MSG_RNOP	1005

#define	NB_THREAD	30

typedef	struct
{
	int	th_num;
	void	*th_iq;
	int	th_value;
	unsigned	int	th_send;
	unsigned	int	th_recv;
}	t_th;

t_th	MainTh;
t_th	TabTh[NB_THREAD];


unsigned int NbMess;
unsigned int NbThread	= NB_THREAD;

// by default use conditionnal wait (no polling on queues)
int	CondWait	= 1;

void	*IQ;

void	ClearScreen()
{
//	printf("[H[2J");
	printf("\033[2J");
}

void	ClearLine()
{
//	printf("[H[2K");
	printf("\033[0K");
}

void	GotoXy(int x,int y)
{
	printf	("\033[%d;%dH",y,x) ;
}

void	GotoLc(int l,int c)
{
	GotoXy(c,l);
}

void	DisplayValueForThread(t_th *th)
{
	char	buf[100];

	GotoLc(th->th_num+3,1);
	ClearLine();

	GotoLc(th->th_num+3,1);
	printf	("%09u %09u %09u ",th->th_send,th->th_recv,
						rtl_imsgCount(th->th_iq));
	memset	(buf,'.',80);
	buf[79]	= 0;
	memset	(buf,'@'+th->th_num+1,th->th_value);
	printf	("%s",buf);

}

void	DisplayValueForMain(t_th *th)
{
	GotoLc(2,1);
	ClearLine();

	GotoLc(2,1);
	printf	("%09u %09u %09u ",th->th_send,th->th_recv,
						rtl_imsgCount(th->th_iq));
}

void	DisplayInfo(uint32 nbmess)
{
	static	double	fmax;
	static	uint32	pm;
	static	time_t	pt;

	static	uint32	dmmax;
	static	uint32	dtmax;

	time_t	dt;
	time_t	ms	= rtl_tmmsmono();
	uint32	dm;


	GotoLc(1,1);
	ClearLine();
	GotoLc(1,1);
	printf("%09u ",nbmess);

	dt	= abs(ms - pt);
	dm	= abs(nbmess - pm);

	if	(dt > dtmax)
		dtmax	= dt;
	if	(dm > dmmax)
		dmmax	= dm;

	if	(dm != 0 && pm != 0)
	{
		double	f;
		f	= ((double)dm / (double)dt) * 1000.0;
		if	(f > fmax)
			fmax	= f;
		printf("dm=%09u dt=%09u msg/s=%f (%09u,%09u,%f)",
				dm,(uint32)dt,f,
				dmmax,dtmax,fmax);
	}

	pm	= nbmess;
	pt	= ms;
}

void	*RunThread(void *pth)
{
	t_th	*th	= (t_th *)pth;
	t_imsg	*imsg,*omsg;
	int	num;
	int	i;
	int	av;
	int	nb = 0;

	num	= th->th_num;
	printf	("thread %d started\n",num);

	if	(CondWait == 0)
	{
pause:
		imsg	= rtl_imsgGet(th->th_iq,IMSG_MSG);
		if	(!imsg)
		{
			sleep(1);
			goto	pause;
		}
	}
	else
	{
		imsg	= rtl_imsgWait(th->th_iq,IMSG_MSG);
	}
	if	(!imsg || imsg->im_type != IM_MSG_START)
	{
		printf("self end thread %d msg=%p type=%d\n",th->th_num,
		imsg,imsg?imsg->im_type:-1);
		goto	endthread;
	}

	while	(1)
	{
		if	(CondWait == 0)
			imsg	= rtl_imsgGet(th->th_iq,IMSG_MSG);
		else
			imsg	= rtl_imsgWait(th->th_iq,IMSG_MSG);
		if	(!imsg)
		{
			continue;
		}
		nb++;
		if	(imsg->im_type == IM_MSG_INFO)
		{
			int	*tab;

			av	= 0;
			tab	= (int *)imsg->im_dataptr;
			for	(i = 0 ; i < NbThread ; i++)
			{
				av	= av + tab[i];
			}
			av		= av / NbThread;
			if	(nb % 10 == 0)
				av	= (rand() % NbThread)+10;
			th->th_value	= av;
			th->th_send++;
			NbMess++;
			omsg	= rtl_imsgAlloc(IM_DEF,IM_MSG_RESP,NULL,0);
			rtl_imsgSetData(omsg,th);
			rtl_imsgAdd(IQ,omsg);
		}
		rtl_imsgFree(imsg);
		if	(CondWait == 0)
		{
			usleep(10000);
		}
	}

endthread :
	omsg	= rtl_imsgAlloc(IM_DEF,IM_MSG_STOP,NULL,0);
	rtl_imsgSetData(omsg,th);
	rtl_imsgAdd(IQ,omsg);

	pthread_exit(NULL);

	return	NULL;
}

void	Usage()
{
printf("-h	: this help\n");
printf("-p	: polling (msgGet()) instead of waiting mode (msgWait())\n");
printf("-n nb	: number of threads [0..%d]\n",NB_THREAD);
}

int	main(int argc,char *argv[])
{
	t_imsg	*imsg,*omsg;
	int	tmt = IM_TIMER_GEN_V;
	int	i;
	int	running = NbThread;
	unsigned short	tabvalue[NB_THREAD];
	int	c;

	while	((c = getopt(argc,argv,"hpn:")) != -1)
	{
		switch	(c)
		{
		case	'h':
			Usage();
			exit(0);
		break;
		case	'p':
			CondWait	= 0;
		break;
		case	'n':
			NbThread	= atoi(optarg);
			if	(NbThread > NB_THREAD)
				NbThread	= NB_THREAD;
			if	(NbThread <= 0)
				NbThread	= 1;
		break;
		}
	}

	ClearScreen();
	rtl_tracemutex();

	running = NB_THREAD;
	IQ	= rtl_imsgInitIq();
	if (!IQ)
	{
		printf("cannot alloc main queue\n");
		exit(1);
	}
	if	(CondWait == 0)
	{
		rtl_imsgAdd(IQ,rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,tmt,NULL,0));
	}
	MainTh.th_iq	= IQ;
	MainTh.th_num	= NB_THREAD;

	for (i = 0; i < NB_THREAD; i++) 
	{
		pthread_attr_t	at;
		pthread_t	th;

		tabvalue[i]	= i+10;
		TabTh[i].th_num	= i;
		TabTh[i].th_iq	= rtl_imsgInitIq();
		if (!TabTh[i].th_iq)
		{
			printf("cannot alloc thread queue\n");
			exit(1);
		}
/*
		rtl_imsgAdd(TabTh[i].th_iq,
				rtl_imsgAlloc(IM_DEF,IM_MSG_START,NULL,0));
*/

		if (pthread_attr_init(&at))
		{
			rtl_trace(0,"cannot init thread attr\n");
			exit(1);
		}
		pthread_attr_setdetachstate(&at,PTHREAD_CREATE_DETACHED);
		if (pthread_create(&th,&at,RunThread,(void *)&TabTh[i]))
		{
			rtl_trace(0,"cannot create thread\n");
			exit(1);
		}
	}
	sleep(1);
	for (i = 0; i < NB_THREAD; i++) 
	{
		rtl_imsgAdd(TabTh[i].th_iq,
				rtl_imsgAlloc(IM_DEF,IM_MSG_START,NULL,0));

		omsg	= rtl_imsgAlloc(IM_DEF,IM_MSG_INFO,NULL,0);
		rtl_imsgCpyData(omsg,(void *)tabvalue,sizeof(tabvalue));
		rtl_imsgAdd(TabTh[i].th_iq,omsg);
	}

	while	(running > 0)
	{
		t_th	*th;
//retry1 :
		while (1)
		{
			if (CondWait == 0)
			{
				imsg	= rtl_imsgGet(IQ,IMSG_BOTH);
			}
			else
			{
				imsg	= rtl_imsgWait(IQ,IMSG_MSG);
			}
			if	(imsg == NULL)
				break;
			if (imsg->im_type == IM_MSG_STOP)
			{
				running--;
				th	= (t_th *)imsg->im_dataptr;
				printf("end thread %d running=%d\n",
					th->th_num,running);
				if (&TabTh[th->th_num] == th)
					rtl_imsgFreeIq(th->th_iq);
			}
			if (imsg->im_type == IM_MSG_RESP)
			{
				th	= (t_th *)imsg->im_dataptr;
//				printf	("th=%d value=%d\n",th->th_num,
//								th->th_value);
				th->th_recv++;
				if	(th->th_value >= NB_THREAD)
					th->th_value = NB_THREAD;
				if	(th->th_value <= 0)
					th->th_value = NB_THREAD;
				tabvalue[th->th_num]	= th->th_value;
				DisplayValueForThread(th);
				NbMess++;
				MainTh.th_recv++;
				omsg	= rtl_imsgAlloc(IM_DEF,IM_MSG_INFO,NULL,0);
				rtl_imsgCpyData(omsg,(void *)tabvalue,sizeof(tabvalue));
				rtl_imsgAdd(th->th_iq,omsg);
				MainTh.th_send++;
			}
			if (CondWait == 0 && imsg->im_type == IM_TIMER_GEN)
			{
				DisplayInfo(NbMess);
				DisplayValueForMain(&MainTh);
				rtl_imsgAdd(IQ,
				rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,tmt,NULL,0));
			}
			if (CondWait && (MainTh.th_recv % 1000) == 0)
			{
				DisplayInfo(NbMess);
				DisplayValueForMain(&MainTh);
			}
			rtl_imsgFree(imsg);
		}
	}

	rtl_imsgFreeIq(IQ);

	return	0;

}

