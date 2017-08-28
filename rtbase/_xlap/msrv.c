
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>
#define	_GNU_SOURCE
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "rtlbase.h"
#include "rtlepoll.h"
#include "rtlelap.h"
#define	RTL_INTERNAL_IQMSG_STRUCT
#include "rtllist.h"
#include "rtlhtbl.h"

#define	IM_DEF		1000
#define	IM_TEST_REQ		1000
#define	IM_EXIT_LOOP		1001
#define	IM_START_LAP_LRR	1002
#define					IM_START_LAP_LRR_V	15000
#define	IM_TIMER_LAP_LRR	1003
#define					IM_TIMER_LAP_LRR_V	1000
#define	IM_TIMER_MSG		1004
#define					IM_TIMER_MSG_V		AsyncResp
#define	IM_TIMER_THREAD		1005
#define					IM_TIMER_THREAD_V	5000

#define	NB_THREAD		30

#define	EXIT(v,s,...) \
{\
	printf("line=%d %s (%d)",__LINE__,(s),(v));\
	exit((1));\
}

int	Argc;
char	**Argv;

int     TraceLevel      = 0;
int     TraceDebug      = 0;

u_int		ApiFlags;
void		*MainQ;
void		*MainTbPoll;
t_xlap_ctxt	*LapList;
t_xlap_link	LapLrc;
u_int		LapFlags =	
//		LK_TCP_LISTENER|LK_SSP_MASTER|LK_SSP_AUTOSTART;
		LK_TCP_LISTENER|LK_SSP_MASTER|LK_TCP_NONBLOCK;	// like LRC

pthread_t	XlapThread;
pthread_attr_t	XlapThreadAt;

pthread_t	CliThread[NB_THREAD];
pthread_attr_t	CliThreadAt[NB_THREAD];
void 		*CliQ[NB_THREAD];
u_int		CliSleep[NB_THREAD];
void		*CliMsgQ[NB_THREAD];
int		CliModeChange[NB_THREAD];

u_int	NbMsg;
u_int	PrevNbMsg;
u_int	NbTmt;
u_int	PrevNbTmt;

int	Verbose		= 0;
int	TimerPerLrr	= 0;
int	TimeProc	= 0;
int	ModeMsg		= 'W';	// P:rtl_imsgGet W:rtl_imsgWait
int	AsyncResp	= 300;	// delayed response in ms
int	ModeDisc	= 0;

//////////////////////////////////////////////////////////////////////////////
// sub thread part
//////////////////////////////////////////////////////////////////////////////

int ProceedMsg(int num,t_imsg *msg)
{
	int	ret	= 1;
	int	fd	= 0;
	u_int	serial	= 0;
	t_imsg	*wmsg;

	switch (msg->im_type) 
	{
	case IM_EXIT_LOOP :
		ret	= 0;
	break;
	case IM_TEST_REQ :
		NbMsg++;
		fd	= (int)msg->im_to;
		serial	= msg->im_serialto;
if (Verbose)
{
	printf("th=%d fd=%d serial=%u pkttimeproc=%d for msg\n",num,fd,serial,TimeProc);
}
		if	(TimeProc > 0)
			usleep(TimeProc * 1000);

		if	(AsyncResp)
		{
			rtl_imsgAdd(CliQ[num],
				rtl_timerAlloc(IM_DEF,IM_TIMER_MSG,
				IM_TIMER_MSG_V,NULL,0));
		}

		if	(AsyncResp)
		{	// requeue message in thread queue so dont free it
			rtl_imsgAdd(CliMsgQ[num],msg);
			return	ret;
		}

		rtl_eLapPutOutQueueExt(LapList,fd,serial,
						msg->im_dataptr,msg->im_datasz);
	break;
	case	IM_TIMER_MSG :
		NbTmt++;
if (Verbose)
{
	printf("th=%d timer expire\n",num);
}

//		if	(AsyncResp)
		{
			wmsg	= rtl_imsgGet(CliMsgQ[num],IMSG_MSG);
			if	(wmsg)
			{
				fd	= (int)wmsg->im_to;
				serial	= wmsg->im_serialto;
if (Verbose)
{
	printf("th=%d fd=%d serial=%u retrieve msg\n",num,fd,serial);
}
				rtl_eLapPutOutQueueExt(LapList,fd,serial,
					wmsg->im_dataptr,wmsg->im_datasz);
				rtl_imsgFree(wmsg);
			}
			else
			{	// error
			}
		}
	break;
	case	IM_TIMER_THREAD :
if (Verbose)
{
	printf("th=%d private timer expire diff=%d\n",num,(int)msg->im_diffms);
}
		rtl_imsgAdd(CliQ[num],
			rtl_timerAlloc(IM_DEF,IM_TIMER_THREAD,
			IM_TIMER_THREAD_V,NULL,0));
	break;
	default:
	break;
	}
	rtl_imsgFree(msg);
	return	ret;
}


void CliLoopWait(int num) 
{
	t_imsg  *msg;
	printf	("th=%d enter loop\n",num);

	CliSleep[num]		= 0;
	while	(1)
	{
		int	ms;

		ms	= 100;
//		if ((msg= rtl_imsgWait(CliQ[num],IMSG_MSG))) 
		msg= rtl_imsgWaitTime(CliQ[num],IMSG_BOTH,&ms);
		if (!msg)
		{
			if	(Verbose)
			{
				printf("th=%d no msg => wait\n",num);
			}
			continue;
		}
		if	(!ProceedMsg(num,msg))
			break;
	}
	printf	("th=%d exit loop\n",num);
	if	(CliModeChange[num])
	{
		printf("th=%d change polling mode %c\n",num,ModeMsg);
		CliModeChange[num]	= 0;
		CliSleep[num]		= 0;
	}
}

void CliLoopGet(int num) 
{
	t_imsg  *msg;
	int	stop	= 0;

	printf	("th=%d enter loop\n",num);
	while	(!stop)
	{
		int	count = 0;
		while ((msg= rtl_imsgGet(CliQ[num],IMSG_BOTH))) 
		{
			if	(!ProceedMsg(num,msg))
			{
				stop	= 1;
				break;
			}
			count++;
		}
		CliSleep[num] = count ? 1000/count : 1000;
		if	(0 && Verbose)
			printf	("th=%d usleep (%d)\n",num,CliSleep[num]);
		if (CliSleep[num])
			usleep (CliSleep[num]);
	}
	printf	("th=%d exit loop\n",num);
	if	(CliModeChange[num])
	{
		printf("th=%d change polling mode %c\n",num,ModeMsg);
		CliModeChange[num]	= 0;
	}
}

void CliLoop(int num) 
{
	rtl_imsgAdd(CliQ[num],rtl_timerAlloc(IM_DEF,IM_TIMER_THREAD,
			IM_TIMER_THREAD_V,NULL,0));

	while	(1)
	{
		switch(ModeMsg)
		{
		case	'W' :
			CliLoopWait(num);
		break;
		case	'P' :
		default :
			CliLoopGet(num);
		break;
		}
	}
}

void	cleanup_thread(void *param) 
{
	int num = *(int *)param;
	rtl_imsgRemoveAll(CliQ[num]);
	free (param);
}
void	*RunThread(void *pth)
{
	int	num = (int)pth;
	pthread_cleanup_push(cleanup_thread, pth);
	CliLoop(num);
	pthread_cleanup_pop(0);
	return	NULL;
}

//////////////////////////////////////////////////////////////////////////////
// main thread part
//////////////////////////////////////////////////////////////////////////////

int	LapEventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
	static	u_int	nbmsg	= 0;
	static	u_int	rr	= 0;
	t_imsg	*msg;

if	(evtnum == EVT_LK_CREATED || evtnum == EVT_LK_DELETED)
{
printf("LAP CB (%s,%s/%s,%s) fd=%d st='%s' receive evttype=%d evtnum='%s'\n",
	lk->lk_addr,lk->lk_port,lk->lk_rhost,lk->lk_rport,lk->lk_fd,
	rtl_eLapStateTxt(lk->lk_state),evttype,rtl_eLapEventTxt(evtnum));
}
	switch	(evtnum)
	{
	case	EVT_LK_CREATED :
		if	(!IS_SSP_AUTOSTART(lk))
		{
			t_imsg	*msg;

			msg	= rtl_imsgAlloc(IM_DEF,IM_START_LAP_LRR,
						lk,lk->lk_userserial);
			if	(!msg)
				break;
			rtl_imsgAddDelayed(MainQ,msg,IM_START_LAP_LRR_V);

		}
	break;
	case	EVT_LK_DELETED :
	break;
	case	EVT_LK_STARTED :
	break;
	case	EVT_LK_STOPPED :
	break;
	case	EVT_FRAMEI :
		nbmsg++;
		msg	= rtl_imsgAlloc(IM_DEF, IM_TEST_REQ, NULL, 0);
		msg->im_to		= (void *)lk->lk_fd;
		msg->im_serialto	= lk->lk_serial;
		rtl_imsgDupData (msg, data, sz);
		rtl_imsgAdd (CliQ[rr%NB_THREAD], msg);
		rr++;
		if	(ModeDisc && ((nbmsg % 1000)) == 0)
		{
			rtl_eLapDisc(lk,"");
		}
	break;
	}

	return	0;
}

void	ThreadInfo()
{
	int	i;

	for	(i = 0 ; i < NB_THREAD ; i++)
	{
		printf	("th=%d #qmsg=%d #qtmt=%d #qpending=%d lastusleep=%d\n",
			i,rtl_imsgCount(CliQ[i]),rtl_timerCount(CliQ[i]),
			rtl_imsgCount(CliMsgQ[i]),CliSleep[i]);
	}
}

// to illustrate how to add other file descriptors in the poll context
// a basic command line interface is used on stdin
void	Usage()
{
if	(ModeMsg == 'W')
	printf	("P get message mode by polling/usleep (%c)\n",ModeMsg);
if	(ModeMsg == 'P')
	printf	("W get message mode by event/condwait (%c)\n",ModeMsg);
printf	("a async response mode (%s %dms)\n",AsyncResp?"yes":"no",AsyncResp);
printf	("v verbose mode\n");
printf	("t threads infos\n");
printf	("q to quit\n");
printf	("- + change async response delay (%u)\n",AsyncResp);
printf	("/ * change time processing for a message (%u)\n",TimeProc);
printf	("0 time processing to 0ms\n");
}

// called for each fd of the poll context during poll(2)
// here only stdin has been added via rtl_pollAdd()
int	CB_StdinEvent(void *tb,int fd,void *r1,void *r2,int revents)
{
	int	c;
	int	i;


	if	(fd != fileno(stdin))
		return	0;

	rtl_epollSetEvt(MainTbPoll,fileno(stdin),POLLIN);
	if	((revents & POLLIN) != POLLIN)
		return	1;

	c	= getchar();
	switch	(c)
	{
	default :
	case	'?':
		Usage();
	break;
	case	'P' :
		ModeMsg	= 'P';
		for	(i = 0 ; i < NB_THREAD ; i++)
		{
			t_imsg	*msg;
			CliModeChange[i]	= 1;
			msg	= rtl_imsgAlloc(IM_DEF, IM_EXIT_LOOP, NULL, 0);
			rtl_imsgAdd (CliQ[i], msg);
		}
	break;
	case	'W' :
		ModeMsg	= 'W';
		for	(i = 0 ; i < NB_THREAD ; i++)
		{
			t_imsg	*msg;
			CliModeChange[i]	= 1;
			msg	= rtl_imsgAlloc(IM_DEF, IM_EXIT_LOOP, NULL, 0);
			rtl_imsgAdd (CliQ[i], msg);
		}
	break;
	case	'a' :
		if	(AsyncResp)
			AsyncResp	= 0;
		else
		{
			AsyncResp	= 100;
		}
	break;
	case	'd' :
		ModeDisc	= !ModeDisc;
	break;
	case	'v' :
		Verbose	= !Verbose;
	break;
	case	't' :
		ThreadInfo();
	break;
	case	'/' :
		TimeProc	= TimeProc - 1;
		if	(TimeProc < 0)
			TimeProc	= 0;
	break;
	case	'*' :
		TimeProc	= TimeProc + 1;
	break;
	case	'0' :
		TimeProc	= 0;
	break;
	case	'-' :
		AsyncResp	= AsyncResp - 10;
		if	(AsyncResp < 0)
			AsyncResp	= 0;
	break;
	case	'+' :
		AsyncResp	= AsyncResp + 10;
	break;
	case	'q' :
		exit(0);
	break;
	}

	return	1;
}

static	void	DoInternalTimer(t_imsg *imsg)
{
	switch(imsg->im_class)
	{
	case	IM_DEF :
		switch(imsg->im_type)
		{
		case	IM_TIMER_LAP_LRR :
			if	(TimerPerLrr)
			{
				rtl_imsgAdd(MainQ,
				rtl_timerAlloc(IM_DEF,IM_TIMER_LAP_LRR,
				IM_TIMER_LAP_LRR_V,NULL,0));
			}
		break;
		case	IM_TIMER_MSG :
		break;
		}
	break;
	}
	NbTmt++;
}

static	void	DelayedStart(t_xlap_link *lk,u_int serial)
{
	rtl_eLapEventRequest(lk,EVT_STARTDTact,NULL,0);
	if	(TimerPerLrr)
	{
		rtl_imsgAdd(MainQ,
			rtl_timerAlloc(IM_DEF,IM_TIMER_LAP_LRR,
			IM_TIMER_LAP_LRR_V,NULL,0));
	}
}

static	void	DoInternalEvent(t_imsg *imsg)
{
	RTL_TRDBG(3,"receive event cl=%d ty=%d\n",imsg->im_class,imsg->im_type);
	switch(imsg->im_class)
	{
	case	IM_DEF :
		switch(imsg->im_type)
		{
		case	IM_START_LAP_LRR :
			DelayedStart(imsg->im_to,imsg->im_serialto);
		break;
		}
	break;
	}
}

int	xlapmain(int argc,char *argv[])
{
//	struct sigaction sigact;
	char	*port	= "1234";
	char	*addr	= "0.0.0.0";
	int	flags	= 0;

	time_t	lasttimems	= 0;
	time_t	lasttimesc	= 0;
	time_t	lasttimehr	= 0;
	time_t	now		= 0;

	flags	= flags | LAP_LONG_FRAME;
	flags	= flags | LAP_NO_TCP_CLI;
	ApiFlags= flags;

	if	(argc > 1)
		port	= argv[1];
	if	(argc > 2)
		addr	= argv[2];

	printf	("bind srv '%s' '%s'\n",addr,port);

	rtl_init();
	rtl_tracemutex();
	MainQ		= rtl_imsgInitIq();
	MainTbPoll	= rtl_epollInit();
	LapList		= rtl_eLapInit(flags,MainTbPoll);

	// add stdin in poll context
	rtl_epollAdd(MainTbPoll,fileno(stdin),CB_StdinEvent,NULL,NULL,NULL);
	rtl_epollSetEvt(MainTbPoll,fileno(stdin),POLLIN);

	strcpy	(LapLrc.lk_name,"server");
	LapLrc.lk_addr		= addr;
	LapLrc.lk_port		= port;
	LapLrc.lk_type		= LapFlags;
	LapLrc.lk_cbEvent	= LapEventProceed;
	rtl_eLapAddLink(LapList,&LapLrc);

	rtl_eLapBindAll(LapList);

	while	(1)
	{
		t_imsg	*msg;
		int	nbd;
		int	left;
		int	err;

		// internal events
		while ((msg= rtl_imsgGet(MainQ,IMSG_BOTH)) != NULL)
		{
			DoInternalEvent(msg);
			rtl_imsgFree(msg);
		}

		nbd	= rtl_eLapDispatchQueueExt(LapList,-1,&left,&err);	
		if	(Verbose && (nbd != 0 || err != 0))
			printf("dispatch nb=%d left=%d error=%d\n",nbd,left,err);
		rtl_epollRaw(MainTbPoll,30);	// in ms

		// clocks
		now	= rtl_tmmsmono();
		if	(abs(now-lasttimems) >= 100)
		{
			lasttimems	= now;
			rtl_eLapDoClockMs(LapList,10);
		}
		if	(abs(now-lasttimesc) >= 1000)
		{
			static	t_xlap_stat	Prevst;
			t_xlap_stat	*st;
			st	= rtl_eLapGetStat(LapList);

			printf("%umsg/s %utmt/s m=%c pkttimeproc=%d\n",
				abs(NbMsg-PrevNbMsg),
				abs(NbTmt-PrevNbTmt),
				ModeMsg,TimeProc);
			printf	("sendB/s=%09u #writ/s=%09u recvB/s=%09u #read/s=%09u\n",
				abs(st->st_nbsendB-Prevst.st_nbsendB),
				abs(st->st_nbsendw-Prevst.st_nbsendw),
				abs(st->st_nbrecvB-Prevst.st_nbrecvB),
				abs(st->st_nbrecvr-Prevst.st_nbrecvr));

			printf	("sendB=%09u #writ=%09u recvB=%09u #read=%09u\n",
				st->st_nbsendB,st->st_nbsendw,
				st->st_nbrecvB,st->st_nbrecvr);
			PrevNbMsg	= NbMsg;
			PrevNbTmt	= NbTmt;
			memcpy	(&Prevst,st,sizeof(Prevst));
//			rtl_eLapDoClockSc(LapList);
			lasttimesc	= now;
		}
		if	(abs(now-lasttimehr) >= 3600000) {
			lasttimehr	= now;
		}

		// internal timer
		while ((msg= rtl_imsgGet(MainQ,IMSG_TIMER)) != NULL)
		{
			DoInternalTimer(msg);
			rtl_imsgFree(msg);
		}
	}

	return	0;
}

void	*RunXlap(void *pth)
{
	xlapmain(Argc,Argv);
	exit(0);
}

int	main(int argc,char *argv[])
{
	struct sigaction sigact;
	int	i;

	Argc	= Argc;
	Argv	= Argv;

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags		= 0;
	sigact.sa_handler	= SIG_IGN;
	sigaction(SIGHUP,&sigact,NULL);
	sigaction(SIGPIPE,&sigact,NULL);
	sigaction(SIGQUIT,&sigact,NULL);
//	sigaction(SIGINT,&sigact,NULL);

	for	(i = 0 ; i < NB_THREAD ; i++)
	{
		CliQ[i] = rtl_imsgInitIq();
	}

	if	(pthread_attr_init(&XlapThreadAt))
	{
		EXIT(1,"cannot init thread attr err=%s\n",STRERRNO);
	}
	if (pthread_create(&XlapThread,&XlapThreadAt,RunXlap,(void *)0))
	{
		EXIT(1,"cannot create thread\n");
	}
	pthread_setname_np(XlapThread,"msrv-xlap");

	for	(i = 0 ; i < NB_THREAD ; i++)
	{
		char	name[16];

		if	(pthread_attr_init(&CliThreadAt[i]))
		{
			EXIT(1,"cannot init thread attr err=%s\n",STRERRNO);
		}
		if (pthread_create(&CliThread[i],NULL,RunThread,(void *)i))
		{
			EXIT(1,"cannot create thread\n");
		}
		sprintf	(name,"msrv-work_%d",i);
		pthread_setname_np(CliThread[i],name);

		CliMsgQ[i]	= rtl_imsgInitIq();
		if	(!CliMsgQ[i])
		{
			EXIT(1,"cannot alloc iqueue\n");
		}
	}

	while	(1)
		sleep(30);

	return	0;
}
