
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
			printf("%d %s",__LINE__,(s));\
			exit((v));\
}

/*
#define	NBELEM	(100000)
#define	HASHSZ	(9999)
*/

typedef	struct
{
	char	key[128];
	char	data[128];
}	t_data;

int	NBELEM	= 5;
int	HASHSZ	= 11;

int	Count;

void	DataFree(void *h,char *k,void *d)
{
	printf	("free k=%s d=%s\n",k,((t_data *)d)->data);
	free(d);
}

void	DataUpdate(void *h,char *k,void *odata,void *ndata)
{
	if	(odata == NULL)	// creation
	{
		printf	("create k=%s d=%s\n",k,((t_data *)ndata)->data);
		return;
	}

	// update
	printf	("update k=%s d=%s -> d=%s\n",k,
		((t_data *)odata)->data,((t_data *)ndata)->data);
	free(odata);
}

int	main(int argc,char *argv[])
{
	void	*h;
	int	i;
	char	key[128];
	t_data	*data;
	int	offset;
	int	ret;

retry :

	printf	("--NBELEM=%d    HASHSZ=%d--\n",NBELEM,HASHSZ);

#if	1
	offset	= 0;
	h	= rtl_htblCreateSpec(HASHSZ,NULL,HTBL_KEY_STRING);
	if	(!h)
		EXIT(1,"error rtl_htblNew\n");

	rtl_htblSetRmovFunc(h,DataFree);
	rtl_htblSetUpdateFunc(h,DataUpdate);

printf("creation (key not included)\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		data	= (t_data *)malloc(sizeof(t_data));
		if	(!data)
		{
			EXIT(1,"error malloc()\n");
		}
		sprintf	(data->key,"%08d",i);
		sprintf	(data->data,"%08d",i);
		if	((ret=rtl_htblUpdateInsert(h,data->key,data)) < 0)
		{
			printf	("ret=%d i=%d\n",ret,i);
			EXIT(1,"error rtl_htblUpdateInsert\n");
		}
	}
printf("first update\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		data	= (t_data *)malloc(sizeof(t_data));
		if	(!data)
		{
			EXIT(1,"error malloc()\n");
		}
		sprintf	(data->key,"%08d",i);
		sprintf	(data->data,"%08d",i+10);
		if	((ret=rtl_htblUpdateInsert(h,data->key,data)) < 0)
		{
			printf	("ret=%d i=%d\n",ret,i);
			EXIT(1,"error rtl_htblUpdateInsert\n");
		}
	}
printf("second update\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		data	= (t_data *)malloc(sizeof(t_data));
		if	(!data)
		{
			EXIT(1,"error malloc()\n");
		}
		sprintf	(data->key,"%08d",i);
		sprintf	(data->data,"%08d",i);
		if	((ret=rtl_htblUpdateInsert(h,data->key,data)) < 0)
		{
			printf	("ret=%d i=%d\n",ret,i);
			EXIT(1,"error rtl_htblUpdateInsert\n");
		}
	}
printf("retrieve copy\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		t_data	stdata;
		sprintf	(key,"%08d",i);
		data	= rtl_htblGetCopy(h,key,&stdata,sizeof(stdata),0);
		if	(!data)
			EXIT(1,"error rtl_htblGet\n");
		if	(strcmp(data->key,data->data) != 0)
			EXIT(1,"error strcmp\n");
	}

	Count	= 0;
printf("destroy\n");
	rtl_htblDestroy(h);
#endif

#if	1
	offset	= 0;
	h	= rtl_htblCreateSpec(HASHSZ,NULL,HTBL_KEY_STRING|HTBL_KEY_INCLUDED);
	if	(!h)
		EXIT(1,"error rtl_htblNew\n");

	if	(rtl_htblSetKeyOffset(h,offset) < 0)
		EXIT(1,"error rtl_htblSetKeyOffset\n");

	rtl_htblSetRmovFunc(h,DataFree);
	rtl_htblSetUpdateFunc(h,DataUpdate);

printf("creation (key included)\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		data	= (t_data *)malloc(sizeof(t_data));
		if	(!data)
		{
			EXIT(1,"error malloc()\n");
		}
		sprintf	(data->key,"%08d",i);
		sprintf	(data->data,"%08d",i);
		if	((ret=rtl_htblUpdateAdd(h,data)) < 0)
		{
			printf	("ret=%d i=%d\n",ret,i);
			EXIT(1,"error rtl_htblUpdateAdd\n");
		}
	}
printf("first update\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		data	= (t_data *)malloc(sizeof(t_data));
		if	(!data)
		{
			EXIT(1,"error malloc()\n");
		}
		sprintf	(data->key,"%08d",i);
		sprintf	(data->data,"%08d",i+10);
		if	((ret=rtl_htblUpdateAdd(h,data)) < 0)
		{
			printf	("ret=%d i=%d\n",ret,i);
			EXIT(1,"error rtl_htblUpdateAdd\n");
		}
	}
printf("second update\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		data	= (t_data *)malloc(sizeof(t_data));
		if	(!data)
		{
			EXIT(1,"error malloc()\n");
		}
		sprintf	(data->key,"%08d",i);
		sprintf	(data->data,"%08d",i);
		if	((ret=rtl_htblUpdateAdd(h,data)) < 0)
		{
			printf	("ret=%d i=%d\n",ret,i);
			EXIT(1,"error rtl_htblUpdateAdd\n");
		}
	}
printf("retrieve copy remove\n");
	for	(i = 0 ; i < NBELEM ; i++)
	{
		t_data	stdata;
		sprintf	(key,"%08d",i);
		data	= rtl_htblGetCopy(h,key,&stdata,sizeof(stdata),HTBL_GET_REMOVE);
		if	(!data)
			EXIT(1,"error rtl_htblGet\n");
		if	(strcmp(data->key,data->data) != 0)
			EXIT(1,"error strcmp\n");
	}

	Count	= 0;
printf("destroy\n");
	rtl_htblDestroy(h);
#endif
	
	printf	("---------------------------------------------\n");

	NBELEM	= (rand() % 10 ) + 1;
	HASHSZ	= (rand() % 10 ) + 1;

//	goto	retry;

	exit(0);
}
