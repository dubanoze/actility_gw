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

/*! @file lgw.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* no more use of PCAP */
#undef	PCAP_OK

#ifdef  PCAP_OK
#include <pcap/pcap.h>
#endif

#include "rtlbase.h"
#include "rtlimsg.h"
#include "rtllist.h"
#include "rtlhtbl.h"

#include "timeoper.h"

#include "semtech.h"
#include "headerloramac.h"

#include "xlap.h"
#include "struct.h"
#include "define.h"
#include "infrastruct.h"
#include "cproto.h"
#include "extern.h"

t_channel	*Rx2Channel	= NULL;

unsigned	int	LgwNbPacketSend;
unsigned	int	LgwNbPacketWait;	// max waiting

unsigned	int	LgwNbPacketRecv;

unsigned	int	LgwNbStartOk;
unsigned	int	LgwNbStartFailure;
unsigned	int	LgwNbConfigFailure;
unsigned	int	LgwNbLinkDown;
unsigned	int	LgwNbBusySend;
unsigned	int	LgwNbSyncError;
unsigned	int	LgwNbCrcError;
unsigned	int	LgwNbSizeError;
unsigned	int	LgwNbChanUpError;
unsigned	int	LgwNbChanDownError;
unsigned	int	LgwNbDelayError;
unsigned	int	LgwNbDelayReport;
u_int			LgwBeaconRequestedCnt;
u_int			LgwBeaconSentCnt;
u_char			LgwBeaconLastDeliveryCause;

#ifdef REF_DESIGN_V2
int	LastTmoaRequested[SX1301AR_MAX_BOARD_NB];	// ms + 10%
int	CurrTmoaRequested[SX1301AR_MAX_BOARD_NB];	// ms + 10%
#else
int	LastTmoaBoard;
int	LastTmoaRequested;	// ms + 10%
int	CurrTmoaRequested;	// ms + 10%
#endif /* REF_DESIGN_V2 */


int	LgwInvertPol		= 1;
int	LgwInvertPolBeacon	= 0;
int	LgwNoCrc		= 1;
int	LgwNoHeader		= 0;
int	LgwPreamble		= 8;
int	LgwPreambleAck		= 8;
int	LgwPower		= 14;
int	LgwPowerMax		= 30;
int	LgwAckData802Wait	= 10;
int	LgwBoard		= 1;
#ifdef REF_DESIGN_V2
int	LgwChipsPerBoard	= SX1301AR_BOARD_CHIPS_NB;
#else
int	LgwChipsPerBoard	= 2;
#endif /* REF_DESIGN_V2 */
int	LgwAntenna		= 1;
int	LgwForceRX2		= 0;

u_int	LgwSyncWord		= 0x34;	// 0x34:public 0x12:private
char	*LgwSyncWordStr		= "public";

#ifdef	WITH_LBT
int	LgwLbtSupport		= 1;
#else
int	LgwLbtSupport		= 0;
#endif
int	LgwLbtEnable		= 0;
int	LgwLbtRssi		= -80;
int	LgwLbtRssiOffset	= -4;
int	LgwLbtScantime		= 5000;		// us
int	LgwLbtNbChannel		= 8;

t_channel	TbChannel[NB_CHANNEL];
int		NbChannel	= 0;
int		MaxChannel	= 0;
t_channel_entry	TbChannelEntry[NB_CHANNEL];	// sorted TbChannel for UP freq
int		NbChannelEntry	= 0;
t_channel_entry	TbChannelEntryDn[NB_CHANNEL];	// sorted TbChannel for DN freq
int		NbChannelEntryDn	= 0;

#ifdef  PCAP_OK
static	pcap_dumper_t	*Pcap	= NULL;
#endif
static	int	Pcapture	= 0;

static	unsigned int NbLoop;

#define	NB_TEMP_GAIN	16
typedef	struct	s_temp_power
{
	int	tg_temp;
	int	tg_gain;
}	t_temp_gain;

typedef	struct
{
	int		tg_nb;
	t_temp_gain	tg_tab[NB_TEMP_GAIN];
}	t_cfg_temp_gain;

static	t_cfg_temp_gain	CfgTempGain;

static	void	LoadTempGain(int hot,int config)
{
	int	i;
	char	*pt;

	CfgTempGain.tg_nb	= 0;
	for	(i = 0 ; i < NB_TEMP_GAIN ; i++)
	{
		char	var[64];
		int	temp,gain;

		sprintf	(var,"%d",i);
		pt	= CfgStr(HtVarLgw,"temperaturegain",-1,var,"");
		if	(!pt || !*pt)	continue;
		temp	= 0;
		gain	= 0;
		sscanf	(pt,"%d%d",&temp,&gain);
		CfgTempGain.tg_nb++;
		CfgTempGain.tg_tab[i].tg_temp	= temp;
		CfgTempGain.tg_tab[i].tg_gain	= gain;
	}
}

int	LgwTempPowerGain()
{
	int	gain	= 0;
	int	i;
	int	temp	= 0;


	if	(TempEnable == 0)		// temp measurement disabled
		goto	endtempgain;

	if	(TempPowerAdjust == 0)		// power adjust disabled
		goto	endtempgain;

	if	(CfgTempGain.tg_nb <= 0)	// not configured
		goto	endtempgain;

	if	(CurrTemp == -273)		// temp measure not available
		goto	endtempgain;

	temp	= CurrTemp + TempExtAdjust;
	for	(i = 0 ; i < CfgTempGain.tg_nb ; i++)
	{
		if	(temp < CfgTempGain.tg_tab[i].tg_temp)
		{
			gain	= CfgTempGain.tg_tab[i].tg_gain;
			break;
		}
	}
	if	(i == CfgTempGain.tg_nb && CfgTempGain.tg_nb >= 1)
	{
		gain	= CfgTempGain.tg_tab[i-1].tg_gain;
	}

endtempgain:

	RTL_TRDBG(3,"enab=%d/%d temp=%d(%d,%d) count=%d gain=%d\n",
		TempEnable,TempPowerAdjust,
		temp,CurrTemp,TempExtAdjust,
		CfgTempGain.tg_nb,gain);
	return	gain;
}

void	LgwDumpGenConf(FILE *f)
{
	RTL_TRDBG(1,"gen invertpol=%d\n",LgwInvertPol);
	RTL_TRDBG(1,"gen invertpolbeacon=%d\n",LgwInvertPolBeacon);
	RTL_TRDBG(1,"gen nocrc=%d\n",LgwNoCrc);
	RTL_TRDBG(1,"gen noheader=%d\n",LgwNoHeader);
	RTL_TRDBG(1,"gen preamble=%d\n",LgwPreamble);
	RTL_TRDBG(1,"gen preambleack=%d\n",LgwPreambleAck);
	RTL_TRDBG(1,"gen power=%d\n",LgwPower);
	RTL_TRDBG(1,"gen powermax=%d\n",LgwPowerMax);
	RTL_TRDBG(1,"gen ackdata802wait=%d\n",LgwAckData802Wait);
	RTL_TRDBG(1,"gen syncword=%s (0x%02x)\n",LgwSyncWordStr,LgwSyncWord);
	RTL_TRDBG(1,"gen rfregionid=%s.%u\n",RfRegionId,RfRegionIdVers);
	RTL_TRDBG(1,"gen board=%d\n",LgwBoard);
	RTL_TRDBG(1,"gen chips per board=%d\n",LgwChipsPerBoard);
	RTL_TRDBG(1,"gen antenna=%d\n",LgwAntenna);
	RTL_TRDBG(1,"gen forcerx2=%d\n",LgwForceRX2);
	RTL_TRDBG(1,"lbt enable=%d (%s)\n",LgwLbtEnable,
				LgwLbtSupport?"supported":"not supported");
	RTL_TRDBG(1,"lbt rssi=%d rssioffset=%d scantime=%d nbchannel=%d\n",
		LgwLbtRssi,LgwLbtRssiOffset,LgwLbtScantime,LgwLbtNbChannel);

	if	(f == NULL)
		return;
	fprintf(f,"gen invertpol=%d\n",LgwInvertPol);
	fprintf(f,"gen invertpolbeacon=%d\n",LgwInvertPolBeacon);
	fprintf(f,"gen nocrc=%d\n",LgwNoCrc);
	fprintf(f,"gen noheader=%d\n",LgwNoHeader);
	fprintf(f,"gen preamble=%d\n",LgwPreamble);
	fprintf(f,"gen preambleack=%d\n",LgwPreambleAck);
	fprintf(f,"gen power=%d\n",LgwPower);
	fprintf(f,"gen powermax=%d\n",LgwPowerMax);
	fprintf(f,"gen ackdata802wait=%d\n",LgwAckData802Wait);
	fprintf(f,"gen syncword=%s (0x%02x)\n",LgwSyncWordStr,LgwSyncWord);
	fprintf(f,"gen rfregionid=%s.%u\n",RfRegionId,RfRegionIdVers);
	fprintf(f,"gen board=%d\n",LgwBoard);
	fprintf(f,"gen chips per board=%d\n",LgwChipsPerBoard);
	fprintf(f,"gen antenna=%d\n",LgwAntenna);
	fprintf(f,"gen forcerx2=%d\n",LgwForceRX2);
	fprintf(f,"lbt enable=%d (%s)\n",LgwLbtEnable,
				LgwLbtSupport?"supported":"not supported");
	fprintf(f,"lbt rssi=%d rssioffset=%d scantime=%d nbchannel=%d\n",
		LgwLbtRssi,LgwLbtRssiOffset,LgwLbtScantime,LgwLbtNbChannel);
	fflush(f);
}

int	LgwGenConfigure(int hot,int config)
{
	char	*pt;
	u_int	vers;
	int	change	= 0;

	LgwInvertPol		= CfgInt(HtVarLgw,"gen",-1,"invertpol",
								LgwInvertPol);
	LgwInvertPolBeacon	= CfgInt(HtVarLgw,"gen",-1,"invertpolbeacon",
							LgwInvertPolBeacon);
	LgwNoCrc	= CfgInt(HtVarLgw,"gen",-1,"nocrc",LgwNoCrc);
	LgwNoHeader	= CfgInt(HtVarLgw,"gen",-1,"noheader",LgwNoHeader);
	LgwPreamble	= CfgInt(HtVarLgw,"gen",-1,"preamble",LgwPreamble);
	LgwPreambleAck	= CfgInt(HtVarLgw,"gen",-1,"preambleack",LgwPreambleAck);
#ifdef	WIRMAV2
	extern	char	*IsmBand;
	if	(strstr(IsmBand,"868"))
		LgwPowerMax	= 20;
#endif
	pt		= CfgStr(HtVarSys,"",-1,"LORABOARD_TYPE","");
	if	(pt && *pt && strstr(pt,"-27dBm"))
		LgwPowerMax	= 30;
	if	(pt && *pt && strstr(pt,"-FPGA"))
		LgwPowerMax	= 30;

	LgwPower	= CfgInt(HtVarLgw,"gen",-1,"power",LgwPower);
	LgwPowerMax	= CfgInt(HtVarLgw,"gen",-1,"powermax",LgwPowerMax);
	if	(LgwPower > LgwPowerMax)
		LgwPower	= LgwPowerMax;
	LgwAckData802Wait= CfgInt(HtVarLgw,"gen",-1,"ackdata802wait",
							LgwAckData802Wait);
	LgwBoard	= CfgInt(HtVarLgw,"gen",-1,"board",LgwBoard);
	LgwChipsPerBoard = CfgInt(HtVarLgw,"gen",-1,"chipsperboard",LgwChipsPerBoard);
	LgwAntenna	= CfgInt(HtVarLgw,"gen",-1,"antenna",0);
	if	(LgwAntenna == 0)
		LgwAntenna	= LgwBoard;

	LgwForceRX2	= CfgInt(HtVarLgw,"gen",-1,"forcerx2",0);
	pt		= CfgStr(HtVarLgw,"gen",-1,"syncword","public");	
	if	(strcmp(pt,"public") == 0 || strcmp(pt,"0x34") == 0)
	{
		LgwSyncWord	= 0x34;
		LgwSyncWordStr	= "public";
	}	else
	if	(strcmp(pt,"private") == 0 || strcmp(pt,"0x12") == 0)
	{
		LgwSyncWord	= 0x12;
		LgwSyncWordStr	= "private";
	}	else
	{
		LgwSyncWord	= 0x34;
		LgwSyncWordStr	= "public";
	}

#ifdef	WITH_LBT
	LgwLbtEnable	= CfgInt(HtVarLgw,"lbt",-1,"enable",LgwLbtEnable);
	LgwLbtRssi	= CfgInt(HtVarLgw,"lbt",-1,"rssitarget",LgwLbtRssi);
	LgwLbtRssiOffset= CfgInt(HtVarLgw,"lbt",-1,"rssioffset",LgwLbtRssiOffset);
	LgwLbtScantime	= CfgInt(HtVarLgw,"lbt",-1,"scantime",LgwLbtScantime);
	LgwLbtNbChannel	= CfgInt(HtVarLgw,"lbt",-1,"nbchannel",LgwLbtNbChannel);
#ifdef REF_DESIGN_V2
	/* Arch X8 / Ref Design v2 */
	if	(LgwLbtNbChannel > SX1301AR_LBT_CHANNEL_NB_MAX)
		LgwLbtNbChannel	= SX1301AR_LBT_CHANNEL_NB_MAX;
#else
	if	(LgwLbtNbChannel > LBT_CHANNEL_FREQ_NB)
		LgwLbtNbChannel	= LBT_CHANNEL_FREQ_NB;
#endif
#endif

	LoadTempGain(hot,config);

	Pcapture	= CfgInt(HtVarLgw,"pcap",-1,"enable",Pcapture);

	pt		= CfgStr(HtVarLgw,"gen",-1,"rfregionid","");
	vers		= CfgInt(HtVarLgw,"gen",-1,"rfregionidvers",0);
	if	(hot && RfRegionId && pt
		&& (strcmp(RfRegionId,pt) != 0 || RfRegionIdVers != vers))
	{	// name/version of region id change => capab
		RTL_TRDBG(1,"rfregionid changes '%s.%u'=>'%s.%u'\n",
					RfRegionId,RfRegionIdVers,pt,vers);
		change	= 1;
	}
	RfRegionIdVers	= vers;

	if	(pt && (pt=strdup(pt)))
	{
		if	(RfRegionId)
		{
			free(RfRegionId);
		}
		RfRegionId	= pt;
	}


	if	(change)
	{
		SendCapabToLrc(NULL);	// all lrcs
	}

	return	0;
}

void	DoPcap(char *data,int sz)
{
#ifdef	PCAP_OK
	if	(Pcap)
	{
		struct pcap_pkthdr hdr;

		hdr.caplen	= p->size;
		hdr.len		= p->size;
		gettimeofday	(&hdr.ts,NULL);
		pcap_dump	((u_char *)Pcap,&hdr,p->payload);
		pcap_dump_flush	(Pcap);
	}
#endif
}

double TimeOnAir( t_RadioLoRaSettings *modem, uint8_t pktLen )
{
    double airTime = 0.0;

    double bw = 0.0;
    switch( modem->Bandwidth )
    {
    case 1: // 250 kHz
         bw = 250e3;
    break;
    case 2: // 500 kHz
         bw = 500e3;
    break;
    case 0: // 125 kHz
    default :
         bw = 125e3;
    break;
    }

    if	(modem->Datarate < 7 || modem->Datarate > 12)
    	return	0.0;

    if	(modem->Coderate < 1 || modem->Coderate > 4)
//    	return	0.0;
	modem->Coderate	= 1;

    // Symbol rate : time for one symbol (secs)
    double rs = bw / ( 1 << modem->Datarate );
    double ts = 1 / rs;
    // time of preamble
    double tPreamble = ( modem->PreambleLen + 4.25 ) * ts;
    // Symbol length of payload and time
    double tmp = ceil( (8 * pktLen - 4 * modem->Datarate +
                       28 + 16 * modem->CrcOn -
                       ( modem->FixLen ? 20 : 0 ) ) /
                         ( double )( 4 * modem->Datarate -
                         ( ( modem->LowDatarateOptimize > 0 ) ? 8 : 0 ) ) ) *
                                 ( modem->Coderate + 4 );
    double nPayload = 8 + ( ( tmp > 0 ) ? tmp : 0 );
    double tPayload = nPayload * ts;
    // Time on air 
    double tOnAir = tPreamble + tPayload;
    // return us secs
    airTime = floor( tOnAir * 1e6 + 0.999 );
    return airTime;
}

float	TmoaLrrPacketUp(T_lgw_pkt_rx_t *pkt)
{
	t_RadioLoRaSettings	modem;

	memset	(&modem,0,sizeof(modem));

	switch(pkt->bandwidth)
	{
		case	BW_500KHZ :
			modem.Bandwidth	= 2;
			break;
		case	BW_250KHZ :
			modem.Bandwidth	= 1;
			break;
		case	BW_125KHZ :
		default :
			modem.Bandwidth	= 0;
			break;
	}
	modem.FixLen			= 0;
	modem.LowDatarateOptimize	= 0;
	modem.PreambleLen		= LgwPreamble;	
	switch (pkt->status)
	{
		case	STAT_NO_CRC :
		case	STAT_UNDEFINED :
			modem.CrcOn = 0;
			break;
		case	STAT_CRC_BAD :
			return 0;
		case	STAT_CRC_OK :
			modem.CrcOn = 1;
			break;
	}

#ifdef REF_DESIGN_V2
	modem.Datarate		= CodeSpreadingFactor(pkt->modrate);
#else
	modem.Datarate		= CodeSpreadingFactor(pkt->datarate);
#endif /* REF_DESIGN_V2 */
	modem.Coderate		= CodeCorrectingCode(pkt->coderate) - 4;	//[5..8] -> [1..4]

RTL_TRDBG(3,"TmoaLrrPacketUp: bw=%d crc=%d dr=%d cr=%d\n", modem.Bandwidth, modem.CrcOn, modem.Datarate, modem.Coderate);
	return TimeOnAir(&modem,pkt->size);
}

void	TmoaLrrPacket(t_lrr_pkt *downpkt)
{
	t_RadioLoRaSettings	modem;
	double			tmp;

	memset	(&modem,0,sizeof(modem));

	modem.Bandwidth			= 0;	// 125khz
	modem.FixLen			= 0;
	modem.LowDatarateOptimize	= 0;
	modem.PreambleLen		= LgwPreamble;
	modem.CrcOn			= (LgwNoCrc ? 0 : 1);

	modem.Datarate		= downpkt->lp_spfact;
	modem.Coderate		= downpkt->lp_correct - 4;  //[5..8] -> [1..4]

	if ((downpkt->lp_flag&LP_RADIO_PKT_ACKMAC) == LP_RADIO_PKT_ACKMAC)
		modem.PreambleLen = LgwPreambleAck;

	if ((downpkt->lp_flag&LP_RADIO_PKT_SZPREAMB) == LP_RADIO_PKT_SZPREAMB)
		modem.PreambleLen = downpkt->lp_szpreamb;

	downpkt->lp_tmoa	= TimeOnAir(&modem,downpkt->lp_size);
	if	(downpkt->lp_tmoa == 0.0)
		return;

	if ((downpkt->lp_flag&LP_RADIO_PKT_ACKDATA) == LP_RADIO_PKT_ACKDATA
	&& (downpkt->lp_flag&LP_RADIO_PKT_802154) == LP_RADIO_PKT_802154)
	{
		tmp	= TimeOnAir(&modem,5);	// size for ack 802154
		if	(tmp == 0.0)
			return;
		downpkt->lp_tmoa	= downpkt->lp_tmoa + tmp;
	}
}


// we cannot use bsearch(3) because of possible duplicate frequencies
static	int	_BinSearchFirstChannel(uint32_t freq,int nb,t_channel_entry tbentry[])
{
	int	l	= 0;
	int	h	= nb-1;
	int	m;

	t_channel_entry	*e;

	while	(l <= h)
	{
		m	= (l + h) / 2;
		e	= &tbentry[m];
		if	(e->freq_hz == freq)
		{	// we have to find the first entry with this freq
			if(m > 0 && e->freq_hz == tbentry[m-1].freq_hz)
			{
				h	= m - 1;
			}
			else
				return	m;
		}
		else if	(e->freq_hz < freq)
		{
			l	= m + 1;
		}
		else
		{
			h	= m - 1;
		}
	}
	return	-1;
}

int	BinSearchFirstChannel(uint32_t freq)
{
	return	_BinSearchFirstChannel(freq,NbChannelEntry,TbChannelEntry);
}

int	BinSearchFirstChannelDn(uint32_t freq)
{
	return	_BinSearchFirstChannel(freq,NbChannelEntryDn,TbChannelEntryDn);
}

t_channel	*FindChannelUp(uint32_t freq)
{
	int		ret;
	t_channel_entry	*ent;

	ret	= _BinSearchFirstChannel(freq,NbChannelEntry,TbChannelEntry);
	if	(ret < 0)
		return	NULL;
	if	(ret >= NB_CHANNEL)
		return	NULL;

	ent	= &TbChannelEntry[ret];
	return	&TbChannel[ent->index];
}

t_channel	*FindChannelDn(uint32_t freq)
{
	int		ret;
	t_channel_entry	*ent;

	ret	= _BinSearchFirstChannel(freq,NbChannelEntryDn,TbChannelEntryDn);
	if	(ret < 0)
		return	NULL;
	if	(ret >= NB_CHANNEL)
		return	NULL;

	ent	= &TbChannelEntryDn[ret];
	return	&TbChannel[ent->index];
}

void	ChangeChannelFreq(t_lrr_pkt *downpkt,T_lgw_pkt_tx_t *txpkt)
{
	u_int		freqdn;
	t_channel	*c;

#if	0	// tests only
	freqdn	= (u_int)CfgInt(HtVarLrr,"lrr",-1,"testfreqdn",868888888);
#else
	// search if the LRC requires a different DL freq for the channel
	freqdn	= (downpkt->lp_freqdn[2] * 256 * 256)
		+ (downpkt->lp_freqdn[1] * 256)
		+ (downpkt->lp_freqdn[0]);
	freqdn	= freqdn * 100;
	if	(freqdn <= 0)
		return;
#endif

RTL_TRDBG(1,"PKT SEND change downk link freq %u -> %u\n",txpkt->freq_hz,freqdn);

	// by default accept the new freq
	txpkt->freq_hz	= freqdn;

	// find channel in TbChannel dn or up for this frequency
	// and adapt channel/subband numbers to get correct DTC
	c	= FindChannelDn(freqdn);
	if	(!c)
	{
		c	= FindChannelUp(freqdn);
		if	(!c)
		{
RTL_TRDBG(1,"PKT SEND change downk link freq %u not declared\n",freqdn);
			return;
		}
	}
	if	(c->name[0] == '\0' || c->freq_hz != freqdn)
		return;


RTL_TRDBG(1,"PKT SEND change downk link channel %u -> %u\n",
			downpkt->lp_channel,c->channel);

	downpkt->lp_channel	= c->channel;
	downpkt->lp_subband	= c->subband;

	return;
}

int	CmpChannel(const void *m1, const void *m2)
{
	t_channel_entry	*e1	= (t_channel_entry *)m1;
	t_channel_entry	*e2	= (t_channel_entry *)m2;

	return	e1->freq_hz - e2->freq_hz;
}

int	ChannelConfigure(int hot,int config)
{
	char	file[PATH_MAX];
	FILE	*f	= NULL;
	int	i;
	char	*pt;
	t_channel	*p;

	int	defmod	= MOD_LORA;
	int	defband	= BW_125KHZ;
	int	defrate	= DR_LORA_MULTI;



	NbChannel	= 0;
	NbChannelEntry	= 0;
	MaxChannel	= 0;
	for	(i = 0 ; i < NB_CHANNEL ; i++)
	{
		p	= &TbChannel[i];
		pt		= CfgStr(HtVarLgw,"channel",i,"name",0);
		if	(!pt || !*pt)	continue;
		strcpy	((char *)p->name,pt);

		pt		= CfgStr(HtVarLgw,"channel",i,"freqhz",0);
		if	(!pt || !*pt)	continue;
		if	(strchr(pt,'.'))
		{
			p->freq_hz	= (int)(atof(pt) * 1000000);
			if	(p->freq_hz % 1000 == 999)
				p->freq_hz++;
		}
		else
			p->freq_hz	= atoi(pt);
		p->channel	= i;
		p->subband	= CfgInt(HtVarLgw,"channel",i,"subband",0);
		p->bandwidth	= 
			CfgInt(HtVarLgw,"channel",i,"bandwidth",defband);
		p->datarate	= 
			CfgInt(HtVarLgw,"channel",i,"datarate",defrate);
		p->modulation	= 
			CfgInt(HtVarLgw,"channel",i,"modulation",defmod);

		p->power	= LgwPower;
		pt		= CfgStr(HtVarLgw,"channel",i,"power",0);
		if	(pt && *pt && *pt == '+')
			p->power	= LgwPower + atoi(pt);
		else if	(pt && *pt && *pt == '-')
			p->power	= LgwPower + atoi(pt);
		else if	(pt && *pt)
			p->power	= atoi(pt);

		if	(p->power < 0)
			p->power	= LgwPower;

		p->usedforrx2	= CfgInt(HtVarLgw,"channel",i,"usedforrx2",0);
		p->dataraterx2	= CfgInt(HtVarLgw,"channel",i,"dataraterx2",0);
		if	(p->usedforrx2 && p->dataraterx2)
			Rx2Channel	= p;
		p->lbtscantime	= CfgInt(HtVarLgw,"channel",i,"lbtscantime",0);
		if	(p->lbtscantime == 1)
		{
			p->lbtscantime	= LgwLbtScantime;
		}
		NbChannel++;
		MaxChannel	= i+1;

		if	(p->channel <= MAXUP_CHANNEL)
		{	// need to sort uplink channels
			TbChannelEntry[NbChannelEntry].freq_hz	= p->freq_hz;
			TbChannelEntry[NbChannelEntry].index	= i;
			NbChannelEntry++;
		}
		if	(p->channel > MAXUP_CHANNEL && p->channel < NB_CHANNEL)
		{	// need to sort dnlink channels
			TbChannelEntryDn[NbChannelEntryDn].freq_hz = p->freq_hz;
			TbChannelEntryDn[NbChannelEntryDn].index	= i;
			NbChannelEntryDn++;
		}
	}

	qsort(TbChannelEntry,NbChannelEntry,sizeof(t_channel_entry),
								CmpChannel);
	qsort(TbChannelEntryDn,NbChannelEntryDn,sizeof(t_channel_entry),
								CmpChannel);

	for	(i = 0 ; i < MaxChannel && i < NB_CHANNEL ; i++)
	{
		p	= &TbChannel[i];
		if	(p->name[0] == '\0')	continue;
		if	(p->freq_hz == 0)	continue;
RTL_TRDBG(1,"logicchan%03d G%d n='%s' frhz=%d m=0x%02x bandw=0x%02x datar=0x%02x power=%d power-lost+gain[0]=%d rx2=%d datarrx2=0x%02x scantime=%d\n",
		p->channel,p->subband,p->name,p->freq_hz,p->modulation,
		p->bandwidth,p->datarate,
		p->power,p->power-AntennaGain[0]+CableLoss[0],
		p->usedforrx2,p->dataraterx2,p->lbtscantime);

	}

	for	(i = 0 ; i < NbChannelEntry && i < NB_CHANNEL ; i++)
	{
RTL_TRDBG(1,"sortchanup frhz=%d index=%d\n",TbChannelEntry[i].freq_hz,
			TbChannelEntry[i].index);
	}
	for	(i = 0 ; i < NbChannelEntryDn && i < NB_CHANNEL ; i++)
	{
RTL_TRDBG(1,"sortchandn frhz=%d index=%d\n",TbChannelEntryDn[i].freq_hz,
			TbChannelEntryDn[i].index);
	}


#if	0
	printf("868300000=%d\n",BinSearchFirstChannel(868300000));
	printf("868100000=%d\n",BinSearchFirstChannel(868100000));
	printf("903100000=%d\n",BinSearchFirstChannel(903100000));
	printf("923400000=%d\n",BinSearchFirstChannel(923400000));
	printf("123400000=%d\n",BinSearchFirstChannel(123400000));
#endif

	if	(!config)
		return	0;
	sprintf	(file,"%s/var/log/lrr/logicchan.txt",RootAct);
	f	= fopen(file,"w");
	if	(!f)
		return	0;

	for	(i = 0 ; i < MaxChannel && i < NB_CHANNEL ; i++)
	{
		p	= &TbChannel[i];
		if	(p->name[0] == '\0')	continue;
		if	(p->freq_hz == 0)	continue;
fprintf(f,"logicchan%03d G%d n='%s' frhz=%d m=0x%02x bandw=0x%02x datar=0x%02x power=%d power-lost+gain[0]=%d rx2=%d datarrx2=0x%02x scantime=%d\n",
		p->channel,p->subband,p->name,p->freq_hz,p->modulation,
		p->bandwidth,p->datarate,
		p->power,p->power-AntennaGain[0]+CableLoss[0],
		p->usedforrx2,p->dataraterx2,p->lbtscantime);

	}
	fclose(f);

	return	0;
}


static	void	LgwDoClockMs()
{
}

static	void	LgwDoClockSc()
{
	static	unsigned	int	nbclock	= 0;
	time_t	now		= 0;
	int	linkstatus	= IM_LGW_LINK_UP;

	nbclock++;
	rtl_timemono(&now);

	if	(LgwLinkUp && !LgwThreadStopped && !LgwLINKUP())
	{
		linkstatus	= IM_LGW_LINK_DOWN;
		LgwNbLinkDown++;
	}

#if	0	// TEST ONLY
	if	(nbclock > 10)
		linkstatus	= IM_LGW_LINK_DOWN;
#endif

	rtl_imsgAdd(MainQ,rtl_imsgAlloc(IM_DEF,linkstatus,NULL,0));
}

static	void	LgwDoInternalEvent(t_imsg *imsg)
{
#ifdef HAL_VERSION_5
	t_gpstime * gt;
#endif
	RTL_TRDBG(3,"receive event cl=%d ty=%d\n",imsg->im_class,imsg->im_type);
	switch(imsg->im_class)
	{
	case	IM_DEF :
		switch(imsg->im_type)
		{
		case IM_LGW_EXIT :
		break;
		case IM_LGW_GPS_TIME :
#ifdef	WITH_GPS
#ifdef HAL_VERSION_5
			gt = (t_gpstime *)imsg->im_dataptr;
			LgwGpsTimeUpdated(&gt->utc, &gt->ubx);
#else
			LgwGpsTimeUpdated((struct timespec *)imsg->im_dataptr, NULL);
#endif /* HAL_VERSION_5 */
#endif /* WITH_GPS */
		break;
		case IM_SERVICE_STATUS_RQST :
		break;
		}
	break;
	}
}


static	void	LgwDoInternalTimer(t_imsg *imsg)
{
	RTL_TRDBG(3,"receive timer cl=%d ty=%d\n",
		imsg->im_class,imsg->im_type);

	switch(imsg->im_class)
	{
	case	IM_DEF :
	switch(imsg->im_type)
	{
	case	IM_TIMER_GEN :
		rtl_imsgAdd(LgwQ,
		rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,IM_TIMER_GEN_V,NULL,0));
		NbLoop	= 0;
	break;
	case	IM_TIMER_BEACON :
		rtl_imsgAdd(LgwQ,
		rtl_timerAlloc(IM_DEF,IM_TIMER_BEACON,IM_TIMER_BEACON_V,NULL,0));
	break;
	}
	break;
	}
}

void	AutoRx2Settings(t_lrr_pkt *downpkt)
{
	t_channel	*chan	= Rx2Channel;
	uint16_t 	spfact;

	if	(!chan)
		return;

	spfact		= CodeSpreadingFactor(chan->dataraterx2);

	downpkt->lp_channel	= chan->channel;
	downpkt->lp_subband	= chan->subband;
	if	(downpkt->lp_rx2spfact > 0)
		downpkt->lp_spfact	= downpkt->lp_rx2spfact;
	else
		downpkt->lp_spfact	= spfact;

RTL_TRDBG(1,"LRR REPORT DELAY auto RX2 params sb=%d ch=%d sfpkt=%d sfdef=%d\n",
		downpkt->lp_subband,
		downpkt->lp_channel,
		downpkt->lp_rx2spfact,
		spfact);

	// FIX3810
	downpkt->lp_flag	= downpkt->lp_flag | LP_RADIO_PKT_RX2;
}

void	AdjustRx2Settings(t_lrr_pkt *downpkt)
{
	t_channel	*chan	= Rx2Channel;
	int		adjust	= 0;
	uint16_t 	spfact;
	uint8_t		cc;

	if	(!chan)
		return;

	spfact	= CodeSpreadingFactor(chan->dataraterx2);
	cc	= CodeCorrectingCode(CR_LORA_4_5);

	if	(downpkt->lp_channel == UNK_CHANNEL)
	{
		adjust	= 1;
		downpkt->lp_channel	= chan->channel;
		downpkt->lp_subband	= chan->subband;
		// FIX3810
		downpkt->lp_flag	= downpkt->lp_flag | LP_RADIO_PKT_RX2;
	}
	if	(downpkt->lp_spfact == 0)
	{
		adjust	= 1;
		downpkt->lp_spfact	= spfact;
		downpkt->lp_correct	= cc;
	}
	if	(downpkt->lp_correct == 0)
	{
		adjust	= 1;
		downpkt->lp_correct	= cc;
	}

	if	(adjust)
	{
		// we change or adjust radio parameters => recompute tmoa
		TmoaLrrPacket(downpkt);
RTL_TRDBG(1,"LRR REPORT DELAY adjust RX2 params sb=%d ch=%d sf=%d tmoa=%fms\n",
		downpkt->lp_subband,
		downpkt->lp_channel,
		downpkt->lp_spfact,
		downpkt->lp_tmoa/1000.0);
	}
}

#if	defined(WITH_GPS)
void	LgwEstimUtc(struct timespec *utc) // from GPS
{
#ifdef REF_DESIGN_V2
	utc->tv_sec	= Gps_time_ref[0].utc.tv_sec; // from GPS
#else
	utc->tv_sec	= Time_reference_gps.utc.tv_sec; // from GPS
#endif /* REF_DESIGN_V2 */
	if	(utc->tv_sec == 0)
		utc->tv_sec	= time(NULL);

	if	(LgwTmmsUtcTime == 0)
	{
		utc->tv_nsec	= 500 * 1E6;
		return;
	}
	utc->tv_nsec	= (ABS(rtl_tmmsmono() - LgwTmmsUtcTime)) * 1E6;
	utc->tv_nsec	+= (100 * 1E6); // suppose we are always 100ms late
	if	(utc->tv_nsec >= 1E9)
	{
		utc->tv_sec++;
		utc->tv_nsec	= utc->tv_nsec - 1E9;
	}
}
#else
void	LgwEstimUtc(struct timespec *utc) // no GPS
{

	clock_gettime(CLOCK_REALTIME,utc);
	utc->tv_nsec	+= (100 * 1E6); // suppose we are always 100ms late
	if	(utc->tv_nsec >= 1E9)
	{
		utc->tv_sec++;
		utc->tv_nsec	= utc->tv_nsec - 1E9;
	}
}
#endif

int	LgwPacketDelayMsFromUtc(t_lrr_pkt *downpkt,struct timespec *utc)
{
	double	fdelay;

	fdelay	= (double)(downpkt->lp_gss) - (double)(utc->tv_sec);

	fdelay	+= 1E-9 * ((double)(downpkt->lp_gns) - (double)(utc->tv_nsec));

	fdelay	*= 1000;	// in ms

	return	(int)fdelay;
}

int	LgwPacketDelayMsFromUtc2(t_lrr_pkt *downpkt,struct timespec *utc)
{
	double	fdelay;

	fdelay	= (double)(utc->tv_sec) - (double)(downpkt->lp_gss);

	fdelay	+= 1E-9 * ((double)(utc->tv_nsec) - (double)(downpkt->lp_gns));

	fdelay	*= 1000;	// in ms

	return	(int)fdelay;
}

int	LgwDiffMsUtc(struct timespec *utc1,struct timespec *utc0)
{
	double	fdelay;

	fdelay	= (double)(utc1->tv_sec) - (double)(utc0->tv_sec);

	fdelay	+= 1E-9 * ((double)(utc1->tv_nsec) - (double)(utc0->tv_nsec));

	fdelay	*= 1000;	// in ms

	return	(int)fdelay;
}

void	LgwInitPingSlot(t_lrr_pkt *pkt)
{
	pkt->lp_classb		= 1;
	pkt->lp_nbslot		= pkt->lp_tus;
	pkt->lp_firstslot	= pkt->lp_tms & 0x0000FFFF;
	pkt->lp_firstslot2	= pkt->lp_tms >> 16;
	pkt->lp_period		= 1;

	pkt->lp_tus	= 0;
	pkt->lp_tms	= 0;
	pkt->lp_trip	= 0;
	pkt->lp_delay	= 0;
}

void	LgwResetPingSlot(t_lrr_pkt *pkt)
{
	pkt->lp_gss0 	= pkt->lp_gss;
	pkt->lp_gns0 	= pkt->lp_gns;
	pkt->lp_maxtry	= 2 * pkt->lp_nbslot;
	pkt->lp_currslot= pkt->lp_firstslot;
	pkt->lp_nbtry	= 0;
	pkt->lp_idxtry	= 0;
}

int	LgwNextPingSlot(t_lrr_pkt *pkt,struct timespec *eutc,int maxdelay,int *retdelay)
{
	double	slotLen	= 0.03;
	double	pingPeriod;		// en nombre de slots
	double	sDur;
	int	sIdx;
	int	delay;

	if	(!pkt)
		return	-1;

retry:
	pkt->lp_nbtry++;
	pkt->lp_idxtry++;
	if	(pkt->lp_nbtry > pkt->lp_maxtry)
		return	-1;

	pingPeriod	= 4096.0 / pkt->lp_nbslot;

	if	(pkt->lp_nbtry == pkt->lp_nbslot + 1)
	{	// change beacon period P1 => P2
		pkt->lp_period	= 2;
		pkt->lp_idxtry	= 1;
		pkt->lp_currslot= pkt->lp_firstslot2;
#ifdef	TRACE
printf("beacon period change\n");
#else
RTL_TRDBG(3,"beacon period change\n");
#endif
	}

	sIdx	= (pkt->lp_idxtry - 1) * pingPeriod;
	sIdx	= pkt->lp_currslot + sIdx;
	sDur	= (double)(sIdx) * slotLen;

#ifdef	TRACE
printf	("sIdx=%04d sDur=%f ",sIdx,sDur);
#else
RTL_TRDBG(3,"sIdx=%04d sDur=%f\n",sIdx,sDur);
#endif

	pkt->lp_sidx	= sIdx;
	pkt->lp_sdur	= (float)sDur;

	pkt->lp_gss	= pkt->lp_gss0 + (int)sDur; 
	if	(pkt->lp_nbtry >= pkt->lp_nbslot + 1)
	{	// beacon P2
		pkt->lp_gss	+= 128;
	}
	sDur	= (sDur - (int)sDur) * 1E9;
	pkt->lp_gns	= pkt->lp_gns0 + (int)sDur; 
	if	(pkt->lp_gns > 1E9)
	{
		pkt->lp_gss++;
		pkt->lp_gns	= pkt->lp_gns - 1E9;
	}

	delay	= LgwPacketDelayMsFromUtc(pkt,eutc);

#ifdef	TRACE
printf	("ss=%09u ns=%09u delay(ms)=%d\n",pkt->lp_gss,pkt->lp_gns,delay);
#else
RTL_TRDBG(3,"ss=%09u ns=%09u delay(ms)=%d\n",pkt->lp_gss,pkt->lp_gns,delay);
#endif

	if	(delay < maxdelay)
		goto	retry;

	*retdelay	= delay;

	if	(pkt->lp_nbtry + 1 > pkt->lp_maxtry)
		return	0;	// this is the last possible try
	return	1;
}


//
// called by main thread, use rtl_* functions only
// and do not use lgw_* functions
//
int	LgwSendPacket(t_lrr_pkt *downpkt,int seqnum,int p802,int ack)
{
	t_imsg	*msg;
	int	board;
	int	sz;
	int	nbw;
	int	delay		= 0;	// ms
	int	adjust		= 0;
	int	timelimit	= 3;	//ms
	u_int	trip		= 0;
	int	reportedonce	= 0;
	int	currtmoareq;


	if	(!downpkt || !downpkt->lp_payload || downpkt->lp_size <= 0)
	{
		return	-1;
	}
	msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_SEND_DATA,NULL,0);
	if	(!msg)
	{
		return	-2;
	}
	sz	= sizeof(t_lrr_pkt);

	delay	= 0;
	// recompute trip because it may have been delayed in mainq
	if	(downpkt->lp_tms)
	{
		trip	= ABS(rtl_tmmsmono() - downpkt->lp_tms);
	}
	// invalidate DTC infos
	downpkt->lp_flag = downpkt->lp_flag & ~LP_RADIO_PKT_DTC;

	board = downpkt->lp_chain>>5;
	if (board < 0 || board >= LgwBoard)
	{
		RTL_TRDBG(1,"board out of bounds (%d), forced to 0\n", board);
		board = 0;
	}
	if	(downpkt->lp_beacon || downpkt->lp_classb)
	{
		RTL_TRDBG(3,"PKT SEND beacon or classb direct queueing\n");
		goto	queue_msg;
	}
	if	(downpkt->lp_delay == 0)
	{
		// permanent listening
		// adjust rx2 params
		downpkt->lp_tms		= rtl_tmmsmono();
		downpkt->lp_lgwdelay	= 0;
		downpkt->lp_tus		= 0;
		if	(!p802 && downpkt->lp_majorv == 0 && downpkt->lp_minorv >= 3)
		{
			if	(Rx2Channel)
			{
				AdjustRx2Settings(downpkt);
			}
		}
#ifdef REF_DESIGN_V2
		if	(CurrTmoaRequested[board])
		{
			delay	= CurrTmoaRequested[board] + timelimit;
			RTL_TRDBG(1,"PKT SEND board=%d NODELAY avoid collision postpone=%d\n",
							board, delay);
		}
#else
		if	(CurrTmoaRequested)
		{
			delay	= CurrTmoaRequested + timelimit;
			RTL_TRDBG(1,"PKT SEND board=%d NODELAY avoid collision postpone=%d\n",
							board, delay);
		}
#endif /* REF_DESIGN_V2 */
		goto	queue_msg;
	}

	if	(UseLgwTime /* && UseGpsTime && Gps_ref_valid */ &&
		(downpkt->lp_flag&LP_RADIO_PKT_ACKDATA) != LP_RADIO_PKT_ACKDATA)
	{
//		downpkt->lp_flag = downpkt->lp_flag | LP_RADIO_PKT_LGWTIME;
		downpkt->lp_lgwdelay	= 1;
		adjust			= 0;
	}
	else
	{
//		downpkt->lp_flag = downpkt->lp_flag & ~LP_RADIO_PKT_LGWTIME;
		downpkt->lp_lgwdelay	= 0;
		adjust			= AdjustDelay;
		p802			= 1;
	}

	delay	= downpkt->lp_delay - trip;
	delay	= delay + adjust;

	// on second window LRC tells it does not know rx2 params
	if	(!p802 && downpkt->lp_majorv == 0 && downpkt->lp_minorv >= 3)
	{
		if	((downpkt->lp_flag&LP_RADIO_PKT_RX2)==LP_RADIO_PKT_RX2)
		{
			if	(Rx2Channel)
			{
				AdjustRx2Settings(downpkt);
				SetIndic(downpkt,-1,LP_C1_LRC,-1,-1);
			}
		}
	}

	// too late : but no second window for 802 watteco
	if	(p802 && delay <= timelimit)
	{
RTL_TRDBG(1,"LRR REPORT DELAY ERROR trip=%u rqtdelay=%u delay=%d p802=%d\n",
		trip,downpkt->lp_delay,delay,p802);
		LgwNbDelayError++;
		SetIndic(downpkt,0,LP_C1_DELAY,LP_C2_NA,-1);
		rtl_imsgFree(msg);
		return	-4;
	}

	// too late : can we try the second window
	if	(p802 == 0 && (delay <= timelimit || LgwForceRX2))
	{
report :
		if	((downpkt->lp_flag&LP_RADIO_PKT_RX2)==LP_RADIO_PKT_RX2)
		{	// LRC tells it is already the second window
RTL_TRDBG(1,"LRR REPORT DELAY already the second window\n");
			LgwNbDelayError++;
			SetIndic(downpkt,0,LP_C1_LRC,LP_C2_DELAY,-1);
			rtl_imsgFree(msg);
			return	-4;
		}

		downpkt->lp_rx2lrr = 1;
		downpkt->lp_delay += 1000;
		delay	= downpkt->lp_delay - trip;

RTL_TRDBG(1,"LRR REPORT DELAY trip=%u rqtdelay=%u delay=%d reported=%d\n",
			trip,downpkt->lp_delay,delay,reportedonce);

		SetIndic(downpkt,-1,LP_C1_DELAY,-1,-1);
		reportedonce	= 1;
		if	(delay <= timelimit)
		{
RTL_TRDBG(1,"LRR REPORT DELAY ERROR trip=%u rqtdelay=%u delay=%d p802=%d\n",
			trip,downpkt->lp_delay,delay,p802);
			LgwNbDelayError++;
			SetIndic(downpkt,0,-1,LP_C2_DELAY,-1);
			rtl_imsgFree(msg);
			return	-4;
		}
		if	(downpkt->lp_majorv == 0 && downpkt->lp_minorv >= 3)
		{
			if	(Rx2Channel)
			{
				AutoRx2Settings(downpkt);
			}
		}
		LgwNbDelayReport++;
	}

#ifdef REF_DESIGN_V2
	currtmoareq = CurrTmoaRequested[board];
#else
	currtmoareq = CurrTmoaRequested;
#endif /* REF_DESIGN_V2 */

RTL_TRDBG(1,"%s DELAY %s=%u/%u rqtdelay=%u delay=%u busydur=%d\n",
		(downpkt->lp_lgwdelay==0?"LRR":"LGW"),
		(downpkt->lp_postponed?"postponed":"trip"),
		downpkt->lp_trip,trip,
		downpkt->lp_delay,delay, currtmoareq);

	if	(downpkt->lp_lgwdelay)
	{
		downpkt->lp_lgwdelay	= delay;
		if	(currtmoareq)
		{
			delay	= delay - currtmoareq;
			if	(delay <= timelimit)
			{
				if	(reportedonce == 0 && p802 == 0)
					goto	report;
				RTL_TRDBG(1,"PKT SEND busy\n");
				LgwNbBusySend++;
				SetIndic(downpkt,0,-1,LP_C2_BUSY,-1);
				rtl_imsgFree(msg);
				return	-4;
			}
			downpkt->lp_lgwdelay	= delay;
			delay	= currtmoareq + timelimit;
			RTL_TRDBG(1,"PKT SEND avoid collision postpone=%d\n",
							delay);
		}
		else
			delay	= 0;
	}
	else
	{
	}

queue_msg :
	sz	= sizeof(t_lrr_pkt);
	if	( rtl_imsgDupData(msg,downpkt,sz) != msg)
	{
		rtl_imsgFree(msg);
		return	-5;
	}
	if	(delay)
	{	// delayed by the LRR process itself
#ifdef REF_DESIGN_V2
		nbw	= rtl_imsgAddDelayed(LgwSendQ[board],msg,delay);
#else
		nbw	= rtl_imsgAddDelayed(LgwSendQ,msg,delay);
#endif /* REF_DESIGN_V2 */
	}
	else
	{	// delayed or not by the SX13
#ifdef REF_DESIGN_V2
		nbw	= rtl_imsgAdd(LgwSendQ[board],msg);
#else
		nbw	= rtl_imsgAdd(LgwSendQ,msg);
#endif /* REF_DESIGN_V2 */
	}

	if	(nbw < 0)
	{
		rtl_imsgFree(msg);
		return	-5;
	}
	return	0;
}

static	void	LgwIdleLoop()
{
	t_imsg	*msg;
	RTL_TRDBG(0,"enter Idle loop ... waiting for cancelation\n");

	for(;;sleep(1))
	{
		while ((msg= rtl_imsgGet(LgwQ,IMSG_BOTH)) != NULL)
		{
			rtl_imsgFree(msg);
		}
	}
}

static	void	LgwBootLoop()
{
	int	err;

	if	((err=LgwConfigure(0,1)) < 0)
	{
		LgwNbConfigFailure++;
		rtl_imsgAddDelayed(MainQ,
		rtl_imsgAlloc(IM_DEF,IM_LGW_CONFIG_FAILURE,NULL,0),120000);
		LgwIdleLoop();
	}
#ifdef	WITH_LBT	// TODO provisoire
#ifdef	WIRMAV2
	if	(LgwLbtEnable >= 2)
	{
		char	exe[512];
		int	freq;

		freq	= (IsmFreq / 100 ) * 100;
		sprintf	(exe,"%s/lrr/%s/%s",RootAct,"util_fpga_start",
				"util_fpga_lbt.sh");
		if	(access(exe,X_OK) == 0)
		{
			int	ret;
			int	status;
			char	tmp[64];

			sprintf	(tmp," %d",freq);
			strcat	(exe,tmp);
			RTL_TRDBG(0,"start FPGA for LBT '%s'\n",exe);
			ret	= system(exe);
			RTL_TRDBG(0,"start FPGA for LBT ret=%d status=%d\n",
					ret,WEXITSTATUS(ret));
			sleep(3);
		}
		else
		{
			RTL_TRDBG(0,"cannot start FPGA for LBT '%s'\n",exe);
		}
	}
#endif
#endif
	if	((err=LgwStart()) < 0)
	{
		LgwNbStartFailure++;
		rtl_imsgAddDelayed(MainQ,
		rtl_imsgAlloc(IM_DEF,IM_LGW_START_FAILURE,NULL,0),120000);
		LgwIdleLoop();
	}
	LgwNbStartOk++;
	rtl_imsgAdd(MainQ,
		rtl_imsgAlloc(IM_DEF,IM_LGW_STARTED,NULL,0));
}

static	int count_thread;
static	void	cleanup_thread(void *a)
{
	int	nbm = rtl_imsgRemoveAll(LgwQ);
	RTL_TRDBG(0,"stop lrr.x/lgw th=%lx pid=%d count=%d nbm=%d\n",
			(long)pthread_self(),getpid(),--count_thread,nbm);
}

static	void	LgwMainLoop()
{
	time_t	lasttimems	= 0;
	time_t	lasttimesc	= 0;
	time_t	now		= 0;
	int	nbr;
	int	nbs;
	time_t	t0	= 0;
	time_t	delta	= 0;
#ifdef REF_DESIGN_V2
	uint8_t txstatus[SX1301AR_MAX_BOARD_NB] = { 0 };
	int b;
#else
	uint8_t txstatus = 0;
#endif /* REF_DESIGN_V2 */

	t_imsg	*msg;

	rtl_imsgAdd(LgwQ,
	rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,IM_TIMER_GEN_V,NULL,0));
	rtl_imsgAdd(LgwQ,
	rtl_timerAlloc(IM_DEF,IM_TIMER_BEACON,IM_TIMER_BEACON_V,NULL,0));

	while	(!ServiceStopped && !LgwThreadStopped)
	{
//		TimeStamp	= LgwTimeStamp();
		t0	= rtl_tmmsmono();
		// internal events
		while ((msg= rtl_imsgGet(LgwQ,IMSG_MSG)) != NULL)
		{
			LgwDoInternalEvent(msg);
			rtl_imsgFree(msg);
		}
		// proceed packet(s) to send before received packets
		// BUT mark received packets now
		now	= rtl_tmmsmono();
		nbs	= LgwDoSendPacket(now);
		nbr	= LgwDoRecvPacket(now);
		if	(nbr || nbs)
		{
			RTL_TRDBG(3,"PKT xxxx nbr=%d nbs=%d\n",nbr,nbs);
		}
//		usleep(10*1000);	// 10ms
		usleep(Sx13xxPolling*1000);
		NbLoop++;

#ifdef REF_DESIGN_V2
		for (b=0; b<LgwBoard; b++)
		{
			if	(!CurrTmoaRequested[b])
				continue;
			CurrTmoaRequested[b] = CurrTmoaRequested[b] - ABS(rtl_tmmsmono() - t0);
			if (CurrTmoaRequested[b] <= 0)
			{
				RTL_TRDBG(2, "LGW DELAY TXSTATUS board%d should be free now tmoa+sched=%u\n", 
					b, LastTmoaRequested[b]);
				if (LgwTxFree(b, &txstatus[b]) <= 0) {
					RTL_TRDBG(1, "LGW DELAY board%d status=%d NOT FREE\n", b, txstatus[b]);
				}
				LastTmoaRequested[b]	= 0;
				CurrTmoaRequested[b]	= 0;
			}
		}
#else
		if	(CurrTmoaRequested)
		{
			CurrTmoaRequested = CurrTmoaRequested - ABS(rtl_tmmsmono() - t0);
			{
				static int tmoacount=0;
				if (0 && tmoacount++ % 20 == 0)
					RTL_TRDBG(2, "LGW CurrTmoaRequested=%u\n", CurrTmoaRequested);
			}
			if (CurrTmoaRequested <= 0)
			{
				RTL_TRDBG(2, "LGW DELAY TXSTATUS board%d should be free now tmoa+sched=%u\n",
					LastTmoaBoard, LastTmoaRequested);
				if (LgwTxFree(LastTmoaBoard, &txstatus) <= 0)
				{
					RTL_TRDBG(1, "LGW DELAY board%d status=%d NOT FREE\n",
						LastTmoaBoard, txstatus);
				}
				LastTmoaRequested	= 0;
				LastTmoaBoard		= 0;
				CurrTmoaRequested	= 0;
			}
		}
#endif /* REF_DESIGN_V2 */

		// clocks
		if	((delta=ABS(now-lasttimems)) >= 100)
		{
#if REF_DESIGN_V2
			for (b=0; b<LgwBoard; b++)
			{
				if (1 && !LgwTxFree(b, &txstatus[b]))
				{
					struct timespec utc;
					LgwEstimUtc(&utc);
					RTL_TRDBG(1, "TX in use %d board %d tmoa req=%dms eutc=(%u,%ums)\n",
						txstatus[b], b, CurrTmoaRequested[b],
						utc.tv_sec, utc.tv_nsec/1000000);
				}
			}
#else
			if	(1 && !LgwTxFree(0, &txstatus))
			{
				struct timespec utc;
				LgwEstimUtc(&utc);
				RTL_TRDBG(1,"TX in use %d %d eutc=(%u,%ums)\n",
					txstatus, CurrTmoaRequested,
					utc.tv_sec, utc.tv_nsec/1000000);
			}
#endif /* REF_DESIGN_V2 */
			// try to adjust UTC.nsec
			delta	= LgwCurrUtcTime.tv_nsec + (delta * 1E6);
			if	(delta < 1E9)
			{
				LgwCurrUtcTime.tv_nsec	= delta;
			}
			else
			{
				LgwCurrUtcTime.tv_sec++;
				LgwCurrUtcTime.tv_nsec	= delta - 1E9;
			}
			LgwDoClockMs();
			lasttimems	= now;
		}

		if	(ABS(now-lasttimesc) >= 1000)
		{
#ifdef LP_TP31
		DcCheckLists();
#endif
			LgwDoClockSc();
			lasttimesc	= now;
		}

		// internal timer
		while ((msg= rtl_imsgGet(LgwQ,IMSG_TIMER)) != NULL)
		{
			LgwDoInternalTimer(msg);
			rtl_imsgFree(msg);
		}
	}
	LgwStop();
	RTL_TRDBG(0,"lgw thread auto canceled\n");
}


void	*LgwRun(void *param)
{
	int	capture = 0;

	pthread_cleanup_push(cleanup_thread,param);


#if	0
	if	(Pcapture && !Pcap)
	{
		char	day[256];
		char	hour[256];
		char	file[256];

                sprintf (file,"cap%s_%s.pcap",rtl_aaaammjj(0,day),
	                                                rtl_hhmmss(0,hour));

		Pcap=pcap_dump_open(pcap_open_dead(DLT_IEEE802_15_4,512),file);
		if	(Pcap)
		{
			unlink	("cap.pcap");
			link	(file,"cap.pcap");
		}
	}
	if	(Pcapture && Pcap)
			capture	= 1;
#endif

	RTL_TRDBG(0,"start lrr.x/lgw th=%lx pid=%d count=%d capture=%d\n",
		(long)pthread_self(),getpid(),++count_thread,capture);

	LgwBootLoop();
	LgwMainLoop();
	pthread_cleanup_pop(0);
	return	NULL;
}
