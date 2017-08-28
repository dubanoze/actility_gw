
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

#ifndef	RTL_EPOLL_H
#define	RTL_EPOLL_H

#include	<sys/epoll.h>

#define	RTL_EPTB_MAGIC	0x740411
#define	RTL_EPHD_MAGIC	0x960210

typedef struct epollhand
{
	unsigned int	hd_magic;
	int		hd_fd;
	unsigned int	hd_serial;
	int		(*hd_fctevent)();
	int		(*hd_fctrequest)();	// too bad when using epoll ...
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
	unsigned int	ep_walkrr;
} t_epolltable;


/* epoll.c */
void *rtl_epollInit(void);
int rtl_epollAdd(void *ptbl, int fd, int (*fctevent)(void *, int, void *, void *, int), int (*fctrequest)(void *, int, void *, void *, int), void *ref, void *ref2);
void *rtl_epollHandle(void *ptbl, int fd);
void *rtl_epollHandle2(void *ptbl, int pos);
unsigned int rtl_epollSerial(void *ptbl, int fd);
int rtl_epollSetEvt(void *ptbl, int fd, int mode);
int rtl_epollSetEvt2(void *ptbl, int pos, int mode);
int rtl_epollGetEvt(void *ptbl, int fd);
int rtl_epollGetEvt2(void *ptbl, int pos);
int rtl_epollGetMode(void *ptbl, int fd);
int rtl_epollGetMode2(void *ptbl, int pos);
int rtl_epollRmv(void *ptbl, int fd);
int rtl_epoll(void *ptbl, int timeout);
int rtl_epollRaw(void *ptbl, int timeout);

int rtl_epollWalk(void *ptbl,int per,int (*fctwalk)(time_t now,void *ptbl,void *hand));

#endif
