
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

int     TraceLevel      = 0;
int     TraceDebug      = 0;

char	Dump[10*1024];

void	*MainTbPoll;

struct	list_head	*LapList;
t_xlap_link		LapLrc;
u_int			LapFlags =	
	LK_TCP_LISTENER|LK_SSP_MASTER|LK_SSP_AUTOSTART;

int	LapEventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
printf("LAP CB (%s,%s/%s,%s) fd=%d st='%s' receive evttype=%d evtnum='%s'\n",
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
		rtl_binToStr((u_char *)data,sz,Dump,sizeof(Dump));
printf("echo msg sz=%d '%s'\n",sz,Dump);
		rtl_LapPutOutQueue(lk,data,sz);
	break;
	}

	return	0;
}

int	main(int argc,char *argv[])
{
	struct sigaction sigact;
	char	*port	= "1234";
	char	*addr	= "0.0.0.0";

	if	(argc > 1)
		port	= argv[1];
	if	(argc > 2)
		addr	= argv[2];

	printf	("bind srv '%s' '%s'\n",addr,port);

	rtl_init();
	MainTbPoll	= rtl_pollInit();

	LapList		= rtl_LapInit(0,MainTbPoll);
	rtl_LapEnableLongMode();

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags		= 0;
	sigact.sa_handler	= SIG_IGN;
	sigaction(SIGHUP,&sigact,NULL);
	sigaction(SIGPIPE,&sigact,NULL);
	sigaction(SIGQUIT,&sigact,NULL);
//	sigaction(SIGINT,&sigact,NULL);

	strcpy	(LapLrc.lk_name,"server");
//	LapLrc.lk_addr		= "172.16.216.1";
//	LapLrc.lk_addr		= "0.0.0.0";
	LapLrc.lk_addr		= addr;
	LapLrc.lk_port		= port;
	LapLrc.lk_type		= LapFlags;
	LapLrc.lk_cbEvent	= LapEventProceed;
	rtl_LapAddLink(&LapLrc);

	rtl_LapBindAll();


	while	(1)
	{
		rtl_poll(MainTbPoll,1000);	// 1000ms
		rtl_LapDoClockSc();
	}

	return	0;
}
