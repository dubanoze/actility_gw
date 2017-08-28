
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


#define	EXIT(v,s,...) \
{\
			printf("line=%d %s (%d)",__LINE__,(s),(v));\
			exit((1));\
}

typedef	struct
{
	int		num;
	int		link;
	char		strkey[64];

	/* users data */
}	t_data;

t_data	TbData[100];

void	*HStr;	// hash string

int	NBELEM	= 10;
int	HASHSZ	= 3;

int	Count;

int	WalkCount(void *h,char *k,void *d,void *u)
{
	Count++;
	return	0;
}

int	WalkPrint(void *h,char *k,void *d,void *u)
{
	int	f	= rtl_htblGetFlags(h);
	t_data	*data	= (t_data *)d;

	if	( (f & HTBL_KEY_STRING) == HTBL_KEY_STRING )
	{
		printf	("k=%s d=%s\n",k,data->strkey);
		return	0;
	}
	return	0;
}

int	WalkRemoveOdd(void *h,char *k,void *d,void *u)
{
	int	f	= rtl_htblGetFlags(h);

	if	( (f & HTBL_KEY_STRING) == HTBL_KEY_STRING )
	{
		if	((atoi(k)%2))
		{
			printf	("try to remove '%s'\n",k);
			rtl_htblRemoveNoLock(h,k);
		}
	}
	return	0;
}

void	Dump(char *k,void *d)
{
	t_data	*data	= (t_data *)d;

	printf	("k=%s d=%s\n",k,data->strkey);
}

void	CbRemove(void *h,char *k,void *d)
{
	t_data	*data	= (t_data *)d;

	data->link--;
	printf	("k=%s d=%s removed (%d)\n",k,data->strkey,data->link);
}

void	*AddElem()
{
	int	i;
	char	key[128];
	t_data	*data;
	int	ret;


	for	(i = 0 ; i < NBELEM ; i++)
	{
		data		= &TbData[i];
		data->num	= i;
		data->link	= 0;
		sprintf	(data->strkey,"%08d",i);
		if	((ret=rtl_htblAdd(HStr,data)) < 0)
			EXIT(ret,"error rtl_htblInsert str\n");
		data->link++;
		printf("create node '%s'\n",data->strkey);
	}
	for	(i = 0 ; i < NBELEM ; i++)
	{
		sprintf	(key,"%08d",i);
		data	= rtl_htblGet(HStr,key);
		if	(!data)
			EXIT(1,"error rtl_htblGet\n");
		if	(strcmp(data->strkey,key) != 0)
			EXIT(1,"error strcmp\n");
		printf("retrieve node '%s'\n",data->strkey);
	}

	return	NULL;
}

int	main(int argc,char *argv[])
{
	int	ret;
	int	offset;
	char	key[128];

	printf	("--NBELEM=%d    HASHSZ=%d--\n",NBELEM,HASHSZ);

	HStr	= rtl_htblCreateSpec(HASHSZ,NULL,
			HTBL_KEY_STRING|HTBL_KEY_INCLUDED|HTBL_USE_MUTEX);
	if	(!HStr)
		EXIT(1,"error rtl_htblNew\n");
	offset	= offsetof(t_data,strkey);
	if	(rtl_htblSetKeyOffset(HStr,offset) < 0)
		EXIT(1,"error rtl_htblSetKeyOffset\n");
	rtl_htblSetRmovFunc(HStr,CbRemove);

	AddElem();

	sprintf	(key,"%08d",0);
	ret	= rtl_htblRemoveNoLock(HStr,key);
	printf("ret=%d rtl_htblRemoveNoLock()\n",ret);
	if	(ret != HTBL_WRONG_THREAD)
		EXIT(1,"error rtl_htblRemoveNoLock\n");

	printf("call rtl_htblDump()\n");
	rtl_htblDump(HStr,Dump);

	printf("call rtl_htblWalk() to count\n");
	Count	= 0;
	rtl_htblWalk(HStr,WalkCount,NULL);
	printf("count=%d\n",Count);

	printf("call rtl_htblWalk() to remove print\n");
	rtl_htblWalk(HStr,WalkPrint,NULL);

	printf("call rtl_htblWalk() to remove odd keys\n");
	rtl_htblWalk(HStr,WalkRemoveOdd,NULL);

	exit(0);
}
