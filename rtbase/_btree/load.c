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
#include <sys/types.h>
#include <unistd.h>
#include "rtlbase.h"
#include "rtlbtree.h"

char *sensor = "Plug";
char *sensor_update = "Plug - update";

#define BASE 0x1100000000000000
#define MAX (4*1024*1024)

/* t1  -= t2 */
#define _SUB_TIMESPEC(t1, t2)\
  do {\
        if (((t1).tv_nsec -= (t2).tv_nsec) < 0) {\
                (t1).tv_nsec += 1000000000;\
                (t1).tv_sec  -= 1;\
        }\
        (t1).tv_sec -= (t2).tv_sec;\
  } while (0)

int	main(int argc,char *argv[]) {
	struct btree_head64 btree;
	uint64_t DevEUI = BASE;
	char *res;
	int i;
	struct	timespec t0,t1;
	unsigned int dur;

	srand(time(0)+getpid());
	btree_init64 (&btree);

	rtl_timespecmono(&t0);
	for	(i=0; i<MAX; i++) {
		btree_insert64 (&btree, DevEUI, sensor);
		DevEUI ++;
	}
	rtl_timespecmono(&t1);
	_SUB_TIMESPEC(t1,t0);
	dur	= t1.tv_sec * 1000000 + (t1.tv_nsec / 1000);	// result us
	printf ("Creation %d items : %d ms\n", MAX, dur);

	rtl_timespecmono(&t0);
	for	(i=0; i<MAX; i++) {
		DevEUI = BASE + rand()%MAX;
		res = btree_lookup64(&btree, DevEUI);
		if	(!res)
			printf ("%016llx : %s\n", DevEUI, res);
	}
	rtl_timespecmono(&t1);
	_SUB_TIMESPEC(t1,t0);
	dur	= t1.tv_sec * 1000000 + (t1.tv_nsec / 1000);	// result us
	printf ("Lookup   %d items : %d ms\n", MAX, dur);

	btree_destroy64 (&btree);

	return 0;
}
