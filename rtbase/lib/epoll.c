
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

/*
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <time.h>
#include <signal.h>


//
//	typedef union epoll_data
//	{
//	  void *ptr;
//	  int fd;
//	  uint32_t u32;
//	  uint64_t u64;
//	} epoll_data_t;
//
//	struct epoll_event
//	{
//	  uint32_t events;	/* Epoll events */
//	  epoll_data_t data;	/* User data variable */
//	} __attribute__ ((__packed__));
//

typedef	struct epoll_event	t_epollevt;

#include "rtlepoll.h"
#if	0	// in rtlepoll.h
#define	RTL_EPTB_MAGIC	0x740411
#define	RTL_EPHD_MAGIC	0x960210


typedef struct epollhand
{
	unsigned int	hd_magic;
	int		hd_fd;
	unsigned int	hd_serial;
	int		(*hd_fctevent)();
	int		(*hd_fctrequest)();
	void		*hd_ref;
	void		*hd_ref2;
	unsigned int	hd_events;	// last events returned
	unsigned int	hd_revents;	// last events requested
} t_epollhand;

typedef struct epolltable
{
	unsigned int	ep_magic;
	int		ep_fd;
	int		ep_nbFd;
	int		ep_szTb;
	t_epollhand	**ep_hdTb;
} t_epolltable;
#endif


#define	SZPOLL			1000
#define	MAX_EVENTS_WAIT		1000

static	void	CheckDefines()
{
	if	(POLLIN != EPOLLIN || POLLOUT != EPOLLOUT 
				|| POLLPRI != EPOLLPRI || POLLERR != EPOLLERR)
	{
		printf("define POLLXXX != EPOLLXXX\n");
		exit(1);
	}
}

#undef	POLLIN
#undef	POLLOUT
#undef	POLLPRI
#undef	POLLERR

#define	POLLIN	EPOLLIN
#define	POLLOUT	EPOLLOUT
#define	POLLPRI	EPOLLPRI
#define	POLLERR	EPOLLERR

#if	0
#define	RTL_USE_SPIN_LOCK_FOR_SERIAL
#endif

/*
 * serial numbers are commun to all contexts/threads
 */
static	u_int	PHandSerial	= 0;
#ifdef	RTL_USE_SPIN_LOCK_FOR_SERIAL
static	int	PHandLockSerial	= 0;
#else
static	pthread_mutex_t	PHandLockSerial	= PTHREAD_MUTEX_INITIALIZER;
#endif
static	u_int	PHandDoSerial()
{
	u_int	ret;
#ifdef	RTL_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_lock_gcc(&PHandLockSerial);
#else
	pthread_mutex_lock(&PHandLockSerial);
#endif

	PHandSerial++;
	if	(PHandSerial == 0)
		PHandSerial	= 1;
	ret	= PHandSerial;

#ifdef	RTL_USE_SPIN_LOCK_FOR_SERIAL
	rtl_spin_unlock_gcc(&PHandLockSerial);
#else
	pthread_mutex_unlock(&PHandLockSerial);
#endif
	return	ret;
}

void	*rtl_epollInit()	/* TS */
{
	t_epolltable	*table;
	int		fd;

	CheckDefines();

	table	= calloc(1,sizeof(t_epolltable));
	if	(!table)
		return	NULL;

	table->ep_szTb	= SZPOLL;
	table->ep_hdTb	= (t_epollhand **)malloc(table->ep_szTb*sizeof(t_epollhand *));
	if	(!table->ep_hdTb)
	{
		free(table);
		return	NULL;
	}
	memset	(table->ep_hdTb,0,table->ep_szTb*sizeof(t_epollhand *));

	fd	= epoll_create1(EPOLL_CLOEXEC);
	if	(fd < 0)
	{
		free(table->ep_hdTb);
		free(table);
		return	NULL;
	}
	table->ep_magic	= RTL_EPTB_MAGIC;
	table->ep_fd	= fd;
	return table;
}

/*
 *
 *	Ajoute une entree dans la table de polling
 *
 */

int	rtl_epollAdd(void *ptbl,int fd,			/* TS */
		int (*fctevent)(void *,int,void *,void *,int),
		int (*fctrequest)(void *,int,void *,void *,int),
		void * ref,void * ref2)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;
	t_epollevt	pollevt;
	int		ret;

	if	(!ptbl)
		return	-1;

	if	(fctevent == NULL)
		return	-2;

	if	(fd < 0)
		return	-3;

	if	(fd >= table->ep_szTb)
	{
		int	i;
		int	nsz	= table->ep_szTb + SZPOLL;
		void	*ntb	= realloc(table->ep_hdTb,nsz*sizeof(t_epollhand *));

		if	(!ntb)
			return	-4;

		table->ep_hdTb	=(t_epollhand **)ntb;
		for	(i = table->ep_szTb ; i < nsz ; i++)
		{
			table->ep_hdTb[i]	= NULL;
		}
		table->ep_szTb	= nsz;
	}

	if	(table->ep_hdTb[fd] != NULL)
	{
		return	-5;
	}
	hand	= (t_epollhand *)calloc(1,sizeof(t_epollhand));
	if	(!hand)
	{
		return	-6;
	}

	hand->hd_magic		= RTL_EPHD_MAGIC;
	hand->hd_fd		= fd;
	hand->hd_serial		= PHandDoSerial();
	hand->hd_fctevent	= fctevent;
	hand->hd_fctrequest	= fctrequest;
	hand->hd_ref		= ref;
	hand->hd_ref2		= ref2;

	memset	(&pollevt,0,sizeof(pollevt));
	pollevt.events		= POLLIN;
	pollevt.data.ptr	= (void *)hand;

	ret	= epoll_ctl(table->ep_fd,EPOLL_CTL_ADD,fd,&pollevt);
	if	(ret != 0)
	{
		free	(hand);
		return	-10;
	}

//printf	("in '%s' #fd=%d #sz=%d fd=%d hd=%p magic=%x\n",__FUNCTION__,table->ep_nbFd,table->ep_szTb,fd,hand,hand->hd_magic);
	table->ep_hdTb[fd]	= hand;
	table->ep_nbFd++;
	return	fd;
}

/*
 *
 *	Retourne le handle d'1 fd
 *
 */

void	*rtl_epollHandle(void *ptbl, int fd)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;

	if	(fd < 0 || fd >= table->ep_szTb)
		return	NULL;
	if	((hand = table->ep_hdTb[fd]) == NULL)
		return	NULL;
	if	(hand->hd_fd != fd)
		return	NULL;

	return	hand;
}

void	*rtl_epollHandle2(void *ptbl, int pos)	/* TS */
{
	return	rtl_epollHandle2(ptbl,pos);
}

unsigned int rtl_epollSerial(void *ptbl, int fd)
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;

	if	(fd < 0 || fd >= table->ep_szTb)
		return	0;
	if	((hand = table->ep_hdTb[fd]) == NULL)
		return	0;
	if	(hand->hd_fd != fd)
		return	0;

	return	hand->hd_serial;
}


/*
 *
 *	Change le mode de polling
 *
 */


int	rtl_epollSetEvt(void *ptbl, int fd,int mode)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;
	t_epollevt	pollevt;
	int		ret;

	if	(fd < 0 || fd >= table->ep_szTb)
		return	-1;
	if	((hand = table->ep_hdTb[fd]) == NULL)
		return	-2;
	if	(hand->hd_fd != fd)
		return	-3;

	memset	(&pollevt,0,sizeof(pollevt));
	pollevt.events		= mode & (POLLIN|POLLOUT|POLLPRI|POLLERR); 
	pollevt.data.ptr	= (void *)hand;

	ret	= epoll_ctl(table->ep_fd,EPOLL_CTL_MOD,fd,&pollevt);
	if	(ret != 0)
	{
		return	-10;
	}

	hand->hd_revents	= pollevt.events;

	return 0;
}

int	rtl_epollSetEvt2(void *ptbl, int pos,int mode)	/* TS */
{
	return	rtl_epollSetEvt(ptbl,pos,mode);
}


/*
 *
 *	Retourne les events de polling
 *
 */

int	rtl_epollGetEvt(void *ptbl, int fd)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;

	if	(fd < 0 || fd >= table->ep_szTb)
		return	-1;
	if	((hand = table->ep_hdTb[fd]) == NULL)
		return	-2;
	if	(hand->hd_fd != fd)
		return	-3;

	return	hand->hd_events;
}

int	rtl_epollGetEvt2(void *ptbl, int pos)	/* TS */
{
	return	rtl_epollGetEvt(ptbl,pos);
}

/*
 *
 *	Retourne le mode de polling
 *
 */

int	rtl_epollGetMode(void *ptbl, int fd)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;

	if	(fd < 0 || fd >= table->ep_szTb)
		return	-1;
	if	((hand = table->ep_hdTb[fd]) == NULL)
		return	-2;
	if	(hand->hd_fd != fd)
		return	-3;

	return	hand->hd_revents;
}

int	rtl_epollGetMode2(void *ptbl, int pos)	/* TS */
{
	return	rtl_epollGetMode(ptbl,pos);
}


/*
 *
 *	Retire un file-descriptor de la liste
 *
 */

int	rtl_epollRmv(void *ptbl, int fd)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;
	t_epollevt	pollevt;
	int		ret;

	if	(fd < 0 || fd >= table->ep_szTb)
		return	-1;
	if	((hand = table->ep_hdTb[fd]) == NULL)
		return	-2;
	if	(hand->hd_fd != fd)
		return	-3;

	memset	(&pollevt,0,sizeof(pollevt));
	ret	= epoll_ctl(table->ep_fd,EPOLL_CTL_DEL,fd,&pollevt);
	if	(ret != 0)
	{
	}

//printf	("in '%s' #fd=%d #sz=%d fd=%d hd=%p magic=%x\n",__FUNCTION__,table->ep_nbFd,table->ep_szTb,fd,hand,hand->hd_magic);
	hand->hd_magic	= 0;
	free	(hand);
	table->ep_hdTb[fd]	= NULL;
	table->ep_nbFd--;
	return	0;
}

/*
 *
 *	Scrutation des file-descriptors et appel des procedures
 *
 */

static int rtl_epollRequest(t_epolltable *table)	/* TS */
{
	int	i;
	int	nb=0;
	int	ret;

	for (i=0; i<table->ep_szTb; i++)
	{
		if (table->ep_hdTb[i] && table->ep_hdTb[i]->hd_fctrequest)
		{
			ret =	table->ep_hdTb[i]->hd_fctrequest(table,
					i,
					table->ep_hdTb[i]->hd_ref,
					table->ep_hdTb[i]->hd_ref2,
					0);
			if (ret>=0)
			{
				rtl_epollSetEvt(table,i,ret);
			}
			nb++;
		}
	}
	return nb;
}

static int rtl_epollScan(t_epolltable *table,int resetevent,t_epollevt events[],int rc)	/* TS */
{
	int	i;
	int	nb=0;

	for	(i = 0 ; i < rc ; i++)
	{
		uint32_t	evt;
		t_epollhand	*hand	= NULL;

		evt	= events[i].events;
		hand	= (t_epollhand *)events[i].data.ptr;

		hand->hd_events	= evt;
		hand->hd_fctevent(table,hand->hd_fd,hand->hd_ref,hand->hd_ref2,evt);

		nb++;
	}
	return	nb;
}

// !!! same behaviour as rtl_poll() we call rtl_epollRequest()	!!!  
// !!! so hd_fctrequest() call back are called/used		!!!
// !!! It does not interest to use epoll(2) by this way		!!!
int	rtl_epoll(void *ptbl, int timeout)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	int	rc;
	t_epollevt	events[MAX_EVENTS_WAIT];


	if(table->ep_nbFd<0)
		return -1 ;

	rtl_epollRequest(table);
	rc	= epoll_wait(table->ep_fd,events,MAX_EVENTS_WAIT,timeout);

	if	(rc == 0)
	{
		return	0;
	}

	if	(rc < 0)
	{
		if	(errno == EINVAL)
		{
			return -EINVAL;
		}
		return -1;
	}
	return rtl_epollScan(table,1,events,rc);
}

// !!! note that here we dont call rtl_epollRequest()	!!!
// !!! so hd_fctrequest() call back are not called/used	!!!
// !!! so hd_fctevent() must be coded differently	!!!
// !!! this is the right way to use epoll(2)		!!!
int	rtl_epollRaw(void *ptbl, int timeout)	/* TS */
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	int	rc;
	t_epollevt	events[MAX_EVENTS_WAIT];


	if(table->ep_nbFd<0)
		return -1 ;

	rc	= epoll_wait(table->ep_fd,events,MAX_EVENTS_WAIT,timeout);

	if	(rc == 0)
	{
		return	0;
	}

	if	(rc < 0)
	{
		if	(errno == EINVAL)
		{
			return -EINVAL;
		}
		return -1;
	}
	return rtl_epollScan(table,0,events,rc);
}



int rtl_epollWalk(void *ptbl,int per,int (*fctwalk)(time_t now,void *ptbl,void *hand))
{
	t_epolltable	*table	= (t_epolltable *)ptbl;
	t_epollhand	*hand;
	int		i;
	int		idx;
	int		max;
	int		nb	= 0;
	int		ret	= 0;
	time_t		now;

	if	(!ptbl)
		return	-1;

	if	(table->ep_szTb <= 0)
		return	0;

	if	(per <= 0)
		per	= 10;	// 10 per cent

	if	(per > 100)
		per	= 100;

	max	= table->ep_szTb / 100;
	if	(max <= 0)
		max	= per;

	rtl_timemono(&now);
	for	(i = 0 ; i < table->ep_szTb ; i++)
	{
		idx	= table->ep_walkrr % table->ep_szTb;
		table->ep_walkrr++;

		hand	= table->ep_hdTb[idx];
		if	(hand == NULL)	continue;

		if	(!hand->hd_ref || !hand->hd_ref2)
			continue;

		ret	= (*fctwalk)(now,ptbl,hand);
		if	(ret < 0)
			break;
		nb++;
		if	(nb >= max)
			break;
	}
	return	nb;
}
