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
#include "rtlbase.h"
#include "rtlbtree.h"

char *sensor = "Plug";
char *sensor_update = "Plug - update";

int	main(int argc,char *argv[]) {
	struct btree_head64 btree;
	uint64_t DevEUI = 0x110000000f1d8693;
	char *res;

	btree_init64 (&btree);
	btree_insert64 (&btree, DevEUI, sensor);
	res = btree_lookup64(&btree, DevEUI);
	printf ("res=%s\n", res);

	btree_update64 (&btree, DevEUI, sensor_update);
	res = btree_lookup64(&btree, DevEUI);
	printf ("res=%s\n", res);

	btree_remove64(&btree, DevEUI);
	res = btree_lookup64(&btree, DevEUI);
	printf ("res=%s\n", res);

	btree_destroy64 (&btree);

	return 0;
}
