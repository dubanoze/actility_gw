
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

#include "stdio.h"
#include "_whatstr.h"

// lock must be an address on a global variable initialized to 0
void	rtl_spin_lock_gcc(int *lock)
{
#if	0
	while (__sync_lock_test_and_set(lock,1)) 
		while (*lock);
#else
	while (__sync_lock_test_and_set(lock,1));
#endif
}

void	rtl_spin_unlock_gcc(int *lock)
{
	__sync_lock_release(lock);
}
