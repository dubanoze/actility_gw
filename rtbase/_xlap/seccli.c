
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

// context for poll(2)
void		*TbPoll;
// list of current xlap/links
void		*LapList;
// link to primary server
t_xlap_link	LapLrcPri;
// link to secondary server
t_xlap_link	LapLrcSec;
// active link (could be NULL when both servers are done)
t_xlap_link	*LapLrc;

// flags to create a TCP client of slave type, with auto reconnection and 
// protection to DNS temporary failures (last success entry preserved)
u_int		LapFlags =	
		LK_TCP_CLIENT|LK_SSP_SLAVE|LK_TCP_RECONN|LK_LNK_SAVEDNSENT;

// does the client send or not messages to server
u_int		SendMessage	= 1;


// to illustrate how to add other file descriptors in the poll context
// a basic command line interface is used on stdin
void	Usage()
{
	printf	("q to quit\n");
	printf	("s to start or stop messages\n");
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

t_xlap_link	*LapSelection(t_xlap_link *pri,t_xlap_link *sec)
{
	if	(pri->lk_state == SSP_STARTED)
		return	pri;
	else
		if	(sec->lk_state == SSP_STARTED)
			return	sec;
	return	NULL;
}

// called for each xlap/links of the poll context during poll(2)
int	CB_LapEventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
#if	0
printf("LAP CB (%s,%s/%s,%s) fd=%d st='%s' receive evttype=%d evtnum='%s'\n",
		lk->lk_addr,lk->lk_port,lk->lk_rhost,lk->lk_rport,lk->lk_fd,
		rtl_LapStateTxt(lk->lk_state),evttype,rtl_LapEventTxt(evtnum));
#endif

	// for a peer(primary,secondary) select one link
	if	(lk == &LapLrcPri || lk == &LapLrcSec)
		LapLrc	= LapSelection(&LapLrcPri,&LapLrcSec);

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
	break;
	case	EVT_TCP_REJECTED :	// destination can not be reached
	break;
	case	EVT_LK_STARTED :	// xlap/link started/opened
	break;
	case	EVT_LK_STOPPED :	// xlap/link stopped
	break;
	case	EVT_FRAME_ACKED :	// message ack received
	{	t_xlap_msg	*msg	= (t_xlap_msg *)data;
		u_short		ns	= (u_short)sz;
		static	u_int 	prev;
		u_int		curr;

		curr	= atoi((char *)msg->m_udata);
printf("ACKED %d sz=%d ns=%d\n",curr,msg->m_usz,ns);	

		if	(prev && (prev+1) != curr)
		{
			printf	("****** prev=%u ! curr=%u\n",prev,curr);
			exit(1);
		}
		prev	= curr;
	}
	break;
	case	EVT_FRAMEI :		// user data (here null terminated)
	{
//printf("<%s>\n",(char *)data);
	}
	break;
	}

	return	0;
}

void	SendBuffer(t_xlap_link *lk)
{
	static	u_int	count	= 0;
	char	buf[512];
	int	lg;
	int	i;
	int	max = 1;

	for	(i = 0 ; i < max ; i++)
	{
		printf	("%s\n",lk->lk_name);
		sprintf	(buf,"%0500u",count);
		count++;
		lg	= strlen(buf)+1;
		rtl_LapPutOutQueue(lk,(u_char *)buf,lg);
	}
}

int	main(int argc,char *argv[])
{
	struct sigaction sigact;
	int	ret;
	u_int	lkstatepri	= SSP_STOPPED;
	u_int	lkstatesec	= SSP_STOPPED;

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

//	LapFlags	= LapFlags | LK_EVT_LOWLEVEL;

	// create primary link
	strcpy	(LapLrcPri.lk_name,"primary-link");
	LapLrcPri.lk_addr		= "0.0.0.0";
	LapLrcPri.lk_port		= "1234";
	LapLrcPri.lk_type		= LapFlags;
	LapLrcPri.lk_cbEvent	= CB_LapEventProceed;
	LapLrcPri.lk_userptr	= (void *)&lkstatepri;
	rtl_LapAddLink(&LapLrcPri);
	// create secondary link
	strcpy	(LapLrcSec.lk_name,"secondary-link");
	LapLrcSec.lk_addr		= "0.0.0.0";
	LapLrcSec.lk_port		= "1235";
	LapLrcSec.lk_type		= LapFlags;
	LapLrcSec.lk_cbEvent	= CB_LapEventProceed;
	LapLrcSec.lk_userptr	= (void *)&lkstatesec;
	rtl_LapAddLink(&LapLrcSec);

	// start all TCP client links
	rtl_LapConnectAll();


	Usage();

	while	(1)
	{
		ret	= rtl_poll(TbPoll,1000);	// 1000ms timeout
		if	(ret == 0 && SendMessage)
		{
			if	(LapLrc)
				SendBuffer(LapLrc);
			else
				printf("no link available\n");
		}
		// try to call each second the following function to treat
		// timers, reconnection, windows ack, ...
		rtl_LapDoClockSc();
	}

	return	0;
}
