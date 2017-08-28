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

/*! @file main.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
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

#include "define.h"
#include "struct.h"



void	AvDvUiClear(t_avdv_ui *ad)
{
	memset	(ad,0,sizeof(t_avdv_ui));
}

void	AvDvUiAdd(t_avdv_ui *ad,uint32_t val,time_t when)
{
	if	(!ad || !when)
		return;
	ad->ad_time[ad->ad_slot % AVDV_NBELEM]	= when;
	ad->ad_hist[ad->ad_slot % AVDV_NBELEM]	= val;
	ad->ad_slot++;

	if	(val > ad->ad_vmax)
	{
		ad->ad_vmax	= val;
		ad->ad_tmax	= when;
	}
}

int	AvDvUiCompute(t_avdv_ui *ad,time_t tmax,time_t when)
{
	int		i;
	int		nb;
	uint32_t	histo[AVDV_NBELEM];
	uint32_t	total;
	int32_t		diff;

	ad->ad_vmax	= 0;
	ad->ad_tmax	= 0;

	nb		= 0;
	total		= 0;
	for	(i = 0 ; i < AVDV_NBELEM && ad->ad_time[i] ; i++)
	{
		if	(tmax && when && ABS(when - ad->ad_time[i]) > tmax)
			continue;
		histo[nb]	= ad->ad_hist[i];
		total		= total + histo[nb];
		if	(histo[nb] > ad->ad_vmax)
		{
			ad->ad_vmax	= ad->ad_hist[i];
			ad->ad_tmax	= ad->ad_time[i];
		}
		nb++;
	}
	if	(nb <= 0)
	{
		ad->ad_aver	= 0;
		ad->ad_sdev	= 0;
		return	nb;
	}

	ad->ad_aver	= total	/ nb;

	if	(nb <= 1)
	{
		ad->ad_sdev	= 0;
		return	nb;
	}

	total	= 0;
	for	(i = 0 ; i < nb ; i++)
	{
		diff	= histo[i] - ad->ad_aver;
		total	= total + (diff * diff);
	}

	total	= total / nb;
	ad->ad_sdev	= (uint32_t)sqrt((double)total);

	return	nb;
}


