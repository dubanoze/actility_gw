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

/*! @file crcmain.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <ctype.h>
#ifndef MACOSX
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
#include "rtlimsg.h"
#include "rtllist.h"
#include "rtlhtbl.h"

typedef unsigned char u8;
typedef unsigned short u16;
u16 crc_ccitt(u16 crc, const u8 *buffer, int len);

int	main(int argc,char *argv[])
{
	u_char	binbuff[1024];
	u_short	crcccitt;
	int	szm;

	if	(argc <= 1)
	{
		printf	("%s hexdata\n",argv[0]);
		exit(1);
	}

	szm     = sizeof(binbuff);
	rtl_strToBin(argv[1],binbuff,&szm);

	if	(szm <= 0)
	{
		exit(1);
	}
	crcccitt	= crc_ccitt(0,binbuff,szm);

	printf	("0x%04x\n",crcccitt);
	exit(0);

}
