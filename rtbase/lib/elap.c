
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

/*! @file xlap.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>
#ifndef	MACOSX
#include <malloc.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "rtlbase.h"
#include "rtllist.h"
#include "rtlimsg.h"
#include "rtlepoll.h"
#include "rtlelap.h"

#define	WITH_WAITACK_LIST
/*
*/
#define	ABS(x)	((x) > 0 ? (x) : -(x))
#define	STRERRNO	strerror(errno)

#define	XLAP_TRACE	20
//#define	XLAP_TRACE	2

#define	LapSendTESTFRact(lk)	LapSendFrameU((lk),TESTFRact)
#define	LapSendTESTFRcon(lk)	LapSendFrameU((lk),TESTFRcon)
#define	LapSendSTOPDTact(lk)	LapSendFrameU((lk),STOPDTact)
#define	LapSendSTOPDTcon(lk)	LapSendFrameU((lk),STOPDTcon)
#define	LapSendSTARTDTact(lk)	LapSendFrameU((lk),STARTDTact)
#define	LapSendSTARTDTcon(lk)	LapSendFrameU((lk),STARTDTcon)

#define		lk_nbsendB	lk_stat.st_nbsendB
#define		lk_nbsendu	lk_stat.st_nbsendu
#define		lk_nbsends	lk_stat.st_nbsends
#define		lk_nbsendi	lk_stat.st_nbsendi
#define		lk_nbsendw	lk_stat.st_nbsendw

#define		lk_nbrecvB	lk_stat.st_nbrecvB
#define		lk_nbrecvu	lk_stat.st_nbrecvu
#define		lk_nbrecvs	lk_stat.st_nbrecvs
#define		lk_nbrecvi	lk_stat.st_nbrecvi
#define		lk_nbrecvr	lk_stat.st_nbrecvr

#define		lk_nberrnr	lk_stat.st_nberrnr
#define		lk_nberrns	lk_stat.st_nberrns
#define		lk_nbretry	lk_stat.st_nbretry
#define		lk_nbdropi	lk_stat.st_nbdropi

static	int	TraceLevel;
static	int	TraceDebug;
static	int	StartSz = 1;	// number of bytes to start (1)
static	int	ApciSz = 6;	// sz of apci in short mode, +1 for long mode
static	int	LongModeActive	= 0;


/*
static	__thread	void	*TbPoll;
static	__thread	struct	list_head	LkList;
static	__thread	t_xlap_link	*LastLinkFreed;
static	__thread	int	PollRawActive	= 0;
static	__thread	t_xlap_stat	LkStat;
*/

static	__thread	t_xlap_ctxt	LkCtxt;


static	void	IncomingFrame(t_xlap_link *lk);
static	void	LapDisconnected(t_xlap_link *lk,int evt,char *msg);
static	int	EventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz);

static int	_CB_LapEvent(void *tb,int fd,void *r1,void *r2,int revents);
static int	CB_LapEvent(void *tb,int fd,void *r1,void *r2,int revents);
#if	0
static int	CB_LapRequest(void *tb,int fd,void *r1,void *r2,int revents);
#endif

/*
 * serial numbers are commun to all contexts/threads
 */
static	u_int	LapSerial	= 0;
#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
static	int	LapLockSerial	= 0;
#else
static	pthread_mutex_t	LapLockSerial	= PTHREAD_MUTEX_INITIALIZER;
#endif
static	u_int	LapDoSerial()
{
	u_int	ret;
#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_lock_gcc(&LapLockSerial);
#else
	pthread_mutex_lock(&LapLockSerial);
#endif

	LapSerial++;
	if	(LapSerial == 0)
		LapSerial	= 1;
	ret	= LapSerial;

#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_unlock_gcc(&LapLockSerial);
#else
	pthread_mutex_unlock(&LapLockSerial);
#endif
	return	ret;
}

char	*rtl_eLapStateTxt(int state)
{
	static	__thread	char	buf[64];

	switch	(state)
	{
	case	SSP_INIT		:return "SSP_INIT";
	case	SSP_STOPPED		:return "SSP_STOPPED";
	case	SSP_PENDING_STARTED	:return "SSP_PENDING_STARTED";
	case	SSP_STARTED		:return "SSP_STARTED";
	case	SSP_PENDING_STOPPED	:return "SSP_PENDING_STOPPED";
	case	SSP_PENDING_UNCONFD	:return "SSP_PENDING_UNCONFD";
	default :
		sprintf	(buf,"SSP_%d",state);
	break;
	}
	return	buf;
}

char	*rtl_eLapShortStateTxt(int state)
{
	static	__thread	char	buf[64];

	switch	(state)
	{
	case	SSP_INIT		:return "_INIT_";
	case	SSP_STOPPED		:return "STOPPED";
	case	SSP_PENDING_STARTED	:return "PSTARTE";
	case	SSP_STARTED		:return "STARTED";
	case	SSP_PENDING_STOPPED	:return "PSTOPPE";
	case	SSP_PENDING_UNCONFD	:return "PUNCONF";
	default :
		sprintf	(buf,"SSP_%d",state);
	break;
	}
	return	buf;
}

char	*rtl_eLapEventTxt(int token)
{
	static	__thread	char	buf[64];

	switch	(token)
	{
	// internals events
	case	EVT_TCP_ACCEPTED	:return	"EVT_TCP_ACCEPTED";
	case	EVT_TCP_CONNECTED	:return	"EVT_TCP_CONNECTED";
	case	EVT_TCP_CREATED		:return	"EVT_TCP_CREATED";
	case	EVT_TCP_DISCONNECTED	:return	"EVT_TCP_DISCONNECTED";
	case	EVT_TCP_READERROR	:return	"EVT_TCP_READERROR";
	case	EVT_TCP_REJECTED	:return	"EVT_TCP_REJECTED";
	case	EVT_FRAME_FMTERROR	:return	"EVT_FRAME_FMTERROR";
	case	EVT_FRAME_NRERROR	:return	"EVT_FRAME_NRERROR";
	case	EVT_FRAME_NSERROR	:return	"EVT_FRAME_NSERROR";
	case	EVT_FRAME_NOACK		:return	"EVT_FRAME_NOACK";
	case	EVT_FRAME_ACKED		:return	"EVT_FRAME_ACKED";
	// frames U
	case	EVT_TESTFRact		:return	"EVT_TESTFRact";
	case	EVT_TESTFRcon		:return	"EVT_TESTFRcon";
	case	EVT_STOPDTact		:return	"EVT_STOPDTact";
	case	EVT_STOPDTcon		:return	"EVT_STOPDTcon";
	case	EVT_STARTDTact		:return	"EVT_STARTDTact";
	case	EVT_STARTDTcon		:return	"EVT_STARTDTcon";

	case	EVT_FRAMEI		:return	"EVT_FRAMEI";
	case	EVT_FRAMES		:return	"EVT_FRAMES";
	// internals events
	case	EVT_LK_CREATED		:return	"EVT_LK_CREATED";
	case	EVT_LK_DELETED		:return	"EVT_LK_DELETED";
	case	EVT_LK_STARTED		:return	"EVT_LK_STARTED";
	case	EVT_LK_STOPPED		:return	"EVT_LK_STOPPED";
	default :
		sprintf	(buf,"EVT_%d",token);
	break;
	}
	return	buf;
}

static	void	SETFRAME_U(u_char *frame,int type)
{
	int	idxctrl	= StartSz + 1;

	frame[0]	= STARTFRAME;
	frame[1]	= 4;
	frame[idxctrl+0]	= type;
	frame[idxctrl+1]	= 0;
	frame[idxctrl+2]	= 0;
	frame[idxctrl+3]	= 0;
}

static	void	SETFRAME_SEXT(u_char *frame,u_char ext,u_short nr)
{
	int	idxctrl	= StartSz + 1;

	nr		= nr << 1;
	frame[0]	= STARTFRAME;
	frame[1]	= 4;

	frame[idxctrl+0]= 0x01;
	frame[idxctrl+1]= ext;
	frame[idxctrl+2]= nr & 0x00FF;
	frame[idxctrl+3]= nr >> 8;
}

static	void	SETFRAME_S(u_char *frame,u_short nr)
{
	SETFRAME_SEXT(frame,0,nr);
}

static	void	SETFRAME_I(u_char *frame,u_short ns,u_short nr,int lg)
{
	int	idxctrl;

	ns		= ns << 1;
	nr		= nr << 1;

	if	(frame[0] == STARTFRAMELONG)
	{
		idxctrl	= StartSz + 2;	// size length = 2
RTL_TRDBG(1,"SETFRAME_I long msg %d %d\n",(int)frame[1],(int)frame[2]);
	}
	else
	{
		idxctrl	= StartSz + 1;	// size length = 1
	}

	frame[idxctrl+0]= ns & 0x00FF;
	frame[idxctrl+1]= ns >> 8;
	frame[idxctrl+2]= nr & 0x00FF;
	frame[idxctrl+3]= nr >> 8;
}

static	u_int	SETFRAME_ASDU(u_char *frame,u_char *asdu,int lg)
{
	int	idxctrl;

	if (lg >= ACTIVATELONGLENGTH && LongModeActive)
	{
		frame[0]	= STARTFRAMELONG;
		idxctrl	= StartSz + 2;	// size length = 2
		frame[1]	= (lg + ApciSz + 1 - idxctrl) / 256;
		frame[2]	= (lg + ApciSz + 1 - idxctrl) % 256;
RTL_TRDBG(1,"SETFRAME_ASDU long msg %d %d\n",(int)frame[1],(int)frame[2]);
	}
	else
	{
		frame[0]	= STARTFRAME;
		idxctrl	= StartSz + 1;	// size length = 1
		frame[1]	= (lg + ApciSz) - idxctrl;
	}

	frame[idxctrl+0]= 0;
	frame[idxctrl+1]= 0;
	frame[idxctrl+2]= 0;
	frame[idxctrl+3]= 0;

//	memcpy		(frame+ApciSz,asdu,lg);
	memcpy		(frame+idxctrl+4,asdu,lg);

	return	(u_int)(lg + idxctrl + 4);
}

static	u_short GETNS(u_char *frame)
{
	int	idxctrl;
	u_short	ns;

	if (frame[0] == STARTFRAMELONG && LongModeActive)
		idxctrl	= StartSz + 2;
	else	
		idxctrl	= StartSz + 1;
	ns	= frame[idxctrl+0];
	ns	+= (frame[idxctrl+1] << 8);
	ns	= ns >> 1;

	return	ns;
}

static	u_short GETEXT(u_char *frame)
{
	int	idxctrl;
	if (frame[0] == STARTFRAMELONG && LongModeActive)
		idxctrl	= StartSz + 2;
	else	
		idxctrl	= StartSz + 1;
	return	frame[idxctrl+1];
}

static	u_short GETNR(u_char *frame)
{
	int	idxctrl;
	u_short	nr;

	if (frame[0] == STARTFRAMELONG && LongModeActive)
		idxctrl	= StartSz + 2;
	else	
		idxctrl	= StartSz + 1;

	nr	= frame[idxctrl+2];
	nr	+= (frame[idxctrl+3] << 8);
	nr	= nr >> 1;
	return	nr;
}

#if	1
static	int	OKNR(t_xlap_link *lk,u_short nr)
{
return ((nr - lk->lk_va + lk->lk_n) % lk->lk_n <= (lk->lk_vs - lk->lk_va + lk->lk_n) % lk->lk_n);
}
#else
static	int	OKNR(t_xlap_link *lk,u_short nr)
{
	u_short va = lk->lk_va;
	u_short vs = lk->lk_vs;

	while (va != vs) 
	{
		if (nr == va)
			return 1;
		va = (va + 1) % lk->lk_n;
	}

	return nr == vs;
}
#endif

static	int	OKNS(t_xlap_link *lk,u_short ns)
{
return (ns == lk->lk_vr);
}

static	int	LapSendFrameU(t_xlap_link *lk,int type)
{
	t_xlap_ctxt	*pctxt;
	u_char	frame[FRAME_MIN];
	int	ret;
	char	*lbl;

	if	(!lk)
		return	-1;
	pctxt	= lk->lk_ctxt;
	if	(lk->lk_conn == 0)
	{
RTL_TRDBG(0,"cannot send FrameU TCP not connected on RTU(%s,%s) fd=%d t=%02x\n",
		lk->lk_addr,lk->lk_port,lk->lk_fd,type);
		return	-2;
	}

	switch	(type)
	{
	case	TESTFRact :	lbl	= "TESTFRact"; break;
	case	TESTFRcon :	lbl	= "TESTFRcon"; break;
	case	STOPDTact :	lbl	= "STOPDTact"; break;
	case	STOPDTcon :	lbl	= "STOPDTcon"; break;
	case	STARTDTact :	lbl	= "STARTDTact"; break;
	case	STARTDTcon :	lbl	= "STARTDTcon"; break;
	default :
RTL_TRDBG(0,"error type FrameU on RTU(%s,%s) fd=%d t=%02x\n", 
		lk->lk_addr,lk->lk_port,lk->lk_fd,type);
		return	-3;
	break;
	}

	switch	(type)	// start T1 for frame U
	{
	case	TESTFRact :
		if	(lk->lk_tFrmSndAt == 0)
		{
			lk->lk_tFrmSndAt	= rtl_timemono(NULL);
			lk->lk_tFrmSndAtMs	= rtl_tmmsmono();
		}
	case	STOPDTact :
	case	STARTDTact :
		if	(lk->lk_uFrmSndAt == 0)
			lk->lk_uFrmSndAt	= rtl_timemono(NULL);
	default :
	break;
	}

	switch	(type)	// stop T1 for frame U
	{
	case	TESTFRcon :
	case	STOPDTcon :
	case	STARTDTcon :
		lk->lk_uFrmSndAt	= 0;
	default :
	break;
	}

	SETFRAME_U(frame,type);

RTL_TRDBG(2,"send FRAME_U '%s' on RTU(%s,%s) fd=%d t=%02x\n",lbl,
		lk->lk_addr,lk->lk_port,lk->lk_fd,type);

	lk->lk_nbsendu++;
	lk->lk_nbsendB += ApciSz;
	pctxt->ct_lkstat.st_nbsendu++;
	pctxt->ct_lkstat.st_nbsendB += ApciSz;
	ret	= write(lk->lk_fd,frame,ApciSz);
	if	(ret != ApciSz)
	{
RTL_TRDBG(0,"error write FrameU on RTU(%s,%s) fd=%d t=%02x '%s'\n", 
		lk->lk_addr,lk->lk_port,lk->lk_fd,type,STRERRNO);
		return	-10;
	}
	lk->lk_nbsendw++;
	pctxt->ct_lkstat.st_nbsendw++;
	return	ret;
}

static	int	LapSendFrameS(t_xlap_link *lk,int precvnumber)
{
	t_xlap_ctxt	*pctxt;
	u_short	recvnumber = (u_short)precvnumber;
	u_char	frame[FRAME_MIN];
	int	ret;

	if	(!lk || lk->lk_fd < 0)
		return	-1;
	pctxt	= lk->lk_ctxt;
	if	(lk->lk_conn == 0)
	{
RTL_TRDBG(0,"cannot send FrameS TCP not connected on RTU(%s,%s) fd=%d\n",
		lk->lk_addr,lk->lk_port,lk->lk_fd);
		return	-2;
	}

RTL_TRDBG(2,"send FRAME_S rcv=%d on RTU(%s,%s) fd=%d\n",recvnumber,
		lk->lk_addr,lk->lk_port,lk->lk_fd);

	SETFRAME_S(frame,recvnumber);


	lk->lk_vapeer	= precvnumber;
	lk->lk_nbsends++;
	lk->lk_nbsendB += ApciSz;
	pctxt->ct_lkstat.st_nbsends++;
	pctxt->ct_lkstat.st_nbsendB += ApciSz;
	ret	= write(lk->lk_fd,frame,ApciSz);
	if	(ret != ApciSz)
	{
RTL_TRDBG(0,"error write FrameS on RTU(%s,%s) fd=%d '%s'\n", 
		lk->lk_addr,lk->lk_port,lk->lk_fd,STRERRNO);
		return	-10;
	}
	lk->lk_nbsendw++;
	pctxt->ct_lkstat.st_nbsendw++;

	return	ret;
}

static	int	LapSendFrameI(t_xlap_link *lk,u_char *frame,int lg)
{
	t_xlap_ctxt	*pctxt;
	int	ret;
	u_short	ns;
	u_short	nr;

	if	(!lk || lk->lk_fd < 0)
		return	-1;
	pctxt	= lk->lk_ctxt;
	if	(lk->lk_conn == 0)
	{
RTL_TRDBG(0,"cannot send FrameI TCP not connected on RTU(%s,%s) fd=%d\n",
		lk->lk_addr,lk->lk_port,lk->lk_fd);
		return	-2;
	}
	if	(lk->lk_vs == (lk->lk_va + lk->lk_k) % lk->lk_n)
	{
RTL_TRDBG(0,"cannot send FrameI window limit on RTU(%s,%s) fd=%d vs=%d va=%d\n",
		lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_vs,lk->lk_va);
		return	0;
	}

	ns	= lk->lk_vs;
	nr	= lk->lk_vr;

RTL_TRDBG(2,"send FRAME_I(ns=%d,nr=%d) on RTU(%s,%s) fd=%d sz=%d\n",ns,nr,
		lk->lk_addr,lk->lk_port,lk->lk_fd,lg);

	SETFRAME_I(frame,ns,nr,lg);
	lk->lk_nbsendi++;
	lk->lk_nbsendB += lg;
	pctxt->ct_lkstat.st_nbsendi++;
	pctxt->ct_lkstat.st_nbsendB += ApciSz;

	if	(0 && rand()%100 == 0)	// TEST ONLY
	{
RTL_TRDBG(0,"drop for test FrameI on RTU(%s,%s) fd=%d\n", 
		lk->lk_addr,lk->lk_port,lk->lk_fd);
		return	1;
	}
	if	(0 && rand()%10 == 0)	// TEST ONLY
	{
RTL_TRDBG(0,"dup for test FrameI on RTU(%s,%s) fd=%d\n", 
		lk->lk_addr,lk->lk_port,lk->lk_fd);
		ret	= write(lk->lk_fd,frame,lg);
	}
	if	(0)			// TEST ONLY
	{
		int	i;

RTL_TRDBG(0,"write byte/byte for test FrameI on RTU(%s,%s) fd=%d\n", 
		lk->lk_addr,lk->lk_port,lk->lk_fd);
		for	(i = 0 ; i < lg ; i++)
		{
			ret	= write(lk->lk_fd,&frame[i],1);
		}
		return	lg;
	}
	ret	= write(lk->lk_fd,frame,lg);
	if	(ret != lg)
	{
RTL_TRDBG(0,"error write FrameI on RTU(%s,%s) fd=%d ret=%d lg=%d '%s'\n", 
		lk->lk_addr,lk->lk_port,lk->lk_fd,ret,lg,STRERRNO);
		return	-10;
	}
	lk->lk_nbsendw++;
	pctxt->ct_lkstat.st_nbsendw++;
	return	ret;
}

static	void	LapSetLink(t_xlap_ctxt *pctxt,t_xlap_link *lk)
{
	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	lk->lk_ctxt	= pctxt;
	lk->lk_magic	= RTL_ELAP_LINK_MAGIC;
	INIT_LIST_HEAD(&lk->lk_outqueue.list);
	INIT_LIST_HEAD(&lk->lk_ackqueue.list);
	INIT_LIST_HEAD(&lk->lk_clilist);
}

static	void	AddWaitAckList(t_xlap_link *lk)
{
#ifdef	WITH_WAITACK_LIST
	t_xlap_ctxt	*pctxt	= lk->lk_ctxt;
	if	(lk->lk_inwaitacklist == 0 && lk->lk_ackcount > 0)
	{
		list_add_tail(&lk->waitacklist,&pctxt->ct_waitacklist);
		lk->lk_inwaitacklist	= 1;
	}
#endif
}

static	void	DelWaitAckList(t_xlap_link *lk,int nocond)
{
#ifdef	WITH_WAITACK_LIST
	if	(lk->lk_inwaitacklist && (nocond || lk->lk_ackcount <= 0))
	{
		list_del(&lk->waitacklist);
		lk->lk_inwaitacklist	= 0;
	}
#endif
}

static	void	LapResetLink(t_xlap_link *lk,int full)
{
	int	nbout	= 0;
	int	nback	= 0;

	lk->lk_state	= SSP_INIT;
	lk->lk_fd	= -1;
	lk->lk_serial	= 0;

	lk->lk_conn	= 0;
	lk->lk_prog	= 0;
	lk->lk_progAt	= 0;
	lk->lk_va	= 0;
	lk->lk_vr	= 0;
	lk->lk_vs	= 0;
	lk->lk_vapeer	= 0;

	lk->lk_xFrmRcvAt	= 0;
	lk->lk_iFrmRcvAt	= 0;
	lk->lk_uFrmSndAt	= 0;
	lk->lk_tFrmSndAt	= 0;
	lk->lk_tFrmSndAtMs	= 0;

	if	(lk->lk_t0 == 0) lk->lk_t0	= DEFAULT_T0;
	if	(lk->lk_t1 == 0) lk->lk_t1	= DEFAULT_T1;
	if	(lk->lk_t2 == 0) lk->lk_t2	= DEFAULT_T2;
	if	(lk->lk_t3 == 0) lk->lk_t3	= DEFAULT_T3;
	if	(lk->lk_w == 0) lk->lk_w	= DEFAULT_W;
	if	(lk->lk_k == 0) lk->lk_k	= DEFAULT_K;
	if	(lk->lk_n == 0) lk->lk_n	= OP_MOD;

	if	(lk->lk_rhost)
	{
		free	(lk->lk_rhost);
		lk->lk_rhost	= NULL;
	}
	if	(lk->lk_rport)
	{
		free	(lk->lk_rport);
		lk->lk_rport	= NULL;
	}
	nback	= 0;
	DelWaitAckList(lk,1);
	while	( !list_empty(&lk->lk_ackqueue.list) ) 
	{
		t_xlap_msg *msg;

		msg	= list_entry(lk->lk_ackqueue.list.next,t_xlap_msg,list);
		list_del(&msg->list);
		lk->lk_ackcount--;
		if	(msg->m_data)
			free(msg->m_data);
		free(msg);
		nback++;
	}
	lk->lk_ackcount	= 0;
	RTL_TRDBG(XLAP_TRACE,"Reset %s lk_ackcount=%d\n", lk->lk_addr, lk->lk_ackcount);

	if	(!full)
	{
	RTL_TRDBG(1,"Lap reset partial on RTU(%p,%s,%s) outq=%d ackq=%d\n",
			lk,lk->lk_addr,lk->lk_port,lk->lk_outcount,nback);
		return;
	}
	lk->lk_listener	= NULL;

	memset	(&lk->lk_stat,0,sizeof(lk->lk_stat));

	nbout	= 0;
	while	( !list_empty(&lk->lk_outqueue.list) ) 
	{
		t_xlap_msg *msg;

		msg	= list_entry(lk->lk_outqueue.list.next,t_xlap_msg,list);
		list_del(&msg->list);
		lk->lk_outcount--;
		if	(msg->m_data)
			free(msg->m_data);
		free(msg);
		nbout++;
	}
	lk->lk_outcount	= 0;
	lk->lk_outdrop	= 0;

	// like DISC => POLLOUT to reconnect
	rtl_epollSetEvt2(LkCtxt.ct_tbpoll,lk->lk_fdpos,POLLOUT);

	RTL_TRDBG(1,"Lap reset full on RTU(%p,%s,%s) outq=%d ackq=%d\n",
			lk,lk->lk_addr,lk->lk_port,nbout,nback);

}

void	rtl_eLapDisableLongMode()
{
	LongModeActive = 0;
}

void	rtl_eLapEnableLongMode()
{
	LongModeActive = 1;
}

void	rtl_eLapAddLink(t_xlap_ctxt *pctxt,t_xlap_link *lk)
{
	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	LapSetLink(pctxt,lk);
	LapResetLink(lk,1);
	list_add_tail(&lk->list,&pctxt->ct_fulllist);
}

void	*rtl_eLapAddLinkBind(t_xlap_ctxt *pctxt,t_xlap_link *lk)
{
	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	if	(!IS_TCP_LISTENER(lk))
		return	NULL;
	LapSetLink(pctxt,lk);
	LapResetLink(lk,1);
	list_add_tail(&lk->list,&pctxt->ct_fulllist);
	return	&lk->lk_clilist;
}


void	*rtl_eLapInit(u_int flags,void *tbpoll)
{
	t_xlap_ctxt	*pctxt	= &LkCtxt;
	pctxt->ct_tbpoll	= tbpoll;
	pctxt->ct_flags		= flags;
	INIT_LIST_HEAD(&pctxt->ct_fulllist);
	INIT_LIST_HEAD(&pctxt->ct_waitacklist);
	INIT_LIST_HEAD(&pctxt->ct_extqueue.list);

#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
	pctxt->ct_extlock	= 0;
#else
	pthread_mutex_init(&pctxt->ct_extlock,NULL);
#endif

	if	((flags & LAP_LONG_FRAME) == LAP_LONG_FRAME)
		LongModeActive	= 1;	// common to all contexts/threads
	return	&pctxt->ct_fulllist;
}

static	void	LapRunAckQueue(t_xlap_link *lk,u_short nr)
{
	int	nb;
	u_char	*frame;
	u_short	ns;

	nb	= 0;
	while	( !list_empty(&lk->lk_ackqueue.list))
	{
		t_xlap_msg	*msg;

		msg	= list_entry(lk->lk_ackqueue.list.next,t_xlap_msg,list);
		frame	= msg->m_data;
		ns	= GETNS(frame);

		if	( ns == nr)
			break;

		if	(IS_EVT_LOWLEVEL(lk))
			EventProceed(lk,EVT_APIONLY,EVT_FRAME_ACKED,msg,(int)ns);
		list_del(&msg->list);
		lk->lk_ackcount--;
		if	(msg->m_data)
			free(msg->m_data);
		free(msg);
		nb++;
	}
	DelWaitAckList(lk,0);
	if	(nb > 0)
	RTL_TRDBG(XLAP_TRACE,"remove %d msg from ackqueue(%d) on RTU(%p,%s,%s)\n",
		nb,lk->lk_ackcount,lk,lk->lk_addr,lk->lk_port);
}

static	void	LapTimerAckQueue(t_xlap_link *lk,time_t now,int force)
{
	int	nb;
	t_xlap_msg	*msg;
	t_xlap_msg	*tmp;

	if	(!IS_TCP_CLIENT(lk))
		return;

	if	(!IS_TCP_SERVER(lk))
		return;

	lk->lk_outbusy	= 0;
	nb	= 0;

//RTL_TRDBG(1,"LapTimerAckQueue %s lk_ackcount=%d\n", lk->lk_addr, lk->lk_ackcount);

	list_for_each_entry_safe(msg,tmp,&lk->lk_ackqueue.list,list) 
	{
//RTL_TRDBG(1,"	 check %ld >= %ld\n", ABS(now - msg->m_time), lk->lk_t1);
		if	((ABS(now - msg->m_time) >= lk->lk_t1)) {
			u_char *frame	= msg->m_data;
			u_short ns	= GETNS(frame);
			RTL_TRDBG (1, "msg NS=%d timed out\n", ns);
			nb++;
		}
	}
	if	(nb > 0)
	{
	RTL_TRDBG(0,"non ack msg %d in ackqueue(%d) on RTU(%p,%s,%s)\n",
		nb,lk->lk_ackcount,lk,lk->lk_addr,lk->lk_port);
		LapDisconnected(lk,EVT_FRAME_NOACK,"no ack");
	}
	return;
}

static	void	LapTimerAckQueueAll(t_xlap_ctxt *pctxt,time_t now)
{
	t_xlap_link	*lk;
	t_xlap_link	*tmp;
	int		force;

#ifdef	WITH_WAITACK_LIST
	list_for_each_entry_safe(lk,tmp,&pctxt->ct_waitacklist,waitacklist) 
#else
	list_for_each_entry_safe(lk,tmp,&pctxt->ct_fulllist,list) 
#endif
	{
		LapTimerAckQueue(lk,now,force=0);
	}
}

static	void	LapRunOutQueue(t_xlap_link *lk)
{
	t_xlap_ctxt	*pctxt;
	int	nb;
	int	ret;
	//u_short	start;
	u_short	end;

	if	(lk->lk_outbusy)
		return;
	if	(lk->lk_state != SSP_STARTED)
		return;

	pctxt	= lk->lk_ctxt;

	end	= (lk->lk_va + lk->lk_k) % lk->lk_n;
	if	(list_empty(&lk->lk_ackqueue.list))
	{	// in case of lost frames, readjust lk_vs
		lk->lk_vs	= lk->lk_va;
	}


	nb	= 0;
	while	(!list_empty(&lk->lk_outqueue.list) && (lk->lk_vs != end))
	{
		t_xlap_msg	*msg;

		msg	= list_entry(lk->lk_outqueue.list.next,t_xlap_msg,list);
		ret	= LapSendFrameI(lk,msg->m_data,msg->m_sz);
		if	(ret <= 0)
			break;
		list_del(&msg->list);
		lk->lk_outcount--;
		list_add_tail(&msg->list,&lk->lk_ackqueue.list);
		lk->lk_ackcount++;
		msg->m_time	= rtl_timemono(NULL);
		lk->lk_vs	= (lk->lk_vs + 1) % lk->lk_n;
		nb++;
	}
	AddWaitAckList(lk);
	if	(nb > 0)
	RTL_TRDBG(XLAP_TRACE,"pickup %d msg from outqueue(%d) on RTU(%p,%s,%s)\n",
		nb,lk->lk_outcount,lk,lk->lk_addr,lk->lk_port);

	if	(lk->lk_outcount > 0)
	{
		// more to write => POLLIN|POLLOUT
		rtl_epollSetEvt2(pctxt->ct_tbpoll,lk->lk_fdpos,POLLIN|POLLOUT);
	}
	else
	{
		rtl_epollSetEvt2(pctxt->ct_tbpoll,lk->lk_fdpos,POLLIN);
	}
}

static	void	LapDisconnected(t_xlap_link *lk,int evt,char *msg)
{
	t_xlap_ctxt	*pctxt;

	if	(!lk)
		return;

	pctxt	= lk->lk_ctxt;

	RTL_TRDBG(1,"TCP Disconnected on RTU(%p,%s,%s) fd=%d conn=%d '%s'\n",
		lk,lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_conn,msg);

	EventProceed(lk,EVT_INDICAT,evt,NULL,0);
	EventProceed(lk,EVT_APIONLY,EVT_LK_DELETED,NULL,0);

	if	(lk->lk_fd >= 0)
	{
		shutdown(lk->lk_fd,2);
		rtl_epollRmv(pctxt->ct_tbpoll,lk->lk_fd);
		close(lk->lk_fd);
		lk->lk_fd	= -1;
		lk->lk_serial	= 0;
	}

	if	(lk->lk_rframe)
	{
		free(lk->lk_rframe);
		lk->lk_rframe	= NULL;
		lk->lk_ralloc	= 0;
	}


	if	(IS_TCP_SERVER(lk))
	{
		if	(lk->lk_listener)
		{
			lk->lk_listener->lk_cnxcount--;
		}
	RTL_TRDBG(1,"server link lk=%p uptr=%p => remove from list and free\n",
			lk,lk->lk_userptr);
		list_del(&lk->clilist);
		list_del(&lk->list);
		LapResetLink(lk,1);
		lk->lk_magic	= 0;
		lk->lk_serial	= 0;
		free(lk);
		pctxt->ct_lastlinkfreed	= lk;
		return;
	}

	if	(IS_LNK_NOFULLRESET(lk))// no full reset
		LapResetLink(lk,0);		// do partial reset => keep outq
	else				// full reset (by default now)
		LapResetLink(lk,1);		// do full reset => clear outq
}

void	rtl_eLapDisc(t_xlap_link *lk,char *msg)
{
	LapDisconnected(lk,EVT_LK_DELETED,msg);
}

static	int	LapReallocFrame(t_xlap_link *lk)
{
	int	newsize;
	void	*newptr;

	newsize	= lk->lk_ralloc + TCP_READ_SIZE;
	newptr	= (u_char *)realloc(lk->lk_rframe,newsize);
	if	(!newptr)
	{
		RTL_TRDBG(0,"cannot connect RTU (%s,%s)\n",
						lk->lk_addr,lk->lk_port);
		return	-1;
	}
	lk->lk_rframe	= (u_char *)newptr;
	lk->lk_ralloc	= newsize;

	RTL_TRDBG(XLAP_TRACE,"RTU (%s,%s) frame buffer size is now ralloc=%d\n",
					lk->lk_addr,lk->lk_port,lk->lk_ralloc);

	return	lk->lk_ralloc;
}

static	int	SearchFrame(t_xlap_link *lk)
{
	t_xlap_ctxt	*pctxt;
	int	ret	= -1;
	int	idxctrl;
	u_char	*rframe;

	if	(lk->lk_rcnt <= 0)
	{
		lk->lk_rcnt	= 0;
		lk->lk_rwait	= 0;
		return	0;
	}

	pctxt	= lk->lk_ctxt;
	rframe	= lk->lk_rframe+lk->lk_rpos;

	if	(rframe[0] != STARTFRAME && (rframe[0] != STARTFRAMELONG || !LongModeActive))
	{
		if (LongModeActive)
		{
RTL_TRDBG(0,"%02x or %02x expected on RTU(%s,%s) fd=%d rsz=%d rcnt=%d rpos=%d\n",
			STARTFRAME,STARTFRAMELONG,lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_rsz,lk->lk_rcnt,lk->lk_rpos);
		}
		else
		{
RTL_TRDBG(0,"%02x expected on RTU(%s,%s) fd=%d rsz=%d rcnt=%d rpos=%d\n",
			STARTFRAME,lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_rsz,lk->lk_rcnt,lk->lk_rpos);
		}
		return	-1;
	}
	if	(rframe[0] == STARTFRAME)
	{
		idxctrl	= StartSz + 1;	// size length = 1
		if	(lk->lk_rwait <= 0 && lk->lk_rcnt >= 1)
		{
			lk->lk_rwait	= rframe[1]+idxctrl;
RTL_TRDBG(XLAP_TRACE,"waiting %d bytes on RTU(%s,%s) fd=%d rwait=%d\n",
		lk->lk_rwait,lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_rwait);
		}
	}
	else
	{
		idxctrl	= StartSz + 2;	// size length = 2
		if	(lk->lk_rwait <= 0 && lk->lk_rcnt >= 2)
		{
			lk->lk_rwait	= rframe[1]*256;
			lk->lk_rwait	+= rframe[2]+idxctrl;
RTL_TRDBG(XLAP_TRACE,"(long) waiting %d bytes on RTU(%s,%s) fd=%d rwait=%d\n",
		lk->lk_rwait,lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_rwait);
RTL_TRDBG(XLAP_TRACE,"   rframe[1]=%d, rframe[2]=%d, idxctrl=%d, sz=%d\n",
		rframe[1], rframe[2], idxctrl, rframe[1]*256 + rframe[2]);
		}
	}

	if	(lk->lk_rwait && lk->lk_rcnt > idxctrl && lk->lk_rcnt >= lk->lk_rwait)
	{
		ret		= lk->lk_rwait;
		lk->lk_rcnt	= lk->lk_rwait;
		IncomingFrame(lk);
		if	(lk != pctxt->ct_lastlinkfreed)
		{
			lk->lk_rcnt	= 0;
			lk->lk_rwait	= 0;
		}
		return	ret;
	}

RTL_TRDBG(XLAP_TRACE,"uncomplete frame on RTU(%s,%s) fd=%d rcnt=%d rwait=%d\n",
		lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_rcnt,lk->lk_rwait);

	return	0;

}

static	int	ReadSocket(t_xlap_link *lk)
{
	t_xlap_ctxt	*pctxt;
	u_char	*rbuf;
	int	ret;
	int	pos;
	int	sz;
	int	nbframe = 0;
	

	pctxt	= lk->lk_ctxt;
	sz	= lk->lk_rsz + TCP_READ_SIZE;
	if	(sz > lk->lk_ralloc || !lk->lk_rframe || lk->lk_ralloc <= 0)
	{
		// TODO limit ralloc to FRAME_MAX
		if	(LapReallocFrame(lk) <= 0)
		{
			LapDisconnected(lk,EVT_FRAME_FMTERROR,"alloc frame");
			return	-1;
		}
	}

	sz	= TCP_READ_SIZE;
	rbuf	= lk->lk_rframe;

	ret	= read(lk->lk_fd,rbuf+lk->lk_rsz,sz);
RTL_TRDBG(XLAP_TRACE,"something to read on RTU(%s,%s) fd=%d sz=%d\n",
		lk->lk_addr,lk->lk_port,lk->lk_fd,ret);
	if	(ret == 0)
	{ 	// TCP disconnected
		LapDisconnected(lk,EVT_TCP_DISCONNECTED,"connection closed (eot)");
		return	-1;
	}
	if	(ret < 0)
	{	// TCP error
		LapDisconnected(lk,EVT_TCP_READERROR,"connection closed (read error)");
		return	-1;
	}
	lk->lk_nbrecvr++;
	pctxt->ct_lkstat.st_nbrecvr++;
	if	(0)
	{
		FILE	*f;
		int	i;
		char	fn[256];
		static	int myidx=0;
		sprintf(fn, "/tmp/xlapin%d", (myidx++)%256);
		f = fopen(fn, "w");
		if (f)
		{
			for (i=0; i<ret; i++)
			{
				fprintf(f, "%02x ", *(rbuf+lk->lk_rsz+i));
				if (((i+1)%25) == 0)
					fprintf(f,"\n");
			}
			fprintf(f,"\n");
			RTL_TRDBG(0,"%s\n", fn);
			fclose(f);
		}
	}
	lk->lk_rsz	+= ret;
	if	(lk->lk_rsz < ApciSz)
		return	0;
	pos	= 0;
	while	(lk->lk_rsz > 0)
	{
		lk->lk_rcnt	= lk->lk_rsz;
		lk->lk_rpos	= pos;
RTL_TRDBG(XLAP_TRACE,"SearchFrame ralloc=%d rsz=%d rcnt=%d rwait=%d pos=%d fr=%p\n",
		lk->lk_ralloc,lk->lk_rsz,lk->lk_rcnt,lk->lk_rwait,pos,
		lk->lk_rframe);
		pctxt->ct_lastlinkfreed	= NULL;
		ret	= SearchFrame(lk);
		if	(lk == pctxt->ct_lastlinkfreed)	// TODO moche
		{
RTL_TRDBG(XLAP_TRACE,"SearchFrame ret=<link freed>\n");
			return	-1;
		}
RTL_TRDBG(XLAP_TRACE,"SearchFrame ret=%d\n",ret);
		if	(ret < 0)
		{	// error
			LapDisconnected(lk,EVT_FRAME_FMTERROR,
						"build frame error 1");
			return	-1;
		}
		if	(ret == 0)
		{	// not enough bytes
			if	(lk->lk_rsz >= FRAME_MAX)
			{
				LapDisconnected(lk,EVT_FRAME_FMTERROR,
						"build frame error 2");
				return	-1;
			}
			if	(lk->lk_rwait > 0 && lk->lk_rcnt > 0)
			{
				u_char	tmp[FRAME_MAX];

RTL_TRDBG(XLAP_TRACE,"keep bytes left=%d from pos=%d\n",lk->lk_rcnt,lk->lk_rpos);
				memcpy	(tmp,lk->lk_rframe+lk->lk_rpos,lk->lk_rcnt);
				memcpy	(lk->lk_rframe,tmp,lk->lk_rcnt);

				lk->lk_rsz	= lk->lk_rcnt;
				lk->lk_rpos	= 0;
				lk->lk_rcnt	= 0;
				lk->lk_rwait	= 0;
			}
			break;
		}
		if	(ret > 0)
		{	// a frame was treated, ret == bytes consumed
			pos		+= ret;
			lk->lk_rsz	-= ret;
			lk->lk_rcnt	= 0;
			lk->lk_rwait	= 0;
			nbframe++;
		}
	}
if	(nbframe > 1)
RTL_TRDBG(XLAP_TRACE,"ReadSocket same loop nbframe=%d\n",nbframe);

	return	0;
}

static int	_CB_LapEvent(void *tb,int fd,void *r1,void *r2,int revents)
{
	t_xlap_link	*lk	= (t_xlap_link *)r1;

RTL_TRDBG(XLAP_TRACE,"CB_LapEvent(%p,%s,%s) fd=%d prog=%d conn=%d events=%d\n",
	lk,lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_prog,lk->lk_conn,revents);

/*
printf("CB_LapEvent(%p,%s,%s) fd=%d prog=%d conn=%d events=%d\n",
	lk,lk->lk_addr,lk->lk_port,lk->lk_fd,lk->lk_prog,lk->lk_conn,revents);
*/


	if	(IS_TCP_SERVER(lk) && lk->lk_tobedisc) 
	{
		LapDisconnected(lk,EVT_LK_DELETED,"listener closed");
		return	0;
	}

	if	(IS_TCP_LISTENER(lk))
	{
		if	((revents & POLLIN) == POLLIN)
		{
			int		fdnew;
			struct sockaddr from;
			socklen_t	lenaddr;
			char		host[NI_MAXHOST];
			char		port[NI_MAXSERV];
			int		flags;

			t_xlap_link	*server;

			flags	= NI_NUMERICHOST | NI_NUMERICSERV;
			lenaddr	= sizeof(from) ;
			fdnew	= accept(fd,&from,&lenaddr);
			if	(fdnew < 0)
				return	0;

			if	(lk->lk_maxConn && lk->lk_cnxcount == lk->lk_maxConn) {
				RTL_TRDBG(0,"New connexion attempt but maxConn reached (maxConn=%d)\n", lk->lk_maxConn);
				shutdown(fdnew,2);
				close (fdnew);
				return 0;
			}

			getnameinfo(&from,lenaddr,host,sizeof(host), port,sizeof(port),flags);
			if	(lk->lk_authList && *lk->lk_authList) {
				if	(!strstr(host, lk->lk_authList)) {
					RTL_TRDBG(0,"New connexion attempt from %s:%s not in authList\n", host, port);
					shutdown(fdnew,2);
					close (fdnew);
					return 0;
				}
			}

			{
				int	nodelay	= 1;
				setsockopt(fdnew,IPPROTO_TCP,TCP_NODELAY,
				(char *)&nodelay,sizeof(nodelay));
			}
			{
				int	quickack	= 1;
				setsockopt(fdnew,IPPROTO_TCP,TCP_QUICKACK,
				(char *)&quickack,sizeof(quickack));
			}

			if	(IS_TCP_NONBLOCK(lk))
			{
				u_long	flags;
				flags = fcntl(fdnew, F_GETFL);
				if (fcntl(fdnew,F_SETFL,O_NONBLOCK|flags)<0)
				{
		RTL_TRDBG(0,"cannot fcntl on new socket (%s)\n",STRERRNO);
					close(fdnew);
					return	0;
				}
			}

			server	= (t_xlap_link *)malloc(sizeof(t_xlap_link));
			if	(!server)
			{
				close(fdnew);
				return	0;
			}
			memset(server,0,sizeof(t_xlap_link));
			LapSetLink(lk->lk_ctxt,server);
			LapResetLink(server,1);
			server->lk_state	= SSP_INIT;
			server->lk_fd		= fdnew;
			server->lk_addr		= lk->lk_addr;
			server->lk_port		= lk->lk_port;
			server->lk_t0		= lk->lk_t0;
			server->lk_t1		= lk->lk_t1;
			server->lk_t2		= lk->lk_t2;
			server->lk_t3		= lk->lk_t3;
			server->lk_k		= lk->lk_k;
			server->lk_w		= lk->lk_w;
			server->lk_n		= lk->lk_n;
			server->lk_conn		= 1;
			server->lk_rhost	= strdup(host);
			server->lk_rport	= strdup(port);
			server->lk_listener	= lk;
			server->lk_cbEvent	= lk->lk_cbEvent;

			sprintf	(server->lk_name,"%s_%p",lk->lk_name,server);

			server->lk_type		= lk->lk_type;
			server->lk_type		&= ~LK_TCP_LISTENER;
			server->lk_type		|= LK_TCP_SERVER;
			if	(!IS_SSP_MASTER(server))
				if	(!IS_SSP_SLAVE(server))
					server->lk_type	 |= LK_SSP_SLAVE;

			lk->lk_cnxcount++;
			lk->lk_callcount++;
			list_add_tail(&server->clilist,&lk->lk_clilist);
	
			list_add_tail(&server->list,&LkCtxt.ct_fulllist);

			server->lk_fdpos = rtl_epollAdd(LkCtxt.ct_tbpoll,fdnew,
			CB_LapEvent,NULL,server,(void *)RTL_ELAP_LINK_MAGIC);
			server->lk_serial= rtl_epollSerial(LkCtxt.ct_tbpoll,fdnew);


	RTL_TRDBG(1,"'[%s]:%s' accepted on RTU(%s,%s) fd=%d\n",host,port,
				server->lk_addr,server->lk_port,server->lk_fd);

			// server TCP default is POLLIN
			rtl_epollSetEvt2(LkCtxt.ct_tbpoll,server->lk_fdpos,POLLIN);

			EventProceed(lk,EVT_INDICAT,EVT_TCP_ACCEPTED,NULL,0);
			EventProceed(server,EVT_INDICAT,EVT_TCP_CREATED,NULL,0);
			EventProceed(server,EVT_APIONLY,EVT_LK_CREATED,NULL,0);

		}
		return	0;
	}

	// special case where socket is not yet connected
	if	(IS_TCP_CLIENT(lk) && lk->lk_conn == 0)
	{
		if	((revents & POLLOUT) == POLLOUT)
		{
			int	err = 1;
			socklen_t optlen = sizeof(err);

			getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&optlen);
			if	(!IS_TCP_NONBLOCK(lk))
			{	// reset non blocking mode
				fcntl(fd,F_SETFL,~O_NONBLOCK,0);
			}
			if	(err == 0)
			{
	RTL_TRDBG(1,"connect accepted on RTU(%p,%s,%s) fd=%d\n",
				lk,lk->lk_addr,lk->lk_port,fd);
				lk->lk_conn	= 1;
				// client TCP default is POLLIN
				rtl_epollSetEvt2(LkCtxt.ct_tbpoll,lk->lk_fdpos,POLLIN);
				EventProceed(lk,EVT_INDICAT,EVT_TCP_CONNECTED,NULL,0);
				return	0;
			}
		}

		// error while trying to connect to RTU
		// bad event, bad address, bad port, ...
		LapDisconnected(lk,EVT_TCP_REJECTED,"connection rejected");
		return	0;
	}

	// normal case socket is connected
	if	((revents & POLLIN) == POLLIN)
	{	// must read
		int	ret;

		ret	= ReadSocket(lk);
		if	(ret < 0)
			return	0;
	}

//dopollout :
	if	((revents & POLLOUT) == POLLOUT)
	{	// can write
		LapRunOutQueue(lk);
	}

	return	0;
}

static int	CB_LapEvent(void *tb,int fd,void *r1,void *r2,int revents)
{
	int	ret;
	int	mode;

	ret	= _CB_LapEvent(tb,fd,r1,r2,revents);
#if	0
	if	(LkCtxt.ct_pollrawactive == 1)
	{
		t_xlap_link	*lk	= (t_xlap_link *)r1;
		
		// recompute revents after an event on network side
		mode	= CB_LapRequest(tb,fd,r1,r2,revents);
		rtl_epollSetEvt2(LkCtxt.ct_tbpoll,lk->lk_fdpos,mode);
		return	ret;
		
	}
	if	(LkCtxt.ct_pollrawactive == 2)
	{
		// do nothing full raw mode
		return	ret;
	}
#endif
	// do nothing full raw mode
	return	ret;
}

static	void	IncomingFrame(t_xlap_link *lk)
{
	t_xlap_ctxt	*pctxt;
	u_char 	*buf;
	int	sz;
	int	nr;
	int	ns;

	int	idxctrl;

	pctxt	= lk->lk_ctxt;

	buf	= lk->lk_rframe+lk->lk_rpos;
	if (buf[0] == STARTFRAMELONG && LongModeActive)
	{
		sz	= lk->lk_rcnt - StartSz - 2;
RTL_TRDBG(XLAP_TRACE,"(long) complete frame on RTU(%s,%s) fd=%d sz=%d\n",
			lk->lk_addr,lk->lk_port,lk->lk_fd,sz);
RTL_TRDBG(XLAP_TRACE,"o1=%02x o2=%02x o3=%02x o4=%02x o5=%02x o6=%02x o7=%02x\n",
			buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
	}
	else
	{
		sz	= lk->lk_rcnt - StartSz - 1;
RTL_TRDBG(XLAP_TRACE,"complete frame on RTU(%s,%s) fd=%d sz=%d\n",
			lk->lk_addr,lk->lk_port,lk->lk_fd,sz);
RTL_TRDBG(XLAP_TRACE,"o1=%02x o2=%02x o3=%02x o4=%02x o5=%02x o6=%02x o7=%02x\n",
			buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6]);
	}


	lk->lk_xFrmRcvAt	= rtl_timemono(NULL);

	if	(sz <= 0 || (buf[0] != STARTFRAME && (buf[0] != STARTFRAMELONG || !LongModeActive)))
	{
		//RTL_TRDBG(0,"recv sync error sz=%d b=%d\n",sz,buf[0]);
		RTL_TRDBG(0, "APCI 0x%02x unknown received from %s:%s\n",
			buf[0], lk->lk_rhost, lk->lk_rport);
		return;
	}

	if	(buf[0] == STARTFRAME)
	{
		idxctrl	= StartSz + 1;	// size length = 1
		if	(sz != buf[1])
		{
			//RTL_TRDBG(0,"recv sync error sz=%d bsz=%d\n", sz,buf[1]);
			RTL_TRDBG(0, "ACPI unknown received from %s:%s (bad size)\n",
				lk->lk_rhost, lk->lk_rport);
			return;
		}
	}
	else
	{
		idxctrl	= StartSz + 2;	// size length = 2
		if	(sz != (buf[1]*256 + buf[2]))
		{
			RTL_TRDBG(0,"recv sync error sz=%d bsz=%d\n",
				sz,buf[1]*256 + buf[2]);
			return;
		}
	}

	if	(sz == 4 && buf[idxctrl] == TESTFRact)
	{
RTL_TRDBG(2,"recv FRAME_U TESTFRact\n");
		lk->lk_nbrecvu++;
		lk->lk_nbrecvB += 4;
		pctxt->ct_lkstat.st_nbrecvu++;
		pctxt->ct_lkstat.st_nbrecvB += 4;
		EventProceed(lk,EVT_INDICAT,EVT_TESTFRact,NULL,0);
		return;
	}
	if	(sz == 4 && buf[idxctrl] == TESTFRcon)
	{
RTL_TRDBG(2,"recv FRAME_U TESTFRcon\n");
		lk->lk_uFrmSndAt	= 0;
		lk->lk_nbrecvu++;
		lk->lk_nbrecvB += 4;
		pctxt->ct_lkstat.st_nbrecvu++;
		pctxt->ct_lkstat.st_nbrecvB += 4;
		EventProceed(lk,EVT_INDICAT,EVT_TESTFRcon,NULL,0);
		return;
	}

	if	(sz == 4 && buf[idxctrl] == STOPDTact)
	{
RTL_TRDBG(2,"recv FRAME_U STOPDTact\n");
		lk->lk_nbrecvu++;
		lk->lk_nbrecvB += 4;
		pctxt->ct_lkstat.st_nbrecvu++;
		pctxt->ct_lkstat.st_nbrecvB += 4;
		EventProceed(lk,EVT_INDICAT,EVT_STOPDTact,NULL,0);
		return;
	}
	if	(sz == 4 && buf[idxctrl] == STOPDTcon)
	{
RTL_TRDBG(2,"recv FRAME_U STOPDTcon\n");
		lk->lk_uFrmSndAt	= 0;
		lk->lk_nbrecvu++;
		lk->lk_nbrecvB += 4;
		pctxt->ct_lkstat.st_nbrecvu++;
		pctxt->ct_lkstat.st_nbrecvB += 4;
		EventProceed(lk,EVT_INDICAT,EVT_STOPDTcon,NULL,0);
		return;
	}

	if	(sz == 4 && buf[idxctrl] == STARTDTact)
	{
RTL_TRDBG(2,"recv FRAME_U STARTDTact\n");
		lk->lk_nbrecvu++;
		lk->lk_nbrecvB += 4;
		pctxt->ct_lkstat.st_nbrecvu++;
		pctxt->ct_lkstat.st_nbrecvB += 4;
		EventProceed(lk,EVT_INDICAT,EVT_STARTDTact,NULL,0);
		return;
	}
	if	(sz == 4 && buf[idxctrl] == STARTDTcon)
	{
RTL_TRDBG(2,"recv FRAME_U STARTDTcon\n");
		lk->lk_uFrmSndAt	= 0;
		lk->lk_nbrecvu++;
		lk->lk_nbrecvB += 4;
		pctxt->ct_lkstat.st_nbrecvu++;
		pctxt->ct_lkstat.st_nbrecvB += 4;
		EventProceed(lk,EVT_INDICAT,EVT_STARTDTcon,NULL,0);
		return;
	}


	// S frame

	if	(sz == 4 && buf[idxctrl] == 0x01)
	{
		//u_char	ex;
		uint32_t	ex;

		lk->lk_nbrecvs++;
		lk->lk_nbrecvB += 4;
		pctxt->ct_lkstat.st_nbrecvs++;
		pctxt->ct_lkstat.st_nbrecvB += 4;
		nr	= GETNR(buf);
		ex	= GETEXT(buf);

RTL_TRDBG(2,"recv FRAME_S(nr=%d,ex=%d) (vr=%d,vs=%d,va=%d) => nothing\n",nr,ex,
			lk->lk_vr,lk->lk_vs,lk->lk_va);

		if	(OKNR(lk,nr) == 0)
		{
//			LapDisconnected(lk,EVT_FRAME_NRERROR,"connection cleared NOK NR");
RTL_TRDBG(0,"NOK NR %d %d on RTU (%s,%s)\n",nr,lk->lk_vr,
						lk->lk_addr,lk->lk_port);
			lk->lk_nberrnr++;
			pctxt->ct_lkstat.st_nberrnr++;
			LapSendFrameS(lk,lk->lk_vr);
			EventProceed(lk,EVT_INDICAT,EVT_FRAME_NRERROR,NULL,0);
			return;
		}
		LapRunAckQueue(lk,nr);
		lk->lk_va	= nr;
		EventProceed(lk,EVT_INDICAT,EVT_FRAMES,(void *)ex,0);
		return;
	}

	if	( (buf[idxctrl] & 0x01) != 0 || (buf[idxctrl+2] & 0x01) != 0 )
	{
//		LapDisconnected(lk,EVT_FRAME_FMTERROR,"connection cleared frame format");
		EventProceed(lk,EVT_INDICAT,EVT_FRAME_FMTERROR,NULL,0);
		return;
	}

	// I frame
	lk->lk_nbrecvi++;
	lk->lk_nbrecvB += sz;
	pctxt->ct_lkstat.st_nbrecvi++;
	pctxt->ct_lkstat.st_nbrecvB += sz;

	ns	= GETNS(buf);
	nr	= GETNR(buf);

RTL_TRDBG(2,"recv FRAME_I(ns=%d,nr=%d) (vr=%d,vs=%d,va=%d)\n",ns,nr,
			lk->lk_vr,lk->lk_vs,lk->lk_va);
/*
	{
	char	tmp[1024];
	rtl_binToStr(buf+6,sz-4,tmp,sizeof(tmp)-10);
	RTL_TRDBG(1,"sz=%d data='%s'\n",sz-4,tmp);
	}
*/

	if	(OKNR(lk,nr) == 0)
	{
//		LapDisconnected(lk,EVT_FRAME_NRERROR,"connection cleared NOK NR");
RTL_TRDBG(0,"NOK NR %d %d on RTU (%s,%s)\n",nr,lk->lk_vr,
						lk->lk_addr,lk->lk_port);
		lk->lk_nberrnr++;
		pctxt->ct_lkstat.st_nberrnr++;
		LapSendFrameS(lk,lk->lk_vr);
		EventProceed(lk,EVT_INDICAT,EVT_FRAME_NRERROR,NULL,0);
		return;
	}
	LapRunAckQueue(lk,nr);
	lk->lk_va	= nr;

	if	(OKNS(lk,ns) == 0)
	{
//		LapDisconnected(lk,EVT_FRAME_NSERROR,"connection cleared NOK NS");
		
RTL_TRDBG(0,"NOK NS %d %d on RTU (%s,%s)\n",ns,lk->lk_vr,
						lk->lk_addr,lk->lk_port);
		lk->lk_nberrns++;
		pctxt->ct_lkstat.st_nberrns++;
		LapSendFrameS(lk,lk->lk_vr);
//		LapSendFrameSExt(lk,1,lk->lk_vr);	// reject ...
		EventProceed(lk,EVT_INDICAT,EVT_FRAME_NSERROR,NULL,0);
		return;
	}
	lk->lk_vr	= (lk->lk_vr + 1)%lk->lk_n;
	if	(lk->lk_iFrmRcvAt == 0)
		lk->lk_iFrmRcvAt	= rtl_timemono(NULL);

	if	((lk->lk_vr - lk->lk_vapeer + lk->lk_n)%lk->lk_n == lk->lk_w)
	{
		LapSendFrameS(lk,lk->lk_vr);
	}

	if (buf[0] == STARTFRAMELONG && LongModeActive)
		EventProceed(lk,EVT_INDICAT,EVT_FRAMEI,buf+ApciSz+1,sz-4);
	else
		EventProceed(lk,EVT_INDICAT,EVT_FRAMEI,buf+ApciSz,sz-4);
}


static	void	LapSetState(t_xlap_link *lk,int newstate)
{
#if	0
	if	(lk->lk_state == newstate)
		return;
#endif
	lk->lk_stateAt	= rtl_timemono(NULL);
	lk->lk_stateP	= lk->lk_state;
	lk->lk_state	= newstate;


	RTL_TRDBG(1,"(%p,%s,%s) from st='%s'to st='%s'(%d->%d)\n",
		lk,lk->lk_addr,lk->lk_port,
		rtl_eLapStateTxt(lk->lk_stateP),rtl_eLapStateTxt(lk->lk_state),
		lk->lk_stateP,lk->lk_state);

	if	(lk->lk_stateP != newstate)
	{
		switch	(newstate)
		{
		case	SSP_STOPPED :
			if	(lk->lk_stateP != SSP_INIT)
			EventProceed(lk,EVT_APIONLY,EVT_LK_STOPPED,NULL,0);
		break;
		case	SSP_STARTED :
			EventProceed(lk,EVT_APIONLY,EVT_LK_STARTED,NULL,0);
		break;
		}
	}
}

static	void	LapTimerT1T2T3(t_xlap_link *lk,int now)
{
	if	(!lk->lk_conn)
		return;
	if	(IS_TCP_LISTENER(lk))
		return;
	if	(lk->lk_uFrmSndAt &&
			ABS(now - lk->lk_uFrmSndAt) > lk->lk_t1)
	{
		LapDisconnected(lk,EVT_FRAME_NOACK,"no ack for frame U");
		return;
	}
	if	(lk->lk_iFrmRcvAt &&
			ABS(now - lk->lk_iFrmRcvAt) > lk->lk_t2)
	{
		lk->lk_iFrmRcvAt	= 0;
		LapSendFrameS(lk,lk->lk_vr);
		return;
	}
	if	(lk->lk_xFrmRcvAt == 0 ||
		ABS(now - lk->lk_xFrmRcvAt) > lk->lk_t3)
	{
		if	(lk->lk_tFrmSndAt == 0)
		{
			LapSendTESTFRact(lk);
		}
		return;
	}
}


static	void	LapTimerT1T2T3All(t_xlap_ctxt *pctxt,int now)
{
	t_xlap_link	*lk;
	t_xlap_link	*tmp;

	list_for_each_entry_safe(lk,tmp,&pctxt->ct_fulllist,list) 
	{
		LapTimerT1T2T3(lk,now);
	}
}


static	void	FreeDnsResol(t_xlap_link *lk)
{
	struct addrinfo *svdns;

	svdns	= (struct addrinfo *)lk->lk_addrinfoOk;
	if	(svdns->ai_addr)
		free(svdns->ai_addr);
	free	(svdns);
	lk->lk_addrinfoOk	= NULL;
}

static	void	SaveDnsResol(t_xlap_link *lk,struct addrinfo *rp)
{
	struct addrinfo *svdns;
	struct sockaddr *svaddr;
	int	sz;
	char	host[NI_MAXHOST];

	sz	= sizeof(struct addrinfo);
	svdns	= (struct addrinfo *)malloc(sz);
	if	(svdns)
	{
		sz	= rp->ai_addrlen;
		svaddr	= (struct sockaddr *)malloc(sz);
		if	(svaddr)
		{
			memcpy(svaddr,rp->ai_addr,
					sizeof(struct sockaddr));
			memcpy(svdns,rp,
						sizeof(struct addrinfo));
			svdns->ai_addr		= svaddr;
			svdns->ai_addrlen	= sz;
			lk->lk_addrinfoOk	= svdns;

			// for traces only
       			host[0]	= '\0';
			sz	= rp->ai_addrlen;
			getnameinfo(rp->ai_addr,sz,
					host,NI_MAXHOST,NULL,0,NI_NUMERICHOST);
RTL_TRDBG(1,"keep DNS resolution '%s' => '%s'\n", lk->lk_addr,host);
		}
	}
}

int	rtl_eLapConnect(t_xlap_link *lk)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int	fd;
	int	err;
	int	reuseaddrinfo	= 0;

	char host[NI_MAXHOST];

	if	(!lk || !lk->lk_addr || !*lk->lk_addr || !IS_TCP_CLIENT(lk))
		return	-1;
	if	(atoi(lk->lk_port) <= 0)
		return	0;

	lk->lk_conn	= 0;
	lk->lk_prog	= 0;
	lk->lk_progAt	= 0;
	lk->lk_fd	= -1;
	lk->lk_serial	= 0;

	lk->lk_rframe	= NULL;
	lk->lk_ralloc	= 0;
	lk->lk_rsz	= 0;
	lk->lk_rpos	= 0;
	lk->lk_rcnt	= 0;
	lk->lk_rwait	= 0;

	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family		= AF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= 0;
	hints.ai_protocol	= 0;

	if	(getaddrinfo(lk->lk_addr,lk->lk_port,&hints,&result) != 0)
	{
	RTL_TRDBG(0,"cannot getaddrinfo(%s,%s)\n",lk->lk_addr,lk->lk_port);
		if	(IS_LNK_SAVEDNSENT(lk) == 0)
			return	-3;
		if	(lk->lk_addrinfoOk == NULL)
			return	-2;
		// we try with the last successfull addrinfo
		reuseaddrinfo	= 1;
		result		= (struct addrinfo *)lk->lk_addrinfoOk;
		result->ai_next	= NULL;

		host[0]	= '\0';
		getnameinfo(result->ai_addr,result->ai_addrlen,
					host,NI_MAXHOST,NULL,0,NI_NUMERICHOST);
		RTL_TRDBG(1,"reuse DNS resolution '%s' => '%s'\n",
				lk->lk_addr,host);
	}

	fd	= -1;
	for	(rp = result ; rp != NULL ; rp = rp->ai_next)
	{
		// free previous dns resolution
		if	(IS_LNK_SAVEDNSENT(lk) 
				&& reuseaddrinfo == 0 && lk->lk_addrinfoOk)
		{
			FreeDnsResol(lk);
		}
		// keep last dns resolution
		if	(IS_LNK_SAVEDNSENT(lk) 
				&& reuseaddrinfo == 0 && !lk->lk_addrinfoOk)
		{
			SaveDnsResol(lk,rp);
		}

		rp->ai_socktype = rp->ai_socktype | SOCK_STREAM | SOCK_CLOEXEC;
		fd	= socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
		if	(fd < 0)
			continue;
		if	(fcntl(fd,F_SETFL,O_NONBLOCK,0) < 0)
		{
		RTL_TRDBG(0,"cannot fcntl on new socket (%s)\n",STRERRNO);
			close	(fd);
			fd	= -1;
			continue;
		}
		err	= connect(fd,rp->ai_addr, rp->ai_addrlen);
//RTL_TRDBG(1,"connect() ==> err=%d errno=%d (%s)\n",err,errno,STRERRNO);
		if	(err != -1 || (err == -1 && errno == EINPROGRESS))
		{
			break;	// success
		}
		RTL_TRDBG(0,"cannot connect on new socket(%s,%s) fd=%d\n",
						lk->lk_addr,lk->lk_port,fd);
		close	(fd);
		fd	= -1;
	}

	if	(!reuseaddrinfo)
	{
		freeaddrinfo(result);
	}

	if	(rp == NULL || fd < 0)
	{
		RTL_TRDBG(0,"cannot connect RTU (%s,%s)\n",
						lk->lk_addr,lk->lk_port);
		if	(fd > 0)
			close	(fd);
		return	-4;
	}

	{
		int	nodelay	= 1;
		setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,
			(char *)&nodelay,sizeof(nodelay));
	}
	{
		int	quickack	= 1;
		setsockopt(fd,IPPROTO_TCP,TCP_QUICKACK,
			(char *)&quickack,sizeof(quickack));
	}

	RTL_TRDBG(1,"connect in progress on RTU(%p,%s,%s) fd=%d\n",
				lk,lk->lk_addr,lk->lk_port,fd);

	lk->lk_conn	= 0;
	lk->lk_prog	= 0;
	lk->lk_progAt	= rtl_timemono(NULL);
	lk->lk_fd	= fd;

	lk->lk_fdpos	= rtl_epollAdd(LkCtxt.ct_tbpoll,fd,
			CB_LapEvent,NULL,lk,(void *)RTL_ELAP_LINK_MAGIC);
	lk->lk_serial	= rtl_epollSerial(LkCtxt.ct_tbpoll,fd);

	// async connect on TCP client => POLLOUT
	rtl_epollSetEvt2(LkCtxt.ct_tbpoll,lk->lk_fdpos,POLLOUT);

	return	fd;
}

int	rtl_eLapReConnect(t_xlap_link *lk)
{
	time_t	now;

	rtl_timemono(&now);
	if	(!lk || !IS_TCP_CLIENT(lk))
		return	-1;
	if	(lk->lk_conn == 0 && IS_TCP_RECONN(lk) && lk->lk_fd < 0)
		return	rtl_eLapConnect(lk);
	if	(lk->lk_conn == 0 && IS_TCP_RECONN(lk) && lk->lk_fd >= 0
		&& lk->lk_progAt != 0 && ABS(now - lk->lk_progAt) > 3)
	{	// connect pending and no response ...
		LapDisconnected(lk,EVT_TCP_REJECTED,"connection pending timeout");
	}
	return	0;
}

int	rtl_eLapBind(t_xlap_link *lk)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int	fd;
	int	err;
	int	flag;

	if	(!lk || !lk->lk_addr || !*lk->lk_addr || !IS_TCP_LISTENER(lk))
		return	-1;

	lk->lk_conn	= 0;
	lk->lk_prog	= 0;
	lk->lk_progAt	= 0;
	lk->lk_fd	= -1;
	lk->lk_serial	= 0;

	lk->lk_rsz	= 0;
	lk->lk_rpos	= 0;
	lk->lk_rcnt	= 0;
	lk->lk_rwait	= 0;

	memset(&hints,0,sizeof(struct addrinfo));
	hints.ai_family		= AF_UNSPEC;
//	hints.ai_socktype	= SOCK_STREAM | SOCK_CLOEXEC;
	hints.ai_socktype	= SOCK_STREAM;
	hints.ai_flags		= AI_PASSIVE;
	hints.ai_protocol	= 0;

	if	(getaddrinfo(lk->lk_addr,lk->lk_port,&hints,&result) != 0)
	{
	RTL_TRDBG(0,"cannot getaddrinfo(%s,%s)\n",lk->lk_addr,lk->lk_port);
		return	-2;
	}

	for	(rp = result ; rp != NULL ; rp = rp->ai_next)
	{
		fd	= socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
		if	(fd < 0)
			continue;

		flag	= 1;
		if	( setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,
					(char *)&flag, sizeof(flag)) < 0)
		{
			RTL_TRDBG(0,"cannot sockopt reuse on RTU (%s,%s)\n",
						lk->lk_addr,lk->lk_port);
			close	(fd) ;
			continue;
		}

		errno	= 0;
		err	= bind(fd,rp->ai_addr, rp->ai_addrlen);
RTL_TRDBG(1,"bind() ==> err=%d errno=%d (%s)\n",err,errno,STRERRNO);
		if	(err != -1)
		{
			break;	// success
		}
		RTL_TRDBG(0,"cannot bind on new socket(%s,%s) fd=%d\n",
						lk->lk_addr,lk->lk_port,fd);
		close	(fd);
	}

	if	(rp == NULL)
	{
		RTL_TRDBG(0,"cannot bind on RTU (%s,%s)\n",
						lk->lk_addr,lk->lk_port);
		return	-4;
	}

	flag	= 1;
	if	( setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,
				(char *)&flag, sizeof(flag)) < 0)
	{
		RTL_TRDBG(0,"cannot sockopt reuse on RTU (%s,%s)\n",
						lk->lk_addr,lk->lk_port);
		close	(fd) ;
		return	-5;
	}
	if	(listen(fd,SOMAXCONN) < 0)
	{
		close(fd);
		RTL_TRDBG(0,"cannot listen on RTU (%s,%s)\n",
						lk->lk_addr,lk->lk_port);
		return	-6;
	}

	RTL_TRDBG(1,"bind+listen OK on RTU(%p,%s,%s) fd=%d\n",
				lk,lk->lk_addr,lk->lk_port,fd);

	lk->lk_conn	= 0;
	lk->lk_prog	= 0;
	lk->lk_progAt	= 0;
	lk->lk_fd	= fd;

	lk->lk_fdpos	= rtl_epollAdd(LkCtxt.ct_tbpoll,fd,
			CB_LapEvent,NULL,lk,(void *)RTL_ELAP_LINK_MAGIC);
	lk->lk_serial	= rtl_epollSerial(LkCtxt.ct_tbpoll,fd);

	lk->lk_bindclosed	= 0;

	// bind+listen only POLLIN is required
	rtl_epollSetEvt2(LkCtxt.ct_tbpoll,lk->lk_fdpos,POLLIN);

	return	fd;
}





static	int	EventMasterSlave(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
	int	treated	= 0;
	//int	request	= 0;

#if	0
	if	(evttype > IM_REQ)
	{
		request	= 1;
		evttype	= evttype % IM_REQ;
	}
#endif


	switch	(evtnum + evttype)
	{
	case	EVT_TESTFRact				+ EVT_INDICAT :
		LapSendTESTFRcon(lk);
		return	1;
	break;
	case	EVT_TESTFRcon				+ EVT_INDICAT :
		lk->lk_tFrmSndAt	= 0;
		//printf("rtt=%d\n",ABS(rtl_tmmsmono() - lk->lk_tFrmSndAtMs));
		return	1;
	break;
	case	EVT_TESTFRact				+ EVT_REQUEST :
		LapSendTESTFRact(lk);
		return	1;
	break;
	}

	switch	(lk->lk_state + evtnum + evttype)
	{
	case	SSP_INIT	+ EVT_TCP_CREATED	+ EVT_INDICAT :
	case	SSP_INIT	+ EVT_TCP_ACCEPTED	+ EVT_INDICAT :
	case	SSP_INIT	+ EVT_TCP_CONNECTED	+ EVT_INDICAT :
		if	(IS_SSP_SLAVE(lk))
		{
			LapSetState(lk,SSP_STOPPED);
			return	1;
		}
		if	(IS_SSP_AUTOSTART(lk))
		{
			LapSendSTARTDTact(lk);
			LapSetState(lk,SSP_PENDING_STARTED);
			return	1;
		}
		LapSetState(lk,SSP_STOPPED);
		return	1;
	break;

	case	SSP_STOPPED	+ EVT_STARTDTact	+ EVT_INDICAT :
		LapSendSTARTDTcon(lk);
		LapSetState(lk,SSP_STARTED);
		return	1;
	break;
	case	SSP_STOPPED	+ EVT_STARTDTact	+ EVT_REQUEST :
		if	(0 && IS_SSP_SLAVE(lk))
			return	0;
RTL_TRDBG(0,"(%p,%s,%s) wants to start on (%p,%s,%s)\n",
				lk,lk->lk_rhost,lk->lk_rport,
				lk->lk_listener,lk->lk_addr,lk->lk_port);
		LapSendSTARTDTact(lk);
		LapSetState(lk,SSP_PENDING_STARTED);
		return	1;
	break;

	case	SSP_PENDING_STARTED	+ EVT_STARTDTcon+ EVT_INDICAT :
		LapSetState(lk,SSP_STARTED);
		return	1;
	break;

	case	SSP_STARTED	+ EVT_STOPDTact		+ EVT_INDICAT :
		if	(lk->lk_vs == lk->lk_va) {
			LapSendSTOPDTcon(lk);
			LapSetState(lk,SSP_STOPPED);
		}
		else {
			// if pending ack enter state SSP_PENDING_UNCONFD
			LapSetState(lk,SSP_PENDING_UNCONFD);
		}
		return	1;
	break;

	case	SSP_PENDING_UNCONFD + EVT_FRAMES + EVT_INDICAT :
		if	(lk->lk_vs == lk->lk_va) {
			LapSendSTOPDTcon(lk);
			LapSetState(lk,SSP_STOPPED);
		}
		return 1;
	break;

	case	SSP_PENDING_UNCONFD + EVT_FRAMEI + EVT_INDICAT :
		LapSendSTOPDTcon(lk);
		LapSetState(lk,SSP_STOPPED);
		return 1;
	break;

	case	SSP_STARTED	+ EVT_STARTDTact	+ EVT_INDICAT :
		LapSendSTARTDTcon(lk);
		return	1;
	break;
	case	SSP_STARTED	+ EVT_STOPDTact		+ EVT_REQUEST :
		if	(0 && IS_SSP_SLAVE(lk))
			return	0;
RTL_TRDBG(0,"(%p,%s,%s) wants to stop\n",lk,lk->lk_addr,lk->lk_port);
		LapSendSTOPDTact(lk);
		LapSetState(lk,SSP_PENDING_STOPPED);
	break;

	case	SSP_PENDING_STOPPED	+EVT_STOPDTcon	+ EVT_INDICAT :
		LapSetState(lk,SSP_STOPPED);
	break;
	}
	if	(treated)
		return	treated;

	return	treated;
}

static	int	EventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
	int	treated	= 0;
	int	retuser = 0;

	if	(evttype == EVT_APIONLY && lk->lk_cbEvent)
	{
		retuser	= (*lk->lk_cbEvent)(lk,evttype,evtnum,data,sz);
		return retuser;
	}

	if	(!IS_TCP_LISTENER(lk))
	{

	RTL_TRDBG(XLAP_TRACE,"(%p,%s,%s) st='%s' receive evttype=%d evtnum='%s'\n",
		lk,lk->lk_addr,lk->lk_port,
		rtl_eLapStateTxt(lk->lk_state),evttype,rtl_eLapEventTxt(evtnum));

		treated	= EventMasterSlave(lk,evttype,evtnum,data,sz);

		if	(!treated)
	RTL_TRDBG(XLAP_TRACE,"(%p,%s,%s) st='%s' evttype=%d evtnum=%d not treated\n",
		lk,lk->lk_addr,lk->lk_port,
		rtl_eLapStateTxt(lk->lk_state),evttype,evtnum);
	}

	if	(evttype == EVT_INDICAT && lk->lk_cbEvent)
	{
		retuser	= (*lk->lk_cbEvent)(lk,evttype,evtnum,data,sz);
	}
	return retuser;
}

int	rtl_eLapEventRequest(t_xlap_link *lk,int evtnum,void *data,int sz)
{
	int	treated;

	if	(!lk)
		return	-1;
	treated	= EventMasterSlave(lk,EVT_REQUEST,evtnum,data,sz);
	return	treated;
}

static	t_xlap_msg	*AllocateMsg(u_char *asdu,int lg)
{
	t_xlap_msg	*msg;
	u_char		*apdu;

	msg	= (t_xlap_msg *)malloc(sizeof(t_xlap_msg));
	if	(!msg)
		return	NULL;
	memset	(msg,0,sizeof(t_xlap_msg));
	// allocate few bytes more for long mode, only one required
	apdu	= (u_char *)malloc(lg+ApciSz+5);
	if	(!apdu)
	{
		free(msg);
		return	NULL;
	}
	msg->m_usz	= lg;		// user data size
	msg->m_data	= apdu;
	lg		= SETFRAME_ASDU(apdu,asdu,lg);
	msg->m_sz	= lg;		// full size
	msg->m_time	= rtl_timemono(NULL);
	msg->m_udata	= msg->m_data + (msg->m_sz - msg->m_usz);

	return	msg;
}

static	int	PollLapPutOutQueue(t_xlap_ctxt *ctxt,t_xlap_link *lk,u_char *asdu,int lg)
{
	void		*hpoll;
	t_xlap_msg	*msg;

	hpoll	= ctxt->ct_tbpoll;
	if	(IS_TCP_LISTENER(lk))
	{
		RTL_TRDBG(1,"(%p,%s,%s) st='%s' send on listener => drop\n",
		lk,lk->lk_addr,lk->lk_port,rtl_eLapStateTxt(lk->lk_state));
		return	0;
	}
	if	(lk->lk_state != SSP_STARTED)
	{
		RTL_TRDBG(1,"(%p,%s,%s) st='%s' !started => drop message\n",
		lk,lk->lk_addr,lk->lk_port,rtl_eLapStateTxt(lk->lk_state));
		return	0;
	}

	rtl_epollSetEvt2(hpoll,lk->lk_fdpos,POLLIN);
	msg	= AllocateMsg(asdu,lg);
	if	(!msg)
		return	-1;
	msg->m_tofd	= lk->lk_fd;
	msg->m_toserial	= lk->lk_serial;

	list_add_tail(&msg->list,&lk->lk_outqueue.list);
	lk->lk_outcount++;
	// more to write => POLLIN|POLLOUT
	rtl_epollSetEvt2(hpoll,lk->lk_fdpos,POLLIN|POLLOUT);

	return	lk->lk_outcount;
}

int	rtl_eLapPutOutQueue(t_xlap_link *lk,u_char *asdu,int lg)
{
	return	PollLapPutOutQueue(&LkCtxt,lk,asdu,lg);
}

int	rtl_eLapPutOutQueueExt(t_xlap_ctxt *ctxt,int fd,u_int serial,u_char *asdu,int lg)
{
	int		ret	= -1;
	t_xlap_msg	*msg;

	msg	= AllocateMsg(asdu,lg);
	if	(!msg)
		return	ret;

	msg->m_tofd	= fd;
	msg->m_toserial	= serial;

#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_lock_gcc(&ctxt->ct_extlock);
#else
	pthread_mutex_lock(&ctxt->ct_extlock);
#endif

	list_add_tail(&msg->list,&ctxt->ct_extqueue.list);
	ret	= ++ctxt->ct_extcount;

#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_unlock_gcc(&ctxt->ct_extlock);
#else
	pthread_mutex_unlock(&ctxt->ct_extlock);
#endif

	return	ret;
}

int	rtl_eLapDispatchQueueExt(t_xlap_ctxt *ctxt,int maxcnt,int *left,int *err)
{
	int	nb	= 0;
	int	nberr	= 0;

	if	(!ctxt || ctxt != &LkCtxt)
		return	-1;

	*err	= 0;
	*left	= ctxt->ct_extcount;
	if	(ctxt->ct_extcount <= 0)
		return	0;

#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_lock_gcc(&ctxt->ct_extlock);
#else
	pthread_mutex_lock(&ctxt->ct_extlock);
#endif

	while	( !list_empty(&ctxt->ct_extqueue.list) ) 
	{
		t_xlap_msg	*msg;
		t_epollhand	*hand;
		t_xlap_link	*lk;

		msg = list_entry(ctxt->ct_extqueue.list.next,t_xlap_msg,list);
		list_del(&msg->list);
		ctxt->ct_extcount--;
		hand	= rtl_epollHandle(ctxt->ct_tbpoll,msg->m_tofd);
		if	(!hand)
		{
printf("dispatch m=(fd=%d serial=%u) handle not found\n",msg->m_tofd,msg->m_toserial);
			nberr++;
			if	(msg->m_data)
				free(msg->m_data);
			free(msg);
			continue;
		}
		if	(hand->hd_fd != msg->m_tofd || hand->hd_ref == NULL)
		{
printf("dispatch m=(fd=%d serial=%u) h=(fd=%d serial=%x ref=%p) bad references\n",msg->m_tofd,msg->m_toserial,hand->hd_fd,hand->hd_serial,hand->hd_ref);
			nberr++;
			if	(msg->m_data)
				free(msg->m_data);
			free(msg);
			continue;
		}
		lk	= (t_xlap_link *)hand->hd_ref;
		if	(lk->lk_magic != RTL_ELAP_LINK_MAGIC)
		{
printf("dispatch h=(fd=%d serial=%u magic=%x) bad magic\n",hand->hd_fd,hand->hd_serial,lk->lk_magic);
			nberr++;
			if	(msg->m_data)
				free(msg->m_data);
			free(msg);
			continue;
		}
		if	(lk->lk_serial != msg->m_toserial)
		{
printf("dispatch m=(fd=%d serial=%u) l=(fd=%d serial=%x) bad references\n",msg->m_tofd,msg->m_toserial,lk->lk_fd,lk->lk_serial);
			nberr++;
			if	(msg->m_data)
				free(msg->m_data);
			free(msg);
			continue;
		}
		rtl_eLapPutOutQueue(hand->hd_ref,msg->m_udata,msg->m_usz);
		if	(msg->m_data)
			free(msg->m_data);
		free(msg);
		nb++;
		if	(maxcnt > 0 && nb >= maxcnt)
			break;
	}
	*left	= ctxt->ct_extcount;
	*err	= nberr;

#ifdef	RTL_ELAP_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_unlock_gcc(&ctxt->ct_extlock);
#else
	pthread_mutex_unlock(&ctxt->ct_extlock);
#endif

	return	nb;
}

void	rtl_eLapBindAll(t_xlap_ctxt *pctxt)
{
	t_xlap_link	*lk;

	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	list_for_each_entry(lk,&pctxt->ct_fulllist,list) 
	{
		if	(IS_TCP_LISTENER(lk))
		{
			rtl_eLapBind(lk);
		}
	}

}

void	rtl_eLapConnectAll(t_xlap_ctxt *pctxt)
{
	t_xlap_link	*lk;

	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	if	((pctxt->ct_flags & LAP_NO_TCP_CLI) == LAP_NO_TCP_CLI)
		return;

	list_for_each_entry(lk,&pctxt->ct_fulllist,list) 
	{
		if	(IS_TCP_CLIENT(lk))
		{
			rtl_eLapConnect(lk);
		}
	}

}

void	rtl_eLapReConnectAll(t_xlap_ctxt *pctxt)
{
	t_xlap_link	*lk;

	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	if	((pctxt->ct_flags & LAP_NO_TCP_CLI) == LAP_NO_TCP_CLI)
		return;

	list_for_each_entry(lk,&pctxt->ct_fulllist,list) 
	{
		if	(IS_TCP_CLIENT(lk))
		{
			rtl_eLapReConnect(lk);
		}
	}

}

void	rtl_eLapDoClockScPeriod(t_xlap_ctxt *pctxt,int reconnect)
{
	static	__thread u_int	nbclock	= 0;
	time_t	now		= 0;

	if	(pctxt == NULL)
		pctxt	= &LkCtxt;


//	RTL_TRDBG(1,"DoClockSc()\n");
	nbclock++;
	if	(reconnect && nbclock % reconnect == 0)
	{
		rtl_eLapReConnectAll(pctxt);
	}
	rtl_timemono(&now);

	LapTimerT1T2T3All(pctxt,now);
	LapTimerAckQueueAll(pctxt,now);

//	if	(nbclock % 3 == 0)
//		LapPutOutQueueAll();
//
//

}

void	rtl_eLapDoClockSc(t_xlap_ctxt *pctxt)
{
	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	rtl_eLapDoClockScPeriod(pctxt,3);
}

#if	0
static	int	WalkLink(time_t now,void *p1,void *p2)
{
	t_xlap_ctxt	*pctxt	= (t_xlap_ctxt *)p1;
	t_epollhand	*hand	= (t_epollhand *)p2;
	t_xlap_link	*lk	= (t_xlap_link *)hand->hd_ref;
	int		force;

	if	(hand->hd_ref2 != (void *)RTL_ELAP_LINK_MAGIC)
		return	0;
	if	(!lk)
		return	0;
	if	(lk->lk_magic != RTL_ELAP_LINK_MAGIC)
		return	0;

	LapTimerT1T2T3(lk,now);
	LapTimerAckQueue(lk,now,force=0);

	return	1;
}
#endif

int	rtl_eLapDoClockMs(t_xlap_ctxt *pctxt,int per)
{
	int	nb;

	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	if	(per <= 0)
		per	= 10;

	inline	int	WalkLink(time_t now,void *p1,void *p2)
	{
//		t_xlap_ctxt	*pctxt	= (t_xlap_ctxt *)p1;
		t_epollhand	*hand	= (t_epollhand *)p2;
		t_xlap_link	*lk	= (t_xlap_link *)hand->hd_ref;
		int		force;

		if	(hand->hd_ref2 != (void *)RTL_ELAP_LINK_MAGIC)
			return	0;
		if	(!lk)
			return	0;
		if	(lk->lk_magic != RTL_ELAP_LINK_MAGIC)
			return	0;

		LapTimerT1T2T3(lk,now);
		LapTimerAckQueue(lk,now,force=0);

		return	1;
	}

	nb	= rtl_epollWalk(pctxt->ct_tbpoll,per,WalkLink);
	return	nb;
}


int rtl_eLapCloseBind(t_xlap_link *lk,int action)
{
	int	nb	= 0;

	if	(!lk || !IS_TCP_LISTENER(lk))
		return	-1;
	if	(lk->lk_bindclosed == 0 && lk->lk_fd >= 0)
	{
		lk->lk_bindclosed	= 1;
		rtl_epollRmv(LkCtxt.ct_tbpoll,lk->lk_fd);
		close(lk->lk_fd);
		lk->lk_fd	= -1;
		lk->lk_serial	= 0;
	}

	if	(action <= CLOSEBIND_ONLY)
		return	0;

	t_xlap_link	*cli;
	t_xlap_link	*tmpcli;

	list_for_each_entry_safe(cli,tmpcli,&lk->lk_clilist,clilist) 
	{
		switch	(action)
		{
		case	CLOSEBIND_DISC_NICE:
			cli->lk_tobedisc	= 1;
			nb++;
		break;
		case	CLOSEBIND_DISC_NOW:
		default:
			LapDisconnected(cli,EVT_LK_DELETED,"listener closed");
			nb++;
		break;
		}
	}
	return	nb;
}


int rtl_eLapOpenBind(t_xlap_link *lk)
{
	int	ret;

	if	(!lk || !IS_TCP_LISTENER(lk))
		return	-1;
	if	(!lk->lk_bindclosed || lk->lk_fd >= 0)
		return	-2;
	ret	= rtl_eLapBind(lk);
	if	(ret < 0)
		return	ret;
	lk->lk_bindclosed	= 0;
	return	1;
}

t_xlap_stat	*rtl_eLapGetStat(t_xlap_ctxt *pctxt)
{
	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	return	&pctxt->ct_lkstat;
}

void	rtl_eLapResetStat(t_xlap_link *lk)
{
	t_xlap_stat	*st;

	if	(lk == NULL || lk == (t_xlap_link *)&LkCtxt.ct_lkstat)
		st	= &LkCtxt.ct_lkstat;
	else
		st	= &lk->lk_stat;
	memset	(st,0,sizeof(t_xlap_stat));
}

int	rtl_eLapPoll(t_xlap_ctxt *pctxt, int tms)
{
	int	max	= -1;
	int	nbd;
	int	left;
	int	err;

	if	(pctxt == NULL)
		pctxt	= &LkCtxt;

	nbd	= rtl_eLapDispatchQueueExt(pctxt,max,&left,&err);	

	return	rtl_epollRaw(pctxt->ct_tbpoll,tms);
}
