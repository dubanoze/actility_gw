
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

u_int		DiscCnt;

// context for poll(2)
void		*TbPoll;
// list of current xlap/links
void		*LapList;
// a link to a server
t_xlap_link	LapLrc;
// flags to create a TCP client of slave type, with auto reconnection and 
// protection to DNS temporary failures (last success entry preserved)
u_int		LapFlags =	
		LK_TCP_CLIENT|LK_SSP_SLAVE|LK_TCP_RECONN|LK_LNK_SAVEDNSENT;

// does the client send or not messages to server
u_int		SendMessage	= 0;

// to illustrate how to add other file descriptors in the poll context
// a basic command line interface is used on stdin
void	Usage()
{
	printf	("q to quit\n");
	printf	("s to start or stop messages\n");
	printf	("#disc=%u\n",DiscCnt);
}

// called for each fd of the poll context before entering poll(2)
// here only stdin has been added via rtl_pollAdd()
int	CB_StdinRequest(void *tb,int fd,void *r1,void *r2,int revents)
{
	if	(fd != fileno(stdin))
		return	0;
	return	POLLIN;
}

// called for each fd of the poll context during poll(2)
// here only stdin has been added via rtl_pollAdd()
int	CB_StdinEvent(void *tb,int fd,void *r1,void *r2,int revents)
{
	int	c;

	if	(fd != fileno(stdin))
		return	0;
	if	((revents & POLLIN) != POLLIN)
		return	1;
	c	= getchar();
	switch	(c)
	{
	default :
	case	'?':
		Usage();
	break;
	case	's' :
		SendMessage	= !SendMessage;
	break;
	case	'q' :
		exit(0);
	break;
	}

	return	1;
}

// called for each xlap/links of the poll context during poll(2)
int	CB_LapEventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
printf("LAP CB (%s,%s/%s,%s) fd=%d st='%s' receive evttype=%d evtnum='%s'\n",
		lk->lk_addr,lk->lk_port,lk->lk_rhost,lk->lk_rport,lk->lk_fd,
		rtl_LapStateTxt(lk->lk_state),evttype,rtl_LapEventTxt(evtnum));

	// keep the link state in the user context if any
	if	(lk->lk_userptr)
	{
		u_int	*st	= (u_int *)lk->lk_userptr;
		*st	 = lk->lk_state;
	}
	switch	(evtnum)
	{
	case	EVT_TCP_CONNECTED :	// tcp connect
	break;
	case	EVT_TCP_DISCONNECTED :	// tcp disconnect
		DiscCnt++;
	break;
	case	EVT_TCP_REJECTED :	// destination can not be reached
	break;
	case	EVT_LK_STARTED :	// xlap/link started/opened
	break;
	case	EVT_LK_STOPPED :	// xlap/link stopped
	break;
	case	EVT_FRAMEI :		// user data (here null terminated)
printf("<%s>\n",(char *)data);
	break;
	}

	return	0;
}

int	main(int argc,char *argv[])
{
	struct sigaction sigact;
	int	ret;
	char	buf[512];
	int	lg;
	u_int	count	= getpid();
	u_int	lkstate	= SSP_STOPPED;

	// create the poll context
	TbPoll	= rtl_pollInit();

	// add stdin in poll context
	rtl_pollAdd(TbPoll,fileno(stdin),CB_StdinEvent,CB_StdinRequest,NULL,NULL);

	// create the xlap/links context using our poll context
	LapList	= rtl_LapInit(0,TbPoll);
	// enter long messages mode
	rtl_LapEnableLongMode();

	// current sig for deamon/tcp usage
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags		= 0;
	sigact.sa_handler	= SIG_IGN;
	sigaction(SIGHUP,&sigact,NULL);
	sigaction(SIGPIPE,&sigact,NULL);
	sigaction(SIGQUIT,&sigact,NULL);
//	sigaction(SIGINT,&sigact,NULL);

	// create our link
	strcpy	(LapLrc.lk_name,"client");
	LapLrc.lk_addr		= "0.0.0.0";
	LapLrc.lk_port		= "1234";
	LapLrc.lk_type		= LapFlags;
	LapLrc.lk_cbEvent	= CB_LapEventProceed;
	LapLrc.lk_userptr	= (void *)&lkstate;
	rtl_LapAddLink(&LapLrc);

	// start all TCP client links
	rtl_LapConnectAll();


	Usage();

	while	(1)
	{
		ret	= rtl_poll(TbPoll,1000);	// 1000ms timeout
		if	(ret == 0 && lkstate == SSP_STARTED && SendMessage)
		{
			sprintf	(buf,"%0500u",count);
			count++;
			lg	= strlen(buf)+1;
			rtl_LapPutOutQueue(&LapLrc,(u_char *)buf,lg);
		}
		// try to call each second the following function to treat
		// timers, reconnection, windows ack, ...
		rtl_LapDoClockSc();
	}

	return	0;
}
