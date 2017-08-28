
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

/* Copyright (c) 2012 the authors listed at the following URL, and/or
 * the authors of referenced articles or incorporated external code:
 * http://en.literateprograms.org/Hash_table_(C)?action=history&offset=20100620072342
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Retrieved from: http://en.literateprograms.org/Hash_table_(C)?oldid=16749
 * */


#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "rtlhtbl.h"


#define	DATA_INCLUDE_KEY(h) ( (h)->offset >= 0 && \
	((h)->flags & HTBL_KEY_INCLUDED) == HTBL_KEY_INCLUDED )

struct hnode_s 
{
	void *key;	// strdup or uint or data+offset
	void *data;
	struct hnode_s *next;
};

typedef struct htbl_s 
{
	pthread_mutex_t	hCS;
	unsigned int flags;
	int offset;	// < 0 if key not included in data
	unsigned int size;
	unsigned int count;
	struct hnode_s **entries;
	unsigned int (*hashfunc)(void *);
	unsigned int (*compfunc)(struct htbl_s *,struct hnode_s *,void *);
	void (*rmovfunc)(struct htbl_s *,char *,void *);
	void (*updatefunc)(struct htbl_s *,char *,void *,void *);
#ifdef	PTHREAD_MUTEX_RECURSIVE
	pthread_mutexattr_t hCSAttr;
#endif
	pthread_t locker;
	char lockerset;
} htbl_t;


static	void	InitCS(htbl_t *h)
{
	if	( (h->flags & HTBL_USE_MUTEX) == HTBL_USE_MUTEX )
	{
#ifdef	PTHREAD_MUTEX_RECURSIVE
		pthread_mutexattr_init(&h->hCSAttr);
		pthread_mutexattr_settype(&h->hCSAttr,
						PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&h->hCS,&h->hCSAttr);
#else
		pthread_mutex_init(&h->hCS,NULL);
#endif
	}
}

static	void	EnterCS(htbl_t *h)
{
	if	( (h->flags & HTBL_USE_MUTEX) == HTBL_USE_MUTEX )
	{
		pthread_mutex_lock(&h->hCS);
		h->locker	= pthread_self();
		h->lockerset	= 1;
	}
}

static	void	LeaveCS(htbl_t *h)
{
	if	( (h->flags & HTBL_USE_MUTEX) == HTBL_USE_MUTEX )
	{
		h->lockerset	= 0;
		pthread_mutex_unlock(&h->hCS);
	}
}


static unsigned int def_hashfunc(void *key)
{
	unsigned int hash=0;
	unsigned char *k = (unsigned char *)key;
	
	while(*k) hash+=*k++;

	return hash;
}

static unsigned int uint_hashfunc(void *key)
{
	return (unsigned int)key;
}

static unsigned int def_compfunc(htbl_t *h,struct hnode_s *node,void *key)
{
	if	( (h->flags & HTBL_KEY_STRING) == HTBL_KEY_STRING )
		return	strcmp(node->key,key);

	if	( (h->flags & HTBL_KEY_UINT) == HTBL_KEY_UINT )
		return	(unsigned int)node->key != (unsigned int)key;


	printf	("fatal error %s:%s:%c\n",__FILE__,__func__,__LINE__);
	abort();
	return	1;
}

static void updatedata(htbl_t *h,struct hnode_s *node, void *newdata)
{
	if	(h->updatefunc && node->data)
	{
		h->updatefunc(h,node->key,node->data,newdata);
		// we assume callback has freed old user data
		node->data	= newdata;
		return;
	}
	if	(h->rmovfunc && node->data)
	{
		h->rmovfunc(h,node->key,node->data);
		// we assume callback has freed user data
		node->data	= newdata;
		return;
	}
	if	( (h->flags & HTBL_FREE_DATA) == HTBL_FREE_DATA )
	{
		if	(node->data)
		{
			free(node->data);
			node->data = newdata;
			return;
		}
	}
	node->data	= newdata;
}

static void freedata(htbl_t *h,struct hnode_s *node)
{
	if	(h->rmovfunc && node->data)
	{
		h->rmovfunc(h,node->key,node->data);
		// we assume callback has freed user data
		node->data	= NULL;
		return;
	}
	if	( (h->flags & HTBL_FREE_DATA) == HTBL_FREE_DATA )
	{
		if	(node->data)
		{
			free(node->data);
			node->data = NULL;
			return;
		}
	}
	node->data	= NULL;
}

static void freekey(htbl_t *h,struct hnode_s *node)
{
	if	(DATA_INCLUDE_KEY(h))
	{
		node->key = NULL;
		return;
	}
	if	( (h->flags & HTBL_KEY_STRING) == HTBL_KEY_STRING )
	{
		free(node->key);
		node->key = NULL;
		return;
	}
	node->key = NULL;
}

static void freekeydata(htbl_t *h,struct hnode_s *node)
{
	freedata(h,node);
	freekey(h,node);
}

static	htbl_t *htbl_create(unsigned int size, unsigned int (*hashfunc)(void *),int flags)
{
	htbl_t *htbl;

	if(!(htbl=malloc(sizeof(htbl_t)))) 
		return NULL;
	memset(htbl,0,sizeof(htbl_t));

	if(!(htbl->entries=calloc(size, sizeof(struct hnode_s*)))) 
	{
		free(htbl);
		return NULL;
	}

	htbl->flags	= HTBL_KEY_STRING;
	htbl->size	= size;
	htbl->offset	= -1;

	if(hashfunc) 
		htbl->hashfunc=hashfunc;
	else 
		htbl->hashfunc=def_hashfunc;

	htbl->compfunc=def_compfunc;

	htbl->flags	= flags;
	if	( !hashfunc && (htbl->flags & HTBL_KEY_UINT) == HTBL_KEY_UINT )
	{
		htbl->hashfunc	= uint_hashfunc;		
	}

	InitCS(htbl);


	return htbl;
}

static	void htbl_destroy(htbl_t *htbl)
{
	unsigned int n;
	struct hnode_s *node, *oldnode;
	
	for(n=0; n<htbl->size; ++n) 
	{
		node=htbl->entries[n];
		while(node) 
		{
			oldnode=node;
			node=node->next;

			freekeydata(htbl,oldnode);

			free(oldnode);
			htbl->count--;
		}
	}
	free(htbl->entries);
	free(htbl);
}
static	void htbl_reset(htbl_t *htbl)
{
	unsigned int n;
	struct hnode_s *node, *oldnode;
	
	for(n=0; n<htbl->size; ++n) 
	{
		node=htbl->entries[n];
		while(node) 
		{
			oldnode=node;
			node=node->next;

			freekeydata(htbl,oldnode);

			free(oldnode);
			htbl->count--;
		}
		htbl->entries[n]= NULL;
		htbl->count	= 0;
	}
}

static	int htbl_insert(htbl_t *htbl, void *key, void *data, int update)
{
	struct hnode_s *node;
	unsigned int hash;

#if	0
	if	( (htbl->flags & HTBL_KEY_STRING) == HTBL_KEY_STRING )
	{
	fprintf(stderr, "htbl_insert() key='%s', hash=%d, data=%s\n", 
			key, hash, (char*)data);
	} else
	if	( (htbl->flags & HTBL_KEY_UINT) == HTBL_KEY_UINT )
	{
	fprintf(stderr, "htbl_insert() key=%u, hash=%d, data=%s\n", 
			(unsigned int)key, hash, (char*)data);
	} else
	{
		return	-10;
	}
#endif

	if	(DATA_INCLUDE_KEY(htbl))
	{
		if	(key)
		{
			return	-10;
		}
		if	((htbl->flags & HTBL_KEY_STRING) == HTBL_KEY_STRING)
		{
			key	= data + htbl->offset;
		} else
		if	((htbl->flags & HTBL_KEY_UINT) == HTBL_KEY_UINT)
		{
			unsigned int *adr = (unsigned int *)((char *)data + htbl->offset);

			key	= (void *)(*adr);
		}
	}

	hash	= htbl->hashfunc(key)%htbl->size;

	node=htbl->entries[hash];
	while(node) 
	{
		if(!htbl->compfunc(htbl,node, key)) 
		{
			if	(update == 0)
				return	-1;
			updatedata(htbl,node,data);
			node->data=data;
			if((htbl->flags & HTBL_KEY_STRING) == HTBL_KEY_STRING
				&& DATA_INCLUDE_KEY(htbl))
			{
				node->key	= (char *)data + htbl->offset;
			}
			return	1;
		}
		node=node->next;
	}

//printf	("htbl_insert() add a new entry key=%s\n",key);

	if(!(node=malloc(sizeof(struct hnode_s)))) 
		return -2;
	memset(node,0,sizeof(struct hnode_s));


	if	( (htbl->flags & HTBL_KEY_STRING) == HTBL_KEY_STRING )
	{
		if	(DATA_INCLUDE_KEY(htbl))
		{
			node->key	= (char *)data + htbl->offset;
		}
		else
		{
			if	(!(node->key=strdup(key))) 
			{
				free(node);
				return -3;
			}
		}
	} else
	if	( (htbl->flags & HTBL_KEY_UINT) == HTBL_KEY_UINT )
	{
		node->key	= (void *)key;
	} else
	{
		free(node);
		return -4;
	}

	node->data=data;
	node->next=htbl->entries[hash];
	htbl->entries[hash]=node;
	htbl->count++;

	if	(htbl->updatefunc)
	{
		htbl->updatefunc(htbl,node->key,NULL,data);
	}

	return htbl->count;
}

static	int htbl_remove(htbl_t *htbl, void *key,int internal)
{
	struct hnode_s *node, *prevnode=NULL;
	unsigned int hash=htbl->hashfunc(key)%htbl->size;

	node=htbl->entries[hash];
	while(node) 
	{
		if(!htbl->compfunc(htbl,node, key)) 
		{
			if(prevnode) 
				prevnode->next=node->next;
			else 
				htbl->entries[hash]=node->next;
			if (internal==0)
			{
				freekeydata(htbl,node);
			}

			free(node);
			htbl->count--;
			return htbl->count;
		}
		prevnode=node;
		node=node->next;
	}

	return -1;
}

static	void *htbl_get(htbl_t *htbl, void *key)
{
	struct hnode_s *node;
	unsigned int hash=htbl->hashfunc(key)%htbl->size;


	node=htbl->entries[hash];
	while(node) 
	{
		if(!htbl->compfunc(htbl,node, key)) 
			return node->data;
		node=node->next;
	}

	return NULL;
}

static	void *htbl_getDup(htbl_t *htbl, void *key, int sz)
{
	struct hnode_s *node;
	unsigned int hash=htbl->hashfunc(key)%htbl->size;


	node=htbl->entries[hash];
	while(node) 
	{
		if(!htbl->compfunc(htbl,node, key)) 
		{
			void	*pt = malloc(sz);
			if	(!pt)
				return	NULL;
			memcpy	(pt,node->data,sz);
			return pt;
		}
		node=node->next;
	}

	return NULL;
}

static	void *htbl_getCopy(htbl_t *htbl, void *key, void *dst, int sz)
{
	struct hnode_s *node;
	unsigned int hash=htbl->hashfunc(key)%htbl->size;


	node=htbl->entries[hash];
	while(node) 
	{
		if(!htbl->compfunc(htbl,node, key)) 
		{
			memcpy	(dst,node->data,sz);
			return dst;
		}
		node=node->next;
	}

	return NULL;
}

static	int htbl_resize(htbl_t *htbl, unsigned int size)
{
	htbl_t newtbl;
	unsigned int n;
	struct hnode_s *node,*next;

	newtbl.size=size;
	newtbl.hashfunc=htbl->hashfunc;
        // TODO manque les flags, offset, la cb remove ...
	//
	
	memcpy	(&newtbl,htbl,sizeof(htbl_t));

	if(!(newtbl.entries=calloc(size, sizeof(struct hnode_s*)))) 
		return -1;

	for(n=0; n<htbl->size; ++n) 
	{
		for(node=htbl->entries[n]; node; node=next) 
		{
			next = node->next;
			htbl_insert(&newtbl, node->key, node->data,0);
			htbl_remove(htbl, node->key,1);
			
		}
	}

	free(htbl->entries);
	htbl->size=newtbl.size;
	htbl->entries=newtbl.entries;

	return htbl->count;
}

// remove protected
static	void
htbl_dump(htbl_t *htbl, void (*fct)(char *key, void *data)) {
	struct hnode_s *node;
	int	i;

	for	(i=0; i<htbl->size; i++) 
	{
		for	(node=htbl->entries[i]; node; ) 
		{
			struct hnode_s *next_node;
			next_node	= node->next;
			(*fct)(node->key, node->data);
			node	= next_node;
		}
	}
}

// remove protected
static	int
htbl_walk(htbl_t *htbl, int (*fct)(void *h,char *key, void *data,void *udata),
void *udata) 
{
	struct hnode_s *node;
	int	i;
	int	ret;

	for	(i=0; i<htbl->size; i++) 
	{
		for	(node=htbl->entries[i]; node; ) 
		{
			struct hnode_s *next_node;
			next_node	= node->next;
			ret	= (*fct)((void *)htbl,node->key, node->data,
						udata);
			if	(ret)
				return	ret;
			node	= next_node;
		}
	}
	return	0;
}
static int htbl_walk_entry(htbl_t *htbl, int entry, int (*fct)(void *h,char *key, void *data,void *udata), void *udata) {
	struct hnode_s *node;
	int	ret;

	if	(entry < htbl->size) {
		for	(node=htbl->entries[entry]; node; ) {
			struct hnode_s *next_node;
			next_node	= node->next;
			ret	= (*fct)((void *)htbl,node->key, node->data,
						udata);
			if	(ret)
				return	ret;
			node	= next_node;
		}
	}
	return	0;
}

// rtl interfaces
//
void *rtl_htblNew(unsigned int size)
{
	return	(void *)htbl_create(size,NULL,HTBL_KEY_STRING);
}
void *rtl_htblCreate(unsigned int size, unsigned int (*hashfunc)(void *))
{
	return	(void *)htbl_create(size,hashfunc,HTBL_KEY_STRING);
}

void *rtl_htblCreateSpec(unsigned int size, unsigned int (*hashfunc)(void *),unsigned int flags)
{
	htbl_t	*h;

	h	= htbl_create(size,hashfunc,flags);
	return	(void *)h;
}
void rtl_htblDestroy(void *htbl)
{
	htbl_t	*h = (htbl_t *)htbl;
	htbl_t	svh;
	EnterCS(h);
	memcpy(&svh,h,sizeof(svh));
	htbl_destroy(h);
	LeaveCS(&svh);
}
void rtl_htblReset(void *htbl)
{
	htbl_t	*h = (htbl_t *)htbl;
	EnterCS(h);
	htbl_reset(h);
	LeaveCS(h);
}
int rtl_htblInsert(void *htbl, void *key, void *data)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	= htbl_insert(h,key,data,0);
	LeaveCS(h);
	return	ret;
}
int rtl_htblAdd(void *htbl, void *data)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	= htbl_insert(h,NULL,data,0);
	LeaveCS(h);
	return	ret;
}
int rtl_htblUpdateInsert(void *htbl, void *key, void *data)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	= htbl_insert(h,key,data,1);
	LeaveCS(h);
	return	ret;
}
int rtl_htblUpdateAdd(void *htbl, void *data)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	= htbl_insert(h,NULL,data,1);
	LeaveCS(h);
	return	ret;
}
int rtl_htblRemove(void *htbl, void *key)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	=  htbl_remove(h,key,0);
	LeaveCS(h);
	return	ret;
}
int rtl_htblRemoveNoLock(void *htbl, void *key)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	if	(!h->lockerset || !pthread_equal(h->locker,pthread_self()))
		return	HTBL_WRONG_THREAD;
	ret	=  htbl_remove(h,key,0);
	return	ret;
}
int rtl_htblResize(void *htbl, unsigned int size)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	= htbl_resize(h,size);
	LeaveCS(h);
	return	ret;
}
void *rtl_htblGet(void *htbl, void *key)
{
	htbl_t	*h = (htbl_t *)htbl;
	void	*ret;
	EnterCS(h);
	ret	= htbl_get(h,key);
	LeaveCS(h);
	return	ret;
}
void *rtl_htblGetDup(void *htbl, void *key, int sz,int flg)
{
	htbl_t	*h = (htbl_t *)htbl;
	void	*ret;
	EnterCS(h);
	ret	= htbl_getDup(h,key,sz);
	if	(ret && (flg & HTBL_GET_REMOVE) == HTBL_GET_REMOVE)
		htbl_remove(h,key,0);
	LeaveCS(h);
	return	ret;
}
void *rtl_htblGetCopy(void *htbl, void *key, void *dst, int sz,int flg)
{
	htbl_t	*h = (htbl_t *)htbl;
	void	*ret;
	EnterCS(h);
	ret	= htbl_getCopy(h,key,dst,sz);
	if	(ret && (flg & HTBL_GET_REMOVE) == HTBL_GET_REMOVE)
		htbl_remove(h,key,0);
	LeaveCS(h);
	return	ret;
}
void rtl_htblDump(void *htbl, void (*fct)(char *key, void *data))
{
	htbl_t	*h = (htbl_t *)htbl;
	EnterCS(h);
	htbl_dump(h,fct);
	LeaveCS(h);
}
int rtl_htblWalk(void *htbl, int (*fct)(void *h,char *key, void *data, void *udata), void *udata)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	= htbl_walk(h,fct,udata);
	LeaveCS(h);
	return	ret;
}
int rtl_htblWalkEntry(void *htbl, int entry, int (*fct)(void *h,char *key, void *data, void *udata), void *udata)
{
	htbl_t	*h = (htbl_t *)htbl;
	int	ret;
	EnterCS(h);
	ret	= htbl_walk_entry(h,entry,fct,udata);
	LeaveCS(h);
	return	ret;
}

int rtl_htblGetFlags(void *htbl)
{
	htbl_t	*h = (htbl_t *)htbl;
	return	h->flags;
}
int rtl_htblGetCount(void *htbl)
{
	htbl_t	*h = (htbl_t *)htbl;
	return	h->count;
}
int rtl_htblSetRmovFunc(void *htbl, void (*rmovfunc)(void *h,char *k,void *d))
{
	htbl_t	*h = (htbl_t *)htbl;
	h->rmovfunc	= rmovfunc;
	return	0;
}
int rtl_htblSetUpdateFunc(void *htbl, void (*updatefunc)(void *h,char *k,void *d,void *nd))
{
	htbl_t	*h = (htbl_t *)htbl;
	h->updatefunc	= updatefunc;
	return	0;
}
int rtl_htblSetKeyOffset(void *htbl, int offset)
{
	htbl_t	*h = (htbl_t *)htbl;
	if	( (h->flags & HTBL_KEY_INCLUDED) != HTBL_KEY_INCLUDED )
		return	-1;

	h->offset	= offset;
	return	0;
}
