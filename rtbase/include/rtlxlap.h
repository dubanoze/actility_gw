
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

#ifndef RTL_XLAP_H_
#define RTL_XLAP_H_

/*! @file xlap.h
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
#include <arpa/inet.h>
#include <netdb.h>

#include "rtlbase.h"
#include "rtllist.h"
#include "rtlimsg.h"


#define DEFAULT_W	8
#define DEFAULT_K	12
#define DEFAULT_T0	30
#define DEFAULT_T1	15	// def 15
#define DEFAULT_T2	10
#define DEFAULT_T3	20
#define	OP_MOD		32767


#define DEFAULT_T4	(2*DEFAULT_T1)	// default DEFAULT_T1
#define	DEFAULT_N2	10


#define	FRAME_MAX	(16*1024)
#define	FRAME_MIN	(7)

#define	TCP_READ_SIZE	(16*1024)

#define	MAX_MSG_STORED	0



#define	STARTFRAME		0x68
#define	STARTFRAMELONG		0x69
#define	ACTIVATELONGLENGTH	250	// payload length that activate long mode

// U Frame types

#define	TESTFRact		0x43
#define	TESTFRcon		0x83
#define	STOPDTact		0x13
#define	STOPDTcon		0x23
#define	STARTDTact		0x07
#define	STARTDTcon		0x0B


// States for start/stop procedure

#define	SSP_INIT		1000	// initiliazed
#define	SSP_STOPPED		2000	// connected && ! established
#define	SSP_PENDING_STARTED	3000	// waiting STARTDTcon
#define	SSP_STARTED		4000	// connected && established
#define	SSP_PENDING_STOPPED	5000	// waiting STOPDTcon
#define	SSP_PENDING_UNCONFD	6000	// non ack messages waiting
#define	SSP_MAX			10000

// events types

#define	EVT_INDICAT		(0*SSP_MAX)
#define	EVT_REQUEST		(1*SSP_MAX)
#define	EVT_APIONLY		(2*SSP_MAX)

// events numbers

#define	EVT_TCP_ACCEPTED	100
#define	EVT_TCP_CONNECTED	101
#define	EVT_TCP_CREATED		102
#define	EVT_TCP_DISCONNECTED	103
#define	EVT_TCP_READERROR	104
#define	EVT_TCP_REJECTED	105

#define	EVT_FRAME_FMTERROR	106
#define	EVT_FRAME_NRERROR	107
#define	EVT_FRAME_NSERROR	108
#define	EVT_FRAME_NOACK		109
#define	EVT_FRAME_ACKED		110


#define	EVT_FRAMEU		200
#define	EVT_TESTFRact		200
#define	EVT_TESTFRcon		201
#define	EVT_STOPDTact		202
#define	EVT_STOPDTcon		203
#define	EVT_STARTDTact		204
#define	EVT_STARTDTcon		205
#define	EVT_FRAMEI		300
#define	EVT_FRAMES		400

#define	EVT_LK_CREATED		900
#define	EVT_LK_DELETED		901
#define	EVT_LK_STARTED		902
#define	EVT_LK_STOPPED		903

// link types and options

#define	LK_TCP_LISTENER		1
#define	LK_TCP_SERVER		2
#define	LK_TCP_CLIENT		4
#define	LK_SSP_MASTER		8
#define	LK_SSP_SLAVE		16
#define	LK_SSP_AUTOSTART	32
#define	LK_TCP_RECONN		64
#define	LK_TCP_NONBLOCK		128
#define	LK_LNK_SAVEDNSENT	256
#define LK_LNK_NOFULLRESET      512
#define	LK_EVT_LOWLEVEL		1024

#define	IS_TCP_LISTENER(l)	((l->lk_type&LK_TCP_LISTENER)==LK_TCP_LISTENER)
#define	IS_TCP_SERVER(l)	((l->lk_type&LK_TCP_SERVER)==LK_TCP_SERVER)
#define	IS_TCP_CLIENT(l)	((l->lk_type&LK_TCP_CLIENT)==LK_TCP_CLIENT)
#define	IS_SSP_MASTER(l)	((l->lk_type&LK_SSP_MASTER)==LK_SSP_MASTER)
#define	IS_SSP_SLAVE(l)		((l->lk_type&LK_SSP_SLAVE)==LK_SSP_SLAVE)
#define	IS_SSP_AUTOSTART(l)	((l->lk_type&LK_SSP_AUTOSTART)==LK_SSP_AUTOSTART)
#define	IS_TCP_RECONN(l)	((l->lk_type&LK_TCP_RECONN)==LK_TCP_RECONN)
#define	IS_TCP_NONBLOCK(l)	((l->lk_type&LK_TCP_NONBLOCK)==LK_TCP_NONBLOCK)

#define	IS_LNK_SAVEDNSENT(l)	((l->lk_type&LK_LNK_SAVEDNSENT)==LK_LNK_SAVEDNSENT)
#define IS_LNK_NOFULLRESET(l)   ((l->lk_type&LK_LNK_NOFULLRESET)==LK_LNK_NOFULLRESET)

#define	IS_EVT_LOWLEVEL(l)	((l->lk_type&LK_EVT_LOWLEVEL)==LK_EVT_LOWLEVEL)





typedef	struct
{
	time_t			m_time;
	u_int			m_sz;		// total frame size
	u_int			m_usz;		// size of user part
	u_char			*m_data;	// full frame
	u_char			*m_udata;	// user part of frame
	u_int			m_retry;
	u_char			m_forced;
	struct	list_head	list;
}	t_xlap_msg;

typedef	struct	s_xlap_stat
{
	// stats
	u_int		st_nbsendB;	// bytes
	u_int		st_nbsendu;	// frame u
	u_int		st_nbsends;	// frame s
	u_int		st_nbsendi;	// frame i
	u_int		st_nbrecvB;	// bytes
	u_int		st_nbrecvu;	// frame u
	u_int		st_nbrecvs;	// frame s
	u_int		st_nbrecvi;	// frame i
	u_int		st_nbrecvd;
	u_int		st_nberrnr;
	u_int		st_nberrns;
	u_int		st_nbretry;
	u_int		st_nbdropi;
}	t_xlap_stat;

typedef	struct	s_xlap_link
{
	char	lk_name[32];
	char	*lk_addr;
	char	*lk_port;
	u_int	lk_type;

	u_int	lk_t0, lk_t1, lk_t2, lk_t3;	// timers params
	u_char	lk_k, lk_w;			// window params
	u_short	lk_n;				// OP_MOD

	int	lk_state;
	time_t	lk_stateAt;
	int	lk_stateP;
	time_t	lk_progAt;
	char	lk_conn;
	char	lk_prog;
	int	lk_fd;
	void	*lk_addrinfoOk;	// last valid struct addrinfo from getaddrinfo()

	int	(*lk_cbEvent)(struct s_xlap_link *lk,int evttype,int evtnum,void *data,int sz);

	char	*lk_rhost;	// remote host for slave
	char	*lk_rport;	// remote port for slave
	struct s_xlap_link *lk_listener;	// listener for slave
	u_int	lk_cnxcount;	// if tcp listener
	u_int	lk_callcount;	// if tcp listener
	u_int	lk_maxConn;	// if tcp listener
	char	*lk_authList;	// if tcp listener
	u_char	lk_bindclosed;	// if tcp listener bind is suspended
	u_char	lk_tobedisc;	// to be disc asap

	u_char	*lk_rframe;		// current input frames buffer
	u_short	lk_ralloc;		// size of rframe allocated
	u_short	lk_rsz;			// total bytes in rframe
	u_short	lk_rcnt;		// rcnt bytes in rframe
	u_short	lk_rwait;		// rwait bytes to complete rframe
	u_short	lk_rpos;		// current read position in rframe

	// "X25" counters
	u_short	lk_va;
	u_short	lk_vr;
	u_short	lk_vs;
	u_short	lk_vapeer;

	// "X25" timers
	time_t	lk_xFrmRcvAt;	// for T3
	time_t	lk_iFrmRcvAt;	// for T2
	time_t	lk_uFrmSndAt;	// for T1
	time_t	lk_tFrmSndAt;
	time_t	lk_tFrmSndAtMs;

	t_xlap_msg	lk_outqueue;
	u_int		lk_outcount;
	t_xlap_msg	lk_ackqueue;
	u_int		lk_ackcount;
	u_int		lk_outbusy;

	u_int		lk_outdrop;	// because MAX_MSG_STORED

	// stats
	t_xlap_stat	lk_stat;

	// user
	void		*lk_userptr;
	u_int		lk_usertype;
	u_int		lk_userserial;

	// client list for listener links only
	struct	list_head 	lk_cliqueue;


	// the chain list of links
	struct	list_head 	list;		// full list
	struct	list_head 	listcli;	// client list for listener
}	t_xlap_link;


/* xlap.c */
char *rtl_LapStateTxt(int state);
char *rtl_LapShortStateTxt(int state);
char *rtl_LapEventTxt(int event);
void rtl_LapDisc(t_xlap_link *lk,char *msg);
void rtl_LapAddLink(t_xlap_link *lk);
void *rtl_LapAddLinkBind(t_xlap_link *lk);
void *rtl_LapInit(u_int flags, void *tbpoll);
int rtl_LapConnect(t_xlap_link *lk);
int rtl_LapReConnect(t_xlap_link *lk);
int rtl_LapBind(t_xlap_link *lk);
int rtl_LapPutOutQueue(t_xlap_link *lk, u_char *asdu, int lg);
void rtl_LapBindAll(void);
void rtl_LapConnectAll(void);
void rtl_LapReConnectAll(void);
void rtl_LapDoClockSc(void);	// == rtl_LapDoClockScPeriod(3)
void rtl_LapDoClockScPeriod(int reconnect);
int rtl_LapEventRequest(t_xlap_link *lk,int evtnum,void *data,int sz);
void rtl_LapDisableLongMode();
void rtl_LapEnableLongMode();
#define	CLOSEBIND_ONLY		0
#define	CLOSEBIND_DISC_NICE	1
#define	CLOSEBIND_DISC_NOW	2
int rtl_LapCloseBind(t_xlap_link *lk,int action);
int rtl_LapOpenBind(t_xlap_link *lk);

/* define old names to help for portability */
#ifdef	RTL_XLAP_DEFINE_OLD_NAMES
#define		LapStateTxt		rtl_LapStateTxt
#define		LapShortStateTxt	rtl_LapShortStateTxt
#define		LapEventTxt		rtl_LapEventTxt
#define		LapDisc			rtl_LapDisc				
#define		LapAddLink		rtl_LapAddLink
#define		LapAddLinkBind		rtl_LapAddLinkBind
#define		LapInit			rtl_LapInit		
#define		LapConnect		rtl_LapConnect		
#define		LapReConnect		rtl_LapReConnect		
#define		LapBind			rtl_LapBind		
#define		LapPutOutQueue		rtl_LapPutOutQueue		
#define		LapBindAll		rtl_LapBindAll		
#define		LapConnectAll		rtl_LapConnectAll		
#define		LapReConnectAll		rtl_LapReConnectAll		
#define		LapDoClockSc		rtl_LapDoClockSc		
#define		LapDoClockScPeriod	rtl_LapDoClockScPeriod		
#define		LapEventRequest		rtl_LapEventRequest		
#define		LapDisableLongMode	rtl_LapDisableLongMode		
#define		LapEnableLongMode	rtl_LapEnableLongMode		
#define		LapCloseBind		rtl_LapCloseBind
#define		LapOpenBind		rtl_LapOpenBind
#endif

#endif
