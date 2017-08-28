
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
#include "rtlhtbl.h"
#include "rtlxlap.h"

#define	EXIT(v,s,...) \
{\
	printf("line=%d %s (%d)",__LINE__,(s),(v));\
	exit((1));\
}

#define	NB_THREAD	10

typedef	struct
{
	unsigned int num;
	unsigned int cnt;
}	t_ref;

int     TraceLevel      = 0;
int     TraceDebug      = 0;

pthread_t	CliThread[NB_THREAD];


void			*SrvTbPoll;
struct	list_head	*SrvLapList;
t_xlap_link		SrvLapLrc;
u_int			SrvLapFlags =	
			LK_TCP_LISTENER|LK_SSP_MASTER|LK_SSP_AUTOSTART;

u_int			CliLapFlags =	
		LK_TCP_CLIENT|LK_SSP_SLAVE|LK_TCP_RECONN|LK_LNK_SAVEDNSENT;


int	SrvLapEventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
RTL_TRDBG(1,"LAP CB (%s,%s/%s,%s) fd=%d st='%s' receive evttype=%d evtnum='%s'\n",
		lk->lk_addr,lk->lk_port,lk->lk_rhost,lk->lk_rport,lk->lk_fd,
		rtl_LapStateTxt(lk->lk_state),evttype,rtl_LapEventTxt(evtnum));
	switch	(evtnum)
	{
	case	EVT_LK_CREATED :
	break;
	case	EVT_LK_DELETED :
	break;
	case	EVT_LK_STARTED :
	break;
	case	EVT_LK_STOPPED :
	break;
	case	EVT_FRAMEI :
RTL_TRDBG(0,"srv echo msg sz=%d '%s'\n",sz,(char *)data);
		rtl_LapPutOutQueue(lk,(u_char *)data,sz);
	break;
	}

	return	0;
}

int	CliLapEventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
	t_ref	*ref	= lk->lk_userptr;

RTL_TRDBG(1,"LAP CB (%s,%s/%s,%s) fd=%d st='%s' receive evttype=%d evtnum='%s'\n",
		lk->lk_addr,lk->lk_port,lk->lk_rhost,lk->lk_rport,lk->lk_fd,
		rtl_LapStateTxt(lk->lk_state),evttype,rtl_LapEventTxt(evtnum));

	switch	(evtnum)
	{
	case	EVT_LK_CREATED :
	break;
	case	EVT_LK_DELETED :
	break;
	case	EVT_LK_STARTED :
	break;
	case	EVT_LK_STOPPED :
	break;
	case	EVT_FRAMEI :
RTL_TRDBG(0,"cli msg sz=%d '%s' %09u\n",sz,(u_char *)data,ref->cnt);
		if	((u_int)atoi(data) != ref->cnt)
		{
			EXIT(1,"wrong count\n");
		}
	break;
	}

	return	0;
}

void	*RunThread(void *pth)
{
	static	unsigned int	nb	= 0;
	time_t	lasttimems	= 0;
	time_t	lasttimesc	= 0;
	time_t	lasttimehr	= 0;
	time_t	now		= 0;
	int			num = (int)pth;
	void			*cliTbPoll;
	struct	list_head	*cliLapList;
	t_xlap_link		cliLapLrc;
	t_ref			cliref;

	char	tmp[128];

	memcpy	(&cliLapLrc,&SrvLapLrc,sizeof(SrvLapLrc));
	strcpy	(cliLapLrc.lk_name,"client");
	cliLapLrc.lk_type	= CliLapFlags;
	cliLapLrc.lk_cbEvent	= CliLapEventProceed;
	cliLapLrc.lk_userptr	= &cliref;

	cliref.num	= num;
	cliref.cnt	= 0;

	sleep	(3);

	RTL_TRDBG(0,"thread %d\n",num);

	cliTbPoll	= rtl_pollInit();
	cliLapList	= rtl_LapInit(0,cliTbPoll);
	rtl_LapAddLink(&cliLapLrc);
	rtl_LapConnectAll();

	while	(1)
	{
		rtl_poll(cliTbPoll,100);	// 100ms
		now	= rtl_tmmsmono();
		if	(abs(now-lasttimems) >= 100)
		{
			int	sz;
			sprintf	(tmp,"%09u",++cliref.cnt);
			sz	= strlen(tmp)+1;
			rtl_LapPutOutQueue(&cliLapLrc,(u_char *)tmp,sz);
			lasttimems	= now;
		}
		if	(abs(now-lasttimesc) >= 1000)
		{
			rtl_LapDoClockSc();
			lasttimesc	= now;
		}
		if	(abs(now-lasttimehr) >= 3600000) {
			lasttimehr	= now;
		}
		nb++;
	}

	pthread_exit(NULL);
	return	NULL;
}

int	main(int argc,char *argv[])
{
	struct sigaction sigact;
	char	*port	= "1234";
	int	i;

	if	(argc > 1)
		port	= argv[1];

	RTL_TRDBG(0,"bind srv '%s'\n",port);

	rtl_init();
	SrvTbPoll	= rtl_pollInit();
	SrvLapList	= rtl_LapInit(0,SrvTbPoll);
	rtl_LapEnableLongMode();

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags		= 0;
	sigact.sa_handler	= SIG_IGN;
	sigaction(SIGHUP,&sigact,NULL);
	sigaction(SIGPIPE,&sigact,NULL);
	sigaction(SIGQUIT,&sigact,NULL);
//	sigaction(SIGINT,&sigact,NULL);

	strcpy	(SrvLapLrc.lk_name,"server");
	SrvLapLrc.lk_addr	= "0.0.0.0";
	SrvLapLrc.lk_port	= port;
	SrvLapLrc.lk_type	= SrvLapFlags;
	SrvLapLrc.lk_cbEvent	= SrvLapEventProceed;

	rtl_LapAddLink(&SrvLapLrc);
	rtl_LapBindAll();



	for	(i = 0 ; i < NB_THREAD ; i++)
	{
		if (pthread_create(&CliThread[i],NULL,RunThread,(void *)i))
		{
			EXIT(1,"cannot create thread\n");
		}
	}


	while	(1)
	{
		rtl_poll(SrvTbPoll,1000);	// 1000ms
		rtl_LapDoClockSc();
	}

	return	0;
}
