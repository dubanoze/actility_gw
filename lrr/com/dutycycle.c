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

/*! @file dutycycle.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include <ctype.h>
#ifndef MACOSX
#include <malloc.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "rtlbase.h"

#include "define.h"
#include "infrastruct.h"

#ifdef LP_TP31
#define	DC_LISTDURATION	3600	// in seconds

#define	DC_COEFF	10000	// tmoa us (10^6) in us and %% (100)

#define DC_ADDELEM	1
#define DC_DELELEM	2

extern	int	TraceLevel;

// store information about a downlink msg
typedef struct s_dc_elem
{
	float	tmoa;		// time on air
	u_char	antenna;	// antenna that received the pkt
	u_char	channel;	// logical channel
	u_char	subband;	// subband
	time_t	expires;	// expires time
	struct s_dc_elem	*next;
} t_dc_elem;

// tmoa for one channel
typedef struct s_dc_countchan
{
	u_char	channel;
	float	tmoaup;
	float	tmoadown;
	struct s_dc_countsub *sub;
	struct s_dc_countchan *next;
} t_dc_countchan;

// tmoa for one subband
typedef struct s_dc_countsub
{
	u_char	subband;
	float	tmoaup;
	float	tmoadown;
	struct s_dc_countsub *next;
} t_dc_countsub;

// store tmoa informations relative to one antenna
typedef struct s_dc_countant
{
	u_char	antenna;
	struct s_dc_countchan *chans;
	struct s_dc_countsub *subs;
	struct s_dc_countant *next;
} t_dc_countant;

static t_dc_elem *DcListUp;		// Uplink elem list
static t_dc_elem *DcListUpLast;		// last elem of the list
static t_dc_elem *DcListDown;		// Downlink elem list
static t_dc_elem *DcListDownLast;	// last elem of the list
static void *DcCounters;		// tmoa list
static int DcMemUsed;			// memory used with malloc done here
static int DcIsInitialized=0;		// Does DcInit has been called ?

// Protos public
void DcCheckLists();
int DcSearchCounters(u_char ant, u_char chan, u_char sub, float *tcu, float *tsu, float *tcd, float *tsd);
int DcTreatUplink(t_lrr_pkt *pkt);
int DcTreatDownlink(t_lrr_pkt *pkt);

// Protos static
static void DcCheckList(time_t now, t_dc_elem **list, t_dc_elem **last);
static void DcDumpCounters();
static int DcInit();
static t_dc_countchan *DcUpdateCounters(t_dc_elem **list, t_dc_elem *elem, int add);
static t_dc_countant *DcNewCountAnt(u_char antenna);
static t_dc_countchan *DcNewCountChan(u_char channel);
static t_dc_countsub *DcNewCountSub(u_char subband);
static t_dc_elem *DcNewElem(t_lrr_pkt *pkt);
static t_dc_countchan *DcSearchCounter(u_char antenna, u_char channel, u_char subband, int create);

// Check a list to clean old msg if required
static void DcCheckList(time_t now, t_dc_elem **list, t_dc_elem **last)
{
	t_dc_elem	*elem;

	elem = *list;
	while (elem && elem->expires < now)
	{
		// set list start to following elem
		*list = elem->next;
		if (*last == elem)
			*last = NULL;
		RTL_TRDBG(3,"DcCheckList: remove elem=0x%x\n", elem);
		DcUpdateCounters(list, elem, DC_DELELEM);
		free(elem);
		DcMemUsed -= sizeof(t_dc_elem);
		elem = *list;
	}

}

// Check uplink and downlink lists to clean old msg if required
void DcCheckLists()
{
	time_t		now;

	now = rtl_timemono(NULL);
	DcCheckList(now, &DcListDown, &DcListDownLast);
	DcCheckList(now, &DcListUp, &DcListUpLast);
}

// Dump all counters
static void DcDumpCounters()
{
	t_dc_countant	*ca;
	t_dc_countchan	*cc;
	t_dc_countsub	*cs;

	if	(TraceLevel < 3)
		return;
	
	RTL_TRDBG(3,"DcDump: memory used=%d\n", DcMemUsed);

	ca = DcCounters;
	while (ca)
	{
		RTL_TRDBG(3,"  antenna %d\n", ca->antenna);

		cc = ca->chans;
		while (cc)
		{
			RTL_TRDBG(3,"    channel %d\n", cc->channel);
			RTL_TRDBG(3,"      tmoaup %f\n", cc->tmoaup);
			RTL_TRDBG(3,"      tmoadown %f\n", cc->tmoadown);
			RTL_TRDBG(3,"      sub %d (0x%lx)\n", cc->sub->subband, cc->sub);
			cc = cc->next;
		}
		cs = ca->subs;
		while (cs)
		{
			RTL_TRDBG(3,"    subband %d (0x%lx)\n", cs->subband, cs);
			RTL_TRDBG(3,"      tmoaup %f\n", cs->tmoaup);
			RTL_TRDBG(3,"      tmoadown %f\n", cs->tmoadown);
			cs = cs->next;
		}
		ca = ca->next;
	}
}

// Save all counters
void DcSaveCounters(FILE *f)
{
	t_dc_countant	*ca;
	t_dc_countchan	*cc;
	t_dc_countsub	*cs;
	
	ca = DcCounters;
	while (ca)
	{
		cc = ca->chans;
		while (cc)
		{
			fprintf(f,"ant=%d chan=%03d up=%f dn=%f\n",
				ca->antenna,cc->channel,
				cc->tmoaup/(DC_LISTDURATION*DC_COEFF),
				cc->tmoadown/(DC_LISTDURATION*DC_COEFF));
			cc = cc->next;
		}
		fflush(f);
		ca = ca->next;
	}

	ca = DcCounters;
	while (ca)
	{
		cs = ca->subs;
		while (cs)
		{
			fprintf(f,"ant=%d subb=%03d up=%f dn=%f\n",
				ca->antenna,cs->subband,
				cs->tmoaup/(DC_LISTDURATION*DC_COEFF),
				cs->tmoadown/(DC_LISTDURATION*DC_COEFF));
			cs = cs->next;
		}
		fflush(f);
		ca = ca->next;
	}
	fflush(f);
}

// Walk all counters channels first then subbands
void DcWalkCounters(void *f,void (*fct)(void *pf,int type,int ant,int idx,
                        float up,float dn))
{
	t_dc_countant	*ca;
	t_dc_countchan	*cc;
	t_dc_countsub	*cs;
	
	ca = DcCounters;
	while (ca)
	{
		cc = ca->chans;
		while (cc)
		{
//			fprintf(f,"ant=%d chan=%03d up=%f dn=%f\n",
			(*fct)(f,'C',
				ca->antenna,cc->channel,
				cc->tmoaup/(DC_LISTDURATION*DC_COEFF),
				cc->tmoadown/(DC_LISTDURATION*DC_COEFF));
			cc = cc->next;
		}
		ca = ca->next;
	}

	ca = DcCounters;
	while (ca)
	{
		cs = ca->subs;
		while (cs)
		{
//			fprintf(f,"ant=%d subb=%03d up=%f dn=%f\n",
			(*fct)(f,'S',
				ca->antenna,cs->subband,
				cs->tmoaup/(DC_LISTDURATION*DC_COEFF),
				cs->tmoadown/(DC_LISTDURATION*DC_COEFF));
			cs = cs->next;
		}
		ca = ca->next;
	}
}

// Walk all counters channels
void DcWalkChanCounters(void *f,void (*fct)(void *pf,int ant,int c,int s,
                        float up,float dn,float upsub,float dnsub))
{
	t_dc_countant	*ca;
	t_dc_countchan	*cc;
	
	ca = DcCounters;
	while (ca)
	{
		cc = ca->chans;
		while (cc)
		{
			if (cc->sub)
			{
				(*fct)(f,
				ca->antenna,cc->channel,cc->sub->subband,
				cc->tmoaup/(DC_LISTDURATION*DC_COEFF),
				cc->tmoadown/(DC_LISTDURATION*DC_COEFF),
				cc->sub->tmoaup/(DC_LISTDURATION*DC_COEFF),
				cc->sub->tmoadown/(DC_LISTDURATION*DC_COEFF));
			}
			cc = cc->next;
		}
		ca = ca->next;
	}
}

// Initialization
static int DcInit()
{
	DcCounters = NULL;
	DcListDown = NULL;
	DcListDownLast = NULL;
	DcListUp = NULL;
	DcListUpLast = NULL;
	DcIsInitialized = 1;
	return 0;
}

// Create new antenna counter
static t_dc_countant *DcNewCountAnt(u_char antenna)
{
	t_dc_countant	*ca;

	ca = (t_dc_countant *) malloc(sizeof(t_dc_countant));
	if (!ca)
	{
		RTL_TRDBG(0,"DcNewCountAnt: cannot alloc counter !\n");
		return NULL;
	}
	DcMemUsed += sizeof(t_dc_countant);
	ca->antenna = antenna;
	ca->chans = NULL;
	ca->subs = NULL;
	ca->next = NULL;
	return ca;
}

// Create new chan counter
static t_dc_countchan *DcNewCountChan(u_char channel)
{
	t_dc_countchan	*cc;

	cc = (t_dc_countchan *) malloc(sizeof(t_dc_countchan));
	if (!cc)
	{
		RTL_TRDBG(0,"DcNewCountChan: cannot alloc counter !\n");
		return NULL;
	}
	DcMemUsed += sizeof(t_dc_countchan);
	cc->channel = channel;
	cc->tmoaup = 0;
	cc->tmoadown = 0;
	cc->sub = NULL;
	cc->next = NULL;
	return cc;
}

// Create new subband counter
static t_dc_countsub *DcNewCountSub(u_char subband)
{
	t_dc_countsub	*cs;

	cs = (t_dc_countsub *) malloc(sizeof(t_dc_countsub));
	if (!cs)
	{
		RTL_TRDBG(0,"DcNewCountSub: cannot alloc counter !\n");
		return NULL;
	}
	DcMemUsed += sizeof(t_dc_countsub);
	cs->subband = subband;
	cs->tmoaup = 0;
	cs->tmoadown = 0;
	cs->next = NULL;
	return cs;
}

static t_dc_elem *DcNewElem(t_lrr_pkt *pkt)
{
	t_dc_elem	*elem;

	// create elem
	elem = (t_dc_elem *) malloc(sizeof(t_dc_elem));
	if (!elem)
	{
		RTL_TRDBG(0,"DcNewElem: cannot allocate elem !\n");
		return NULL;
	}

	DcMemUsed += sizeof(t_dc_elem);

	// set elem
	elem->expires = rtl_timemono(NULL)+DC_LISTDURATION;
	elem->tmoa = pkt->lp_tmoa;
	elem->antenna = pkt->lp_chain>>4;
	elem->channel = pkt->lp_channel;
	elem->subband = pkt->lp_subband;
	elem->next = NULL;
	RTL_TRDBG(3,"DcNewElem: new elem = 0x%lx, ant=%d, chan=%d, sub=%d, tmoa=%f\n",
		elem, elem->antenna, elem->channel, elem->subband, elem->tmoa);

	return elem;
}

// Search for a channel counter, create it if it doesn't exist
static t_dc_countchan *DcSearchCounter(u_char antenna, u_char channel, u_char subband, int create)
{
	t_dc_countant	*ca, *lastca;
	t_dc_countchan	*cc, *lastcc;
	t_dc_countsub	*cs, *lastcs;
	
	// search antenna
	ca = DcCounters;
	lastca = NULL;
	while (ca)
	{
		if (ca->antenna == antenna)
			break;
		lastca = ca;
		ca = ca->next;
	}
	
	// not found and must not be created
	if (!ca && !create)
		return NULL;

	// if no antenna found create one
	if (!ca)
	{
		ca = DcNewCountAnt(antenna);
		if (!ca)
			return NULL;

		// add it in the list
		if (lastca)
			lastca->next = ca;
		if (!DcCounters)
			DcCounters = ca;
	}

	// search channel
	cc = ca->chans;
	lastcc = NULL;
	while (cc)
	{
		if (cc->channel == channel)
			break;
		lastcc = cc;
		cc = cc->next;
	}
	
	// not found and must not be created
	if (!cc && !create)
		return NULL;

	// if no chan found create one
	if (!cc)
	{
		cc = DcNewCountChan(channel);
		if (!cc)
			return NULL;

		// add it in the list
		if (lastcc)
			lastcc->next = cc;
		if (!ca->chans)
			ca->chans = cc;

		// search subband
		cs = ca->subs;
		lastcs = NULL;
		while (cs)
		{
			if (cs->subband == subband)
				break;
			lastcs = cs;
			cs = cs->next;
		}

		// if no subband found create one
		if (!cs)
		{
			cs = DcNewCountSub(subband);
			if (!cs)
				return NULL;

			// add it in the list
			if (lastcs)
				lastcs->next = cs;
			if (!ca->subs)
				ca->subs = cs;

		}

		// set subband to channel
		cc->sub = cs;
	}

	return cc;
}

// Search for a channel counter, return tmoas
// tcu: tmoa for channel in uplink
// tsu: tmoa for subband in uplink
// tcd: tmoa for channel in downlink
// tsd: tmoa for subband in downlink
int DcSearchCounters(u_char ant, u_char chan, u_char sub, float *tcu, float *tsu, float *tcd, float *tsd)
{
	t_dc_countchan	*cc;

	cc = DcSearchCounter(ant, chan, sub, 0);
	if (!cc)
		return -1;

	if (tcu)
		*tcu = cc->tmoaup/(DC_LISTDURATION*DC_COEFF);
	if (tsu)
		*tsu = cc->sub->tmoaup/(DC_LISTDURATION*DC_COEFF);
	if (tcd)
		*tcd = cc->tmoadown/(DC_LISTDURATION*DC_COEFF);
	if (tsd)
		*tsd = cc->sub->tmoadown/(DC_LISTDURATION*DC_COEFF);

	return 0;
}

// Treat downlink packets
int DcTreatDownlink(t_lrr_pkt *pkt)
{
	t_dc_elem	*elem;
	t_dc_countchan	*cc;

	if (!pkt)
		return -1;

	if (!DcIsInitialized)
		DcInit();

	elem = DcNewElem(pkt);
	if (!elem)
		return -1;

	// Add elem in List

	// if the List is empty
	if (!DcListDown)
	{
		DcListDown = elem;
		DcListDownLast = elem;
	}
	else
	{
		DcListDownLast->next = elem;
		DcListDownLast = elem;
	}

	// update counters
	cc = DcUpdateCounters(&DcListDown, elem, DC_ADDELEM);

	if (cc)
	{
		RTL_TRDBG(3,"DcTreatDownlink: ant%d chan%d sub%d tmoachanup=%f tmoachandown=%f tmoasubup=%f tmoasubdown=%f\n",
			elem->antenna, elem->channel, elem->subband, cc->tmoaup,
			cc->tmoadown, cc->sub->tmoaup, cc->sub->tmoadown);
#ifdef LP_TP31
		pkt->lp_flag	= pkt->lp_flag | LP_RADIO_PKT_DTC;
		pkt->lp_u.lp_sent_indic.lr_dtc_ud.lr_dtcchannelup = 
				cc->tmoaup/(DC_LISTDURATION*DC_COEFF);
		pkt->lp_u.lp_sent_indic.lr_dtc_ud.lr_dtcsubbandup = 
				cc->sub->tmoaup/(DC_LISTDURATION*DC_COEFF);
		pkt->lp_u.lp_sent_indic.lr_dtc_ud.lr_dtcchanneldn = 
				cc->tmoadown/(DC_LISTDURATION*DC_COEFF);
		pkt->lp_u.lp_sent_indic.lr_dtc_ud.lr_dtcsubbanddn = 
				cc->sub->tmoadown/(DC_LISTDURATION*DC_COEFF);
#endif
	}

	DcDumpCounters();

	return 0;
}

// Set uplink packet with dutycycle info
int DcTreatUplink(t_lrr_pkt *pkt)
{
	t_dc_countchan	*cc;
	t_dc_elem	*elem;

	if (!pkt)
		return -1;

	if (!DcIsInitialized)
		DcInit();

	elem = DcNewElem(pkt);
	if (!elem)
		return -1;

	// Add elem in List

	// if the List is empty
	if (!DcListUp)
	{
		DcListUp = elem;
		DcListUpLast = elem;
	}
	else
	{
		DcListUpLast->next = elem;
		DcListUpLast = elem;
	}

	// update counters
	cc = DcUpdateCounters(&DcListUp, elem, DC_ADDELEM);

	if (cc)
	{
		RTL_TRDBG(3,"DcTreatUplink: ant%d chan%d sub%d tmoachanup=%f tmoachandown=%f tmoasubup=%f tmoasubdown=%f\n",
			elem->antenna, elem->channel, elem->subband, cc->tmoaup,
			cc->tmoadown, cc->sub->tmoaup, cc->sub->tmoadown);
#ifdef LP_TP31
		pkt->lp_flag	= pkt->lp_flag | LP_RADIO_PKT_DTC;
		pkt->lp_u.lp_radio.lr_u2.lr_dtc_ud.lr_dtcchannelup = 
				cc->tmoaup/(DC_LISTDURATION*DC_COEFF);
		pkt->lp_u.lp_radio.lr_u2.lr_dtc_ud.lr_dtcsubbandup = 
				cc->sub->tmoaup/(DC_LISTDURATION*DC_COEFF);
		pkt->lp_u.lp_radio.lr_u2.lr_dtc_ud.lr_dtcchanneldn = 
				cc->tmoadown/(DC_LISTDURATION*DC_COEFF);
		pkt->lp_u.lp_radio.lr_u2.lr_dtc_ud.lr_dtcsubbanddn = 
				cc->sub->tmoadown/(DC_LISTDURATION*DC_COEFF);
#endif
	}
	else
	{
		RTL_TRDBG(3,"DcTreatUplink: no counter, should be impossible !!!\n");
	}
	return 0;
}

// Update counters when adding or removing a elem in the list
static t_dc_countchan *DcUpdateCounters(t_dc_elem **list, t_dc_elem *elem, int add)
{
	t_dc_countchan	*cc;

	if (!elem)
		return NULL;

	// search counter. Create it if it doesn't exist
	cc = DcSearchCounter(elem->antenna, elem->channel, elem->subband, (add == DC_ADDELEM));
	if (!cc)
		return NULL;

	// update tmoa
	if (list == &DcListUp)
	{
		if (add == DC_ADDELEM)
		{
			cc->tmoaup += elem->tmoa;
			cc->sub->tmoaup += elem->tmoa;
		}
		else
		{
			cc->tmoaup -= elem->tmoa;
			cc->sub->tmoaup -= elem->tmoa;
		}
		RTL_TRDBG(3,"DcUpdateCounters: new tmoas ant%d chan%d sub%d tmoachanup=%f (%.03f%%) tmoasubup=%f (%.03f%%)\n", 
			elem->antenna, cc->channel, cc->sub->subband, cc->tmoaup,
			cc->tmoaup/(DC_LISTDURATION*DC_COEFF), cc->sub->tmoaup,
			cc->sub->tmoaup/(DC_LISTDURATION*DC_COEFF));
	}
	else
	{
		if (add == DC_ADDELEM)
		{
			cc->tmoadown += elem->tmoa;
			cc->sub->tmoadown += elem->tmoa;
		}
		else
		{
			cc->tmoadown -= elem->tmoa;
			cc->sub->tmoadown -= elem->tmoa;
		}
		RTL_TRDBG(3,"DcUpdateCounters: new tmoas ant%d chan%d sub%d tmoachandown=%f (%.03f%%) tmoasubdown=%f (%.03f%%)\n", 
			elem->antenna, cc->channel, cc->sub->subband, cc->tmoadown,
			cc->tmoadown/(DC_LISTDURATION*DC_COEFF), cc->sub->tmoadown,
			cc->sub->tmoadown/(DC_LISTDURATION*DC_COEFF));
	}

	return cc;
}

#endif
