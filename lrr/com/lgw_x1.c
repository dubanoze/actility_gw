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

#include "rtlbase.h"
#include "rtlimsg.h"
#include "rtllist.h"
#include "rtlhtbl.h"

#include "timeoper.h"

#if defined(MTAC_USB)
#define LGW_RF_TX_ENABLE        { true, false }  /* radio B TX not connected*/
#define LGW_RF_CLKOUT           { true, true }   /* both radios have clkout enabled */
#endif

#undef WITH_SX1301_X8
#ifndef WITH_SX1301_X1
#define WITH_SX1301_X1
#endif
#include "semtech.h"
#include "headerloramac.h"

#ifndef	TX_FREE
#define	TX_FREE	TX_EMPTY
#endif

#include "xlap.h"
#include "define.h"
#include "infrastruct.h"
#include "struct.h"
#include "cproto.h"
#include "extern.h"

/* time reference used for UTC <-> timestamp conversion */
struct tref Time_reference_gps; 
/* is GPS reference acceptable (ie. not too old) */
int Gps_ref_valid; 

struct timespec	LgwCurrUtcTime;		// last UTC + nsec computation
struct timespec	LgwBeaconUtcTime;	// last beacon
time_t		LgwTmmsUtcTime;		// tmms mono of the last UTC/GPS

static	int	_LgwStarted;
#ifdef	TEST_ONLY
static	int	LgwWaitSending	= 1;
#else
static	int	LgwWaitSending	= 0;
#endif

#ifdef USELIBLGW3
bool	LgwTxEnable[LGW_RF_CHAIN_NB];
#endif

static	int	RType0	= 1257;	// radio type of rfconf0 (sx1255,sx1257,...)

static	t_define	TbDefine[] =
{
	{ "MOD_LORA"		,	MOD_LORA },
	{ "MOD_FSK"		,	MOD_FSK },

	{ "BW_500KHZ"		,	BW_500KHZ },
	{ "BW_250KHZ"		,	BW_250KHZ },
	{ "BW_125KHZ"		,	BW_125KHZ },
	{ "BW_62K5HZ"		,	BW_62K5HZ },
	{ "BW_31K2HZ"		,	BW_31K2HZ },
	{ "BW_15K6HZ"		,	BW_15K6HZ },
	{ "BW_7K8HZ"		,	BW_7K8HZ },


	{ "DR_LORA_SF7"		,	DR_LORA_SF7 },
	{ "DR_LORA_SF8"		,	DR_LORA_SF8 },
	{ "DR_LORA_SF9"		,	DR_LORA_SF9 },
	{ "DR_LORA_SF10"	,	DR_LORA_SF10 },
	{ "DR_LORA_SF11"	,	DR_LORA_SF11 },
	{ "DR_LORA_SF12"	,	DR_LORA_SF12 },
	{ "DR_LORA_MULTI"	,	DR_LORA_MULTI },

	{ "CR_LORA_4_5"		,	CR_LORA_4_5 },
	{ "CR_LORA_4_6"		,	CR_LORA_4_6 },
	{ "CR_LORA_4_7"		,	CR_LORA_4_7 },
	{ "CR_LORA_4_8"		,	CR_LORA_4_8 },


	{ NULL			,	0 },
	{ NULL			,	0 },
	{ NULL			,	0 },

};

void	LgwForceDefine(void *htbl)
{
	int	i;
	char	value[64];

	for	(i = 0 ; TbDefine[i].name ; i++)
	{
		sprintf	(value,"0x%02x",TbDefine[i].value);
		rtl_htblRemove(htbl,TbDefine[i].name);
		rtl_htblInsert(htbl,TbDefine[i].name,(void *)strdup(value));
	}

}

char	*LgwVersionInfo()
{
	static	char	vers[128];

	sprintf	(vers,"@(#) Semtech hal %s",(char *)lgw_version_info());
	return	vers;
}

int	LgwTxFree(uint8_t board,uint8_t *txstatus)
{
	lgw_status(TX_STATUS,txstatus);
	if	(*txstatus != TX_FREE)
		return	0;
	return	1;
}

#ifdef	WITH_GPS
int	LgwEstimTrigCnt(uint32_t *trig_tstamp)
{
	int	ret;
	struct	timespec	utc;

	*trig_tstamp	= 0;
	LgwEstimUtc(&utc);
	ret	= lgw_utc2cnt(Time_reference_gps,utc,trig_tstamp);
	return	ret;
}
#endif

#ifdef	WITH_GPS
int	LgwEstimUtcBoard(struct timespec *utc)
{
	int		ret;
	uint32_t	trig_tstamp;

	lgw_reg_w(LGW_GPS_EN,0);
	ret	= lgw_get_trigcnt(&trig_tstamp); 
	lgw_reg_w(LGW_GPS_EN,1);
	if	(ret != LGW_HAL_SUCCESS)
		return	LGW_HAL_ERROR;

	ret	= lgw_cnt2utc(Time_reference_gps,trig_tstamp,utc);
	if	(ret != LGW_HAL_SUCCESS)
		return	LGW_HAL_ERROR;

	return	LGW_HAL_SUCCESS;
}
#endif

char	*PktStatusTxt(uint8_t status)
{
	static	char	buf[64];

	switch	(status)
	{
	case	STAT_NO_CRC :
		return	"CRCNO";
	case	STAT_CRC_BAD :
		return	"CRCERR";
	case	STAT_CRC_OK :
		return	"CRCOK";
	}
	sprintf	(buf,"CRC?(0x%x)",status);
	return	buf;
}

char	*BandWidthTxt(int bandwidth)
{
	static	char	buf[64];

	switch	(bandwidth)
	{
	case	BW_500KHZ :
		return	"BW500";
	case	BW_250KHZ :
		return	"BW250";
	case	BW_125KHZ :
		return	"BW125";
	case	BW_62K5HZ :
		return	"BW62.5";
	case	BW_31K2HZ :
		return	"BW31.2";
	case	BW_15K6HZ :
		return	"BW15.6";
	case	BW_7K8HZ :
		return	"BW7.8";
	}
	sprintf	(buf,"BW?(0x%x)",bandwidth);
	return	buf;
}

char	*SpreadingFactorTxt(int sf)
{
	static	char	buf[64];

	switch	(sf)
	{
	case	DR_LORA_SF7 :
		return	"SF7";
	case	DR_LORA_SF8 :
		return	"SF8";
	case	DR_LORA_SF9 :
		return	"SF9";
	case	DR_LORA_SF10 :
		return	"SF10";
	case	DR_LORA_SF11 :
		return	"SF11";
	case	DR_LORA_SF12 :
		return	"SF12";
	}

	sprintf	(buf,"SF?(0x%x)",sf);
	return	buf;
}


uint8_t	CodeSpreadingFactor(int sf)
{
	switch	(sf)
	{
	case	DR_LORA_SF7 :
		return	7;
	case	DR_LORA_SF8 :
		return	8;
	case	DR_LORA_SF9 :
		return	9;
	case	DR_LORA_SF10 :
		return	10;
	case	DR_LORA_SF11 :
		return	11;
	case	DR_LORA_SF12 :
		return	12;
	default :
		return	0;
	}
}

uint8_t	DecodeSpreadingFactor(int spf)
{
	switch	(spf)
	{
	case	7 :
		return	DR_LORA_SF7;
	case	8 :
		return	DR_LORA_SF8;
	case	9 :
		return	DR_LORA_SF9;
	case	10 :
		return	DR_LORA_SF10;
	case	11 :
		return	DR_LORA_SF11;
	case	12 :
		return	DR_LORA_SF12;
	default :
		return	DR_LORA_SF10;
	}
}

char	*CorrectingCodeTxt(int coderate)
{
	static	char	buf[64];

	switch	(coderate)
	{
	case	CR_LORA_4_5 :
		return	"CC4/5";
	case	CR_LORA_4_6 :
		return	"CC4/6";
	case	CR_LORA_4_7 :
		return	"CC4/7";
	case	CR_LORA_4_8 :
		return	"CC4/8";
	}
	sprintf	(buf,"CC?(0x%x)",coderate);
	return	buf;
}

uint8_t	CodeCorrectingCode(int coderate)
{
	switch	(coderate)
	{
	case	CR_LORA_4_5 :
		return	5;
	case	CR_LORA_4_6 :
		return	6;
	case	CR_LORA_4_7 :
		return	7;
	case	CR_LORA_4_8 :
		return	8;
	default :
		return	0;
	}
}

uint8_t	DecodeCorrectingCode(int cr)
{
	switch	(cr)
	{
	case	5 :
		return	CR_LORA_4_5;
	case	6 :
		return	CR_LORA_4_6;
	case	7 :
		return	CR_LORA_4_7;
	case	8 :
		return	CR_LORA_4_8;
	default :
		return	CR_LORA_4_5;
	}
}

#if	0
static	unsigned int	LgwTimeStamp()
{
	uint8_t	buf[4];
	unsigned int value;
	int	nbbits=32;
	int	ret;

	ret	= lgw_reg_rb(LGW_TIMESTAMP,buf,nbbits);
	if	(ret != LGW_REG_SUCCESS)
	{
		RTL_TRDBG(0,"cannot read LGW timestamp REG ret=%d\n",ret);
		return	0;	
	}
//	value	= (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
	value	= *(unsigned int *)buf;
	return	value;
}
#endif

int	LgwLINKUP()
{
	return	1;
#if	0
	static	unsigned int prev;
	static	unsigned int err;
	uint8_t	buf[4];
	unsigned int value;
	unsigned int diff;

	value	= LgwTimeStamp();
	RTL_TRDBG(9,"LGW timestamp REG current=%u previous=%u\n",
							value,prev);
	if	(!prev || !value)
	{
		prev	= value;
		return	1;
	}
	if	(prev == value)
	{
		LgwNbSyncError++;
		err++;
		RTL_TRDBG(0,"LGW timestamp REG no diff=%u err=%d\n",prev,err);
		prev	= value;
		if	(err >= 3)
		{
			RTL_TRDBG(0,"LGW LINK DOWN\n");
			err	= 0;
			prev	= 0;
			return	0;
		}
	}
	prev	= value;
	err	= 0;
	return	1;
#endif
}


t_channel	*FindChannelForPacket(T_lgw_pkt_rx_t *up)
{
	int		i;
	t_channel	*c;
	t_channel_entry	*e;
	int		sortindex;
	if	(!up)
		return	NULL;

	// find first index in sorted TbChannelEntry for this frequency
	sortindex	= BinSearchFirstChannel(up->freq_hz);
	if	(sortindex < 0)
		goto	not_found;

	for	(i = sortindex ; i < NbChannelEntry && i < NB_CHANNEL ; i++)
	{
		e	= &TbChannelEntry[i];
		if	(e->freq_hz != up->freq_hz)
			break;
		c	= &TbChannel[e->index];
		if	(c->name[0] == '\0' || c->freq_hz == 0)
			continue;
		if	(c->modulation != up->modulation)
			continue;
		if	(c->freq_hz != up->freq_hz)
			continue;
		if	(c->bandwidth != up->bandwidth)
			continue;
		if	(c->modulation == MOD_FSK)
		{	// do not compare datarate
			return	c;
		}
		if	(c->modulation == MOD_LORA)
		{
			if	((c->datarate & up->datarate) == up->datarate)
				return	c;
		}
	}

not_found :
RTL_TRDBG(0,"no chan(%d) found for frz=%d mod=0x%02x bdw=0x%02x dtr=0x%02x\n",
	sortindex,up->freq_hz,up->modulation,up->bandwidth,up->datarate);

	return	NULL;
}

static	void LgwDumpRfConf(FILE *f,int rfi,T_lgw_conf_rxrf_t rfconf)
{
	if	(rfconf.enable == 0)
		return;

#ifdef USELIBLGW3
	RTL_TRDBG(1,"rf%d enab=%d frhz=%d tx_enab=%d rssi_off=%f sx=%d\n",
	rfi,rfconf.enable,rfconf.freq_hz,
	rfconf.tx_enable, rfconf.rssi_offset,rfconf.type);
#else
	RTL_TRDBG(1,"rf%d enab=%d frhz=%d\n",rfi,rfconf.enable,
		rfconf.freq_hz);
#endif

	if	(f == NULL)
		return;
#ifdef USELIBLGW3
	fprintf(f,"rf%d enab=%d frhz=%d tx_enab=%d rssi_off=%f sx=%d\n",
	rfi,rfconf.enable,rfconf.freq_hz,
	rfconf.tx_enable, rfconf.rssi_offset,rfconf.type);
#else
	fprintf(f,"rf%d enab=%d frhz=%d\n",rfi,rfconf.enable,
		rfconf.freq_hz);
#endif
	fflush(f);
}

static	void LgwDumpIfConf(FILE *f,int ifi,T_lgw_conf_rxif_t ifconf,int fbase)
{
	if	(ifconf.enable == 0)
		return;

	RTL_TRDBG(1,"if%d enab=%d frhz=%d rfchain=%d bandw=0x%x datar=0x%x\n",
		ifi,ifconf.enable,
		(ifconf.freq_hz+fbase),ifconf.rf_chain,
		ifconf.bandwidth,ifconf.datarate);

	if	(f == NULL)
		return;
	fprintf(f,"if%d enab=%d frhz=%d rfchain=%d bandw=0x%x datar=0x%x\n",
		ifi,ifconf.enable,
		(ifconf.freq_hz+fbase),ifconf.rf_chain,
		ifconf.bandwidth,ifconf.datarate);
	fflush(f);
}

#ifdef USELIBLGW3
#ifdef	WITH_LBT
static	void LgwDumpLbtConf(FILE *f, T_lgw_conf_lbt_t lbtconf)
{
	int i;
	RTL_TRDBG(1, "lbt nb_channel=%d\n", lbtconf.nb_channel);
	for	(i = 0 ; i < lbtconf.nb_channel ; i++) 
	{

		RTL_TRDBG(1,"lbt channels[%d] .freq_hz=%d .scan_time_us=%d\n", 
		i,lbtconf.channels[i].freq_hz,lbtconf.channels[i].scan_time_us);
	}

	if	(f == NULL)
		return;
	fprintf(f, "lbt nb_channel=%d\n", lbtconf.nb_channel);
	for	(i = 0 ; i < lbtconf.nb_channel ; i++) 
	{
		fprintf(f,"lbt channels[%d] .freq_hz=%d .scan_time_us=%d\n", 
		i,lbtconf.channels[i].freq_hz,lbtconf.channels[i].scan_time_us);
	}
	fflush(f);
}
#endif
#endif

#ifdef USELIBLGW3
static	void LgwDumpLutConf(FILE *f,int l,struct lgw_tx_gain_s *lut)
{
	RTL_TRDBG(2,"lut%d power=%d pa=%d mix=%d dig=%d dac=%d\n",l,
	lut->rf_power,lut->pa_gain,lut->mix_gain,lut->dig_gain,lut->dac_gain);

	if	(f == NULL)
		return;
	fprintf(f,"lut%d power=%d pa=%d mix=%d dig=%d dac=%d\n",l,
	lut->rf_power,lut->pa_gain,lut->mix_gain,lut->dig_gain,lut->dac_gain);
	fflush(f);
}
#endif

#ifdef USELIBLGW3
static	int LgwConfigureLut(FILE *f,struct lgw_tx_gain_lut_s *txlut,int config)
{
	char	lutsection[64];
	int	lut;
	char	*pt;
	int	ret;

	sprintf	(lutsection,"lut/%d/%d/%d",IsmFreq,LgwPowerMax,RType0);
	pt	= CfgStr(HtVarLgw,lutsection,-1,"0","");
	if	(!pt || !*pt)
	{
		RTL_TRDBG(0,"LUT '%s' not found\n",lutsection);
		strcpy	(lutsection,"lut");
	}

	memset(txlut,0,sizeof(struct lgw_tx_gain_lut_s));
	for (lut = 0; lut < TX_GAIN_LUT_SIZE_MAX; lut++)
	{
		char	var[64];
		int	dig,pa,dac,mix,pow;

		sprintf	(var,"%d",lut);
		pt	= CfgStr(HtVarLgw,lutsection,-1,var,"");
		if	(!pt || !*pt)	continue;
		txlut->size++;
		dig	= 0;
		dac	= 3;
		sscanf	(pt,"%d%d%d%d%d",&pow,&pa,&mix,&dig,&dac);
		txlut->lut[lut].rf_power	= (int8_t)pow;	// signed !!!
		txlut->lut[lut].pa_gain		= (uint8_t)pa;
		txlut->lut[lut].mix_gain	= (uint8_t)mix;
		txlut->lut[lut].dig_gain	= (uint8_t)dig;
		txlut->lut[lut].dac_gain	= (uint8_t)dac;
		LgwDumpLutConf(f,lut,&txlut->lut[lut]);
	}
	if	(config && txlut->size > 0)
	{
		ret	= lgw_txgain_setconf(txlut);
		if	(ret == LGW_HAL_ERROR)
		{
			RTL_TRDBG(0,"LUT%d cannot be configured ret=%d\n",
							lut,ret);
			if	(f)
				fprintf(f,"LUT%d cannot be configured ret=%d\n", lut,ret);
			return	-1;
		}
	}
	if	(config)
	{
		RTL_TRDBG(1,"%s %s configured nblut=%d\n",
			lutsection,txlut->size==0?"not":"",txlut->size);
	}

	if	(f)
	{
		fprintf(f,"%s %s configured nblut=%d\n",
			lutsection,txlut->size==0?"not":"",txlut->size);
	}
	return	txlut->size;
}
#endif

#ifdef USELIBLGW3
int8_t GetTxCalibratedEIRP(int8_t tx_requested, int8_t antenna_gain, int8_t cable_loss, uint8_t board, uint8_t rfc)
{
	int8_t	tx_found, tx_tmp;
	int	i, nb_lut_max;
	struct lgw_tx_gain_lut_s	txlut;

	tx_tmp = tx_requested - antenna_gain + cable_loss;
	LgwConfigureLut(NULL, &txlut, 0);

	nb_lut_max	= TX_GAIN_LUT_SIZE_MAX;
	tx_found = txlut.lut[0].rf_power; /* If requested value is less than the lowest LUT power value */
	for (i = (nb_lut_max-1); i >= 0; i--)
	{
		if ( tx_tmp >= txlut.lut[i].rf_power)
		{
			tx_found = txlut.lut[i].rf_power;
			break;
		}
	}
	RTL_TRDBG(1,"LUT(%d) => %d\n",tx_tmp,tx_found);
	return (tx_found + antenna_gain - cable_loss);
}
#else
int8_t GetTxCalibratedEIRP(int8_t tx_requested, int8_t antenna_gain, int8_t cable_loss, uint8_t board, uint8_t rfc)
{
	return	tx_requested;
}
#endif

#ifdef	WITH_LBT
int	CmpChannelLbt(const void *m1, const void *m2)
{
	struct lgw_conf_lbt_chan_s *e1	= (struct lgw_conf_lbt_chan_s *)m1;
	struct lgw_conf_lbt_chan_s *e2	= (struct lgw_conf_lbt_chan_s *)m2;

	return	e1->freq_hz - e2->freq_hz;
}
#endif

int	LgwConfigure(int hot,int config)
{
	char	file[PATH_MAX];
	FILE	*f	= NULL;
	T_lgw_conf_rxrf_t rfconf;
	T_lgw_conf_rxif_t ifconf;
	int	rfi;
	int	ifi;
	int	ret;


	if	(config)
	{
		sprintf	(file,"%s/var/log/lrr/radioparams.txt",RootAct);
		f	= fopen(file,"w");
		RTL_TRDBG(1,"RADIO configuring ...\n");
	}
	else
	{
		RTL_TRDBG(1,"RADIO dump configuration ...\n");
	}

	LgwGenConfigure(hot,config);

	LgwDumpGenConf(f);

#if defined(FCMLB) || defined(FCPICO) || defined(FCLAMP)
	if (!strcmp(SpiDevice, "/dev/spidev1.0"))
	{
		lgw_config_spi(SpiDevice);
		lgw_config_sx1301_reset_gpio(87);
	}
	else if (!strcmp(SpiDevice, "/dev/spidev1.1"))
	{
		lgw_config_spi(SpiDevice);
		lgw_config_sx1301_reset_gpio(86);
	}
	else if (!strcmp(SpiDevice, "/dev/spidev2.0"))
	{
		lgw_config_spi(SpiDevice);
		lgw_config_sx1301_reset_gpio(88);
	}
#endif

#ifdef	WITH_TTY
	{
		uint8_t uid[8];  //unique id
		if	(lgw_connect(false) != LGW_REG_SUCCESS)
		{
			RTL_TRDBG(0,"Can not connect to board via tty link \n");
			return	-1;
		}
                lgw_reg_GetUniqueId(&uid[0]);
		RTL_TRDBG(1,"stpico gw id: '%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x'\n",
			uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
	}
#endif

#ifdef USELIBLGW3
	struct lgw_conf_board_s boardconf;
	memset(&boardconf, 0, sizeof(boardconf));
	boardconf.clksrc =	CfgInt(HtVarLgw,"gen",-1,"clksrc",1);
	boardconf.lorawan_public = true;
	if	(LgwSyncWord == 0x12)
		boardconf.lorawan_public = false;
	ret = lgw_board_setconf(boardconf);
	if	(ret == LGW_HAL_ERROR)
	{
		RTL_TRDBG(0,"Board cannot be configured ret=%d\n", ret);
		return	-1;
	}
#endif

#ifdef	WITH_LBT
	if	(LgwLbtEnable)
	{
		T_lgw_conf_lbt_t  lbtconf;
		memset(&lbtconf, 0, sizeof(lbtconf));
		lbtconf.enable		= 1;
		lbtconf.rssi_target	= LgwLbtRssi;
		lbtconf.rssi_offset	= LgwLbtRssiOffset;
		lbtconf.nb_channel	= LgwLbtNbChannel;
	    
		int	i;
		int	lfi	= 0;

		// search freq for LBT in "lowlvlgw.ini" first
        	for	(i = 0 ; i < lbtconf.nb_channel ; i++)
		{
			int	freq_hz;
			int	scantime;

			freq_hz	= CfgInt(HtVarLgw,"lbt/chan_cfg",lfi,"freq",0);
			scantime= CfgInt(HtVarLgw,"lbt/chan_cfg",lfi,"scantime",
						LgwLbtScantime);
			if	(freq_hz == 0)	continue;
			if	(scantime == 0)	continue;
			lbtconf.channels[lfi].freq_hz		= freq_hz;
			lbtconf.channels[lfi].scan_time_us	= scantime;

			lfi++;
			if	(lfi > LBT_CHANNEL_FREQ_NB)	break;
		}
		// no freq found search in "channels.ini"
		if	(lfi == 0)
		{
		for	(i = 0 ; i < MaxChannel && i < NB_CHANNEL ; i++)
		{
			t_channel	*p;

			p	= &TbChannel[i];
			if	(p->name[0] == '\0')	continue;
			if	(p->freq_hz == 0)	continue;
			if	(p->lbtscantime == 0)	continue;

			lbtconf.channels[lfi].freq_hz		= p->freq_hz;
			lbtconf.channels[lfi].scan_time_us	= p->lbtscantime;
			lfi++;
			if	(lfi > LBT_CHANNEL_FREQ_NB)	break;
		}
		}
		lbtconf.nb_channel = lfi;
		if	(lbtconf.nb_channel == 0)
			lbtconf.enable		= 0;
		// sort lbtconf.channels by freq_hz
		int szelem	= sizeof(struct lgw_conf_lbt_chan_s);
		qsort(&lbtconf.channels[0],lfi,szelem,CmpChannelLbt);
		LgwDumpLbtConf(f, lbtconf);
		ret = lgw_lbt_setconf(lbtconf);
		if	(ret == LGW_HAL_ERROR)
		{
			RTL_TRDBG(0,"LBT cannot be configured ret=%d\n", ret);
			return	-1;
		}
	}
#endif

	for	(rfi = 0 ; rfi < LGW_RF_CHAIN_NB ; rfi++)
	{
		memset(&rfconf,0,sizeof(rfconf));
		rfconf.enable	= CfgInt(HtVarLgw,"rfconf",rfi,"enable",0);
		rfconf.freq_hz	= CfgInt(HtVarLgw,"rfconf",rfi,"freqhz",0);
#ifdef USELIBLGW3
		int	rtype;
		rtype	= CfgInt(HtVarLgw,"rfconf",rfi,"radiotype",1257);
		switch	(rtype)
		{
		case	1255:	// 400-510 MHz
			rfconf.type	= LGW_RADIO_TYPE_SX1255;
		break;
		case	1257 :	// 860-1000 MHz
		default :
			rfconf.type	= LGW_RADIO_TYPE_SX1257;
		break;
		}
		if	(rfi == 0)
			RType0	= rtype;
		rfconf.rssi_offset	= atof(CfgStr(HtVarLgw,"rfconf",rfi,
							"rssioffset","-166.0"));
		rfconf.tx_enable	= CfgInt(HtVarLgw,"rfconf",rfi,
								"txenable",1);
//		Force tx_enable=1 for rfconf 0, tx_enable=0 for all others
//		rfconf.tx_enable	= (rfi==0)?1:0;
		LgwTxEnable[rfi]	= rfconf.tx_enable;
#endif
		LgwDumpRfConf(f,rfi,rfconf);
		if	(config)
		{
			ret	= lgw_rxrf_setconf(rfi,rfconf);
			if	(ret == LGW_HAL_ERROR)
			{
			RTL_TRDBG(0,"RF%d cannot be configured ret=%d\n",
								rfi,ret);
				if	(f)	fclose(f);
				return	-1;
			}
		}
	}

	for	(ifi = 0 ; ifi < LGW_IF_CHAIN_NB ; ifi++)
	{
		int	fbase;

		memset(&ifconf,0,sizeof(ifconf));
		ifconf.enable	= CfgInt(HtVarLgw,"ifconf",ifi,"enable",0);
		ifconf.rf_chain	= CfgInt(HtVarLgw,"ifconf",ifi,"rfchain",0);
		ifconf.freq_hz	= CfgInt(HtVarLgw,"ifconf",ifi,"freqhz",0);
		ifconf.bandwidth= CfgInt(HtVarLgw,"ifconf",ifi,"bandwidth",0);
		ifconf.datarate	= CfgInt(HtVarLgw,"ifconf",ifi,"datarate",0);
#ifdef USELIBLGW3
		ifconf.sync_word	= CfgInt(HtVarLgw,"ifconf",ifi,"syncword",0);
		ifconf.sync_word_size	= CfgInt(HtVarLgw,"ifconf",ifi,"syncwordsize",0);
#endif

		rfi		= ifconf.rf_chain;
		fbase		= CfgInt(HtVarLgw,"rfconf",rfi,"freqhz",0);
		LgwDumpIfConf(f,ifi,ifconf,fbase);
		if	(config)
		{
			ret	= lgw_rxif_setconf(ifi,ifconf);
			if	(ret == LGW_HAL_ERROR)
			{
				RTL_TRDBG(0,"IF%d cannot be configured ret=%d\n",
								ifi,ret);
				if	(f)	fclose(f);
				return	-1;
			}
		}
	}
#ifdef USELIBLGW3
	struct lgw_tx_gain_lut_s txlut;

	ret	= LgwConfigureLut(f,&txlut,config);
#endif
	if	(config)
	{
		RTL_TRDBG(1,"RADIO configured\n");
	}

	if	(f)
	{
		fprintf(f,"RADIO configured\n");
		fclose(f);
	}

	return	0;
}

void	LgwRegister()
{
	unsigned char	syncword;

	unsigned char	sw1;
	unsigned char	sw2;

#ifdef	USELIBLGW3	 // sync word set by lgw_rxrf_setconf() hal 3.2.0
	RTL_TRDBG(1,"SET SYNC WORD %s (0x%02x)\n",LgwSyncWordStr,LgwSyncWord);
	return;		
#endif

	syncword	= (unsigned char)LgwSyncWord;


	sw1	= syncword >> 4;
	sw2	= syncword & 0x0F;

	RTL_TRDBG(1,"SET SYNC WORD sw=0x%02x registers sw1=%x sw2=%x\n",
			syncword,sw1,sw2);

	lgw_reg_w(LGW_FRAME_SYNCH_PEAK1_POS,sw1); /* default 1 */
	lgw_reg_w(LGW_FRAME_SYNCH_PEAK2_POS,sw2); /* default 2 */

	lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK1_POS,sw1); /* default 1 */
	lgw_reg_w(LGW_MBWSSF_FRAME_SYNCH_PEAK2_POS,sw2); /* default 2 */

	lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK1_POS,sw1); /* default 1 */
	lgw_reg_w(LGW_TX_FRAME_SYNCH_PEAK2_POS,sw2); /* default 2 */
}

void	LgwReverseRX()
{
#ifndef	WIRMAV2
	return;
#endif

#if	0
	int	ret;
	ret	= CfgInt(HtVarLgw,"gen",-1,"reverserx",0);
	if	(ret >= 1)
	{
		extern	int LGW_REG_MODEM_INVERT_IQ;
		extern	int LGW_REG_ONLY_CRC_EN;

		LGW_REG_MODEM_INVERT_IQ	= 0;
		LGW_REG_ONLY_CRC_EN	= 0;
		RTL_TRDBG(1,"RADIO reverse RX ...\n");
	}
#endif
}

int	LgwStart()
{
	int	ret;
	FILE	*chk;
	char	file[PATH_MAX];

#ifdef	USELIBLGW3	 // LUT calibration set by lgw_rxrf_setconf() hal 3.2.0
#else
	setBoardCalibration();
#endif
	LgwReverseRX();

	RTL_TRDBG(1,"RADIO starting ...\n");
#ifdef GEMTEK
	ret	= lgw_start(SpiDevice);
#else
	ret	= lgw_start();
#endif
	if	(ret == LGW_HAL_ERROR)
	{
		RTL_TRDBG(0,"RADIO cannot be started ret=%d\n",ret);
		sprintf	(file,"%s/var/log/lrr/radioparams.txt",RootAct);
		chk	= fopen(file,"a");
		if	(chk)
		{
			fprintf(chk,"RADIO cannot be started ret=%d\n",ret);
			fclose(chk);
		}
		return	-1;
	}

	LgwRegister();
	_LgwStarted	= 1;
	RTL_TRDBG(1,"RADIO started ret=%d\n",ret);
	sprintf	(file,"%s/var/log/lrr/checkreg.out",RootAct);
	chk	= fopen(file,"w");
	if	(chk)
	{
		lgw_reg_check(chk);
		fclose(chk);
	}
	sprintf	(file,"%s/var/log/lrr/radioparams.txt",RootAct);
	chk	= fopen(file,"a");
	if	(chk)
	{
		fprintf(chk,"RADIO started ret=%d\n",ret);
		fclose(chk);
	}
	return	0;
}

int	LgwStarted()
{
	return	_LgwStarted;
}

void	LgwStop()
{
	_LgwStarted	= 0;
	RTL_TRDBG(1,"RADIO stopping ...\n");
	lgw_stop();
	RTL_TRDBG(1,"RADIO stopped\n");
}

#ifdef	WITH_GPS
void	LgwGpsTimeUpdated(struct timespec *utc_from_gps, struct timespec * ubxtime_from_gps)
{
	int	ret;
	uint32_t trig_tstamp;
	time_t	now;

	now	= rtl_tmmsmono();
	if	(Gps_ref_valid == false)
	{
		Time_reference_gps.systime	= time(NULL);
	}

	long	gps_ref_age = 0;
	gps_ref_age = (long)difftime(time(NULL), Time_reference_gps.systime);
#if	1
	if ((gps_ref_age >= 0) && (gps_ref_age <= GPS_REF_MAX_AGE)) 
	{	/* time ref is ok, validate and  */
		Gps_ref_valid = true;
	} 
	else 
	{	/* time ref is too old, invalidate */
		Gps_ref_valid = false;
		RTL_TRDBG(0,"LGW GPS SYNC (sec=%u,ns=%09u) INVALID\n",
		utc_from_gps->tv_sec,utc_from_gps->tv_nsec);
		return;
	}
#else
	Gps_ref_valid	= true;
#endif

	// get trig counter corresponding to the last GPS TIC
	ret	= lgw_get_trigcnt(&trig_tstamp);
	if	(ret != LGW_HAL_SUCCESS)
	{
		RTL_TRDBG(0,"cannot read trig for GPS SYNC\n");
		return;
	}

	ret	= lgw_gps_sync(&Time_reference_gps,trig_tstamp,*utc_from_gps);
	if	(ret != LGW_HAL_SUCCESS)
	{
		RTL_TRDBG(0,"cannot sync trig for GPS SYNC slope=%f\n",
			Time_reference_gps.xtal_err);
		return;
	}


	RTL_TRDBG(3,
	"LGW GPS SYNC tus=%u difs=%d difms=%d (sec=%u,ns=%09u) (sec=%u,ns=%09u) \n",
		trig_tstamp,gps_ref_age,ABS(now - LgwTmmsUtcTime),
		utc_from_gps->tv_sec,utc_from_gps->tv_nsec,
		LgwCurrUtcTime.tv_sec,LgwCurrUtcTime.tv_nsec);

	LgwCurrUtcTime.tv_sec	= utc_from_gps->tv_sec;
	LgwCurrUtcTime.tv_nsec	= 0;

	LgwTmmsUtcTime		= now;
}
#endif


static	int	ProceedRecvPacket(T_lgw_pkt_rx_t    *p,time_t tms)
{
	t_imsg		*msg;
	t_lrr_pkt	uppkt;
	int		sz;
	u_char		*data;
	t_channel	*chan;
	struct		timespec	tv;

	u_int		numchan	= -1;
	char		*namchan= "?";
	u_int		numband	= -1;

	uint8_t		txstatus = 0;

	clock_gettime(CLOCK_REALTIME,&tv);

	chan	= FindChannelForPacket(p);
	if	(chan)
	{
		numchan	= chan->channel;
		namchan	= (char *)chan->name;
		numband	= chan->subband;
	}

	lgw_status(TX_STATUS,&txstatus);
RTL_TRDBG(1,"PKT RECV tms=%09u tus=%09u if=%d status=%s sz=%d freq=%d mod=0x%02x bdw=%s spf=%s ecc=%s rssi=%f snr=%f channel=%d nam='%s' G%d txstatus=%u\n",
		tms,p->count_us,p->if_chain,PktStatusTxt(p->status),p->size,
		p->freq_hz,p->modulation,BandWidthTxt(p->bandwidth),
		SpreadingFactorTxt(p->datarate),CorrectingCodeTxt(p->coderate),
		p->rssi,p->snr,numchan,namchan,numband,txstatus);

	DoPcap((char *)p->payload,p->size);

//	if	(p->status != STAT_NO_CRC && p->status != STAT_CRC_OK)
	if	(p->status == STAT_CRC_BAD)
	{
		LgwNbCrcError++;
		return	0;
	}

	if	(TraceLevel >= 1)
	{
	char	buff[LP_MACLORA_SIZE_MAX*3];
	char	src[64];
	LoRaMAC_t	mf;
	u_char		*pt;

	memset	(&mf,0,sizeof(mf));
	LoRaMAC_decodeHeader(p->payload,p->size,&mf);	// we assumed this is a loramac packet
	pt	= (u_char *)&mf.DevAddr;
	sprintf	(src,"%02x%02x%02x%02x",*(pt+3),*(pt+2),*(pt+1),*pt);

	rtl_binToStr((unsigned char *)p->payload,p->size,buff,sizeof(buff)-10);
	RTL_TRDBG(1,"PKT RECV data='%s' seq=%d devaddr=%s \n",buff,mf.FCnt,src);
	if	(OkDevAddr(src) <= 0)
		return	0;
	}


	if	(p->size > LP_MACLORA_SIZE_MAX)
	{
		LgwNbSizeError++;
		return	0;
	}

	if	(!chan)
	{
		LgwNbChanUpError++;
		return	0;
	}

	data	= (u_char *)malloc(p->size);
	if	(!data)
		return	-1;
	memcpy	(data,p->payload,p->size);

	msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_RECV_DATA,NULL,0);
	if	(!msg)
		return	-2;

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_flag	= LP_RADIO_PKT_UP;
	if	(p->status == STAT_NO_CRC)
	{
		uppkt.lp_flag	= uppkt.lp_flag | LP_RADIO_PKT_NOCRC;
	}
	uppkt.lp_lrrid	= LrrID;
	uppkt.lp_tms	= tms;
	if	(!uppkt.lp_tms)	uppkt.lp_tms	= 1;
	// by default we set linux time
	uppkt.lp_gss	= (time_t)tv.tv_sec;
	uppkt.lp_gns	= (u_int)tv.tv_nsec;
#ifdef	WITH_GPS
	if	(UseGpsTime && Gps_ref_valid)
	{
		// for classb we have to use a precise UTC time otherwise
		// the LRC will not be able to give a good reply to commands
		// like TiminigBeaconRequest
		int	destim;
		int	ret;
		struct timespec butc;
		struct timespec lutc;

		LgwEstimUtc(&lutc);
		ret	= lgw_cnt2utc(Time_reference_gps,p->count_us,&butc);
		if	(ret == LGW_HAL_SUCCESS)
		{
			destim	= LgwDiffMsUtc(&butc,&lutc);
			// TODO find a flag to indicate the time comes from
			// the radio board not from linux
//			uppkt.lp_flag	= uppkt.lp_flag | LP_RADIO_PKT_ZZZZZZ;
			uppkt.lp_gss	= (time_t)butc.tv_sec;
			uppkt.lp_gns	= (u_int)butc.tv_nsec;

RTL_TRDBG(1,"LGW PPS DATE eutc=(%09u,%ums) butc=(%09u,%09u) diff=%d tus=%u\n",
				lutc.tv_sec,lutc.tv_nsec/1000000,
				butc.tv_sec,butc.tv_nsec,
				destim,p->count_us);
		}
	}
#endif

	uppkt.lp_tus		= p->count_us;
	uppkt.lp_chain		= p->rf_chain;
	uppkt.lp_rssi		= p->rssi;
	uppkt.lp_snr		= p->snr;
	uppkt.lp_channel	= chan->channel;
	uppkt.lp_subband	= chan->subband;
	uppkt.lp_spfact		= CodeSpreadingFactor(p->datarate);
	uppkt.lp_correct	= CodeCorrectingCode(p->coderate);
#if	0
	uppkt.lp_frequency	= p->freq_hz;
	uppkt.lp_board		= 0;
#endif
	uppkt.lp_size		= p->size;
	uppkt.lp_payload	= data;

#ifdef LP_TP31
	uppkt.lp_tmoa	= TmoaLrrPacketUp(p);
	RTL_TRDBG(3,"TmoaLrrPacketUp()=%f\n", uppkt.lp_tmoa);
	DcTreatUplink(&uppkt);
#endif

	sz	= sizeof(t_lrr_pkt);
	if	( rtl_imsgDupData(msg,&uppkt,sz) != msg)
//	if	( rtl_imsgCpyData(msg,&uppkt,sz) != msg)
	{
		rtl_imsgFree(msg);
		return	-3;
	}

	rtl_imsgAdd(MainQ,msg);
	return	1;
}

int	LgwDoRecvPacket(time_t tms)
{
	T_lgw_pkt_rx_t	rxpkt[LGW_RECV_PKT];
	T_lgw_pkt_rx_t	*p;
	int			nbpkt;
	int			i;
	int			ret;
	int			nb	= 0;
#if	0
	time_t			t0,t1;
	rtl_timemono(&t0);
	rtl_timemono(&t1);
	RTL_TRDBG(1,"lgw_receive() => %dms\n",ABS(t1-t0));
#endif

	nbpkt	= lgw_receive(LGW_RECV_PKT,rxpkt);
	if	(nbpkt <= 0)
	{
		RTL_TRDBG(9,"PKT nothing to Recv=%d\n",nbpkt);
		return	0;
	}

#ifdef WIRMANA
	LedShotRxLora();
#endif

#if	0
{
	uint32_t	mycnt;
	lgw_get_trigcnt(&mycnt);
	RTL_TRDBG(1,"after lgw_receive lgw_get_trigcnt=%u\n", mycnt);
}
#endif

	LgwNbPacketRecv	+= nbpkt;
	nb	= 0;
	for	(i = 0 ; i < nbpkt ; i++)
	{
		p	= &rxpkt[i];
		if	((ret=ProceedRecvPacket(p,tms)) < 0)
		{
			RTL_TRDBG(1,"PKT RECV not treated ret=%d\n",ret);
		}
		else
		{
			nb++;
		}
	}

	return	nb;
}

static	u_int	WaitSendPacket(int blo,time_t tms,uint8_t *txstatus)
{
	time_t	diff;
	int	j	= 0;

	if	(!blo)
	{
		if	(LgwWaitSending <= 0)
			return	0;
	}
	RTL_TRDBG(1,"PKT SEND enter blocking mode\n");
	do
	{
		j++;
		lgw_status(TX_STATUS,txstatus);
		usleep(1000);	// * 6000 => 6s max
	} while ((*txstatus != TX_FREE) && (j < 6000));
	diff	= rtl_tmmsmono();
	diff	= ABS(diff - tms);

	RTL_TRDBG(3,"PKT sent j=%d status=%d\n",j,*txstatus);
	if	(*txstatus != TX_FREE)
	{
		RTL_TRDBG(0,"PKT SEND status(=%d) != TX_FREE\n",*txstatus);
	}
	return	diff;
}

static	int	SendPacketNow(int blo,t_lrr_pkt *downpkt,T_lgw_pkt_tx_t txpkt)
{
	uint8_t txstatus = 0;
	int	left;
	int	ret;
	int	diff	= 0;
	int	duration = 0;
	u_int	tms;
	int	tempgain;

	left	= LgwNbPacketWait;

	lgw_status(TX_STATUS,&txstatus);
	if	(downpkt->lp_lgwdelay == 0 && txstatus != TX_FREE)
	{
		RTL_TRDBG(1,"PKT busy status=%d left=%d\n",txstatus,left-1);
		LgwNbBusySend++;
		return	-1;
	}
	if	(0 && downpkt->lp_lgwdelay && txstatus != TX_FREE) // TODO
	{
		RTL_TRDBG(1,"PKT busy status=%d left=%d\n",txstatus,left-1);
		LgwNbBusySend++;
		return	-1;
	}

	ChangeChannelFreq(downpkt,&txpkt); //	NFR703

	tempgain	= LgwTempPowerGain();
	txpkt.rf_power	= txpkt.rf_power + tempgain;
	if	(txpkt.rf_power > LgwPowerMax)
		txpkt.rf_power	= LgwPowerMax;
	tms	= rtl_tmmsmono();
	diff	= (time_t)tms - (time_t)downpkt->lp_tms;
	downpkt->lp_stopbylbt	= 0;
	if	((ret=lgw_send(txpkt)) != LGW_HAL_SUCCESS)
	{
#ifdef	WITH_LBT
		if	(ret == LGW_LBT_ISSUE)
		{
			downpkt->lp_stopbylbt	= 1;
			if	(downpkt->lp_beacon)
			{
				LgwBeaconLastDeliveryCause	= LP_CB_LBT;
				goto	stop_by_blt;
			}
			if	(downpkt->lp_classb)
			{
				SetIndic(downpkt,0,-1,-1,LP_CB_LBT);
				goto	stop_by_blt;
			}
			if	(Rx2Channel && Rx2Channel->freq_hz == txpkt.freq_hz)
				SetIndic(downpkt,0,-1,LP_C2_LBT,-1);
			else
				SetIndic(downpkt,0,LP_C1_LBT,-1,-1);
stop_by_blt:
			RTL_TRDBG(1,"PKT send stop by lbt=%d freq=%d\n",ret,
				txpkt.freq_hz);
			return	-3;
		}
#endif
		RTL_TRDBG(0,"PKT send error=%d\n",ret);
		return	-2;
	}
#ifdef WIRMANA
	LedShotTxLora();
#endif
	if	(downpkt->lp_beacon)
	{
		LgwBeaconSentCnt++;
		LgwBeaconLastDeliveryCause	= 0;
	}
	LgwNbPacketSend++;
	lgw_status(TX_STATUS,&txstatus);
	if	(!downpkt->lp_bypasslbt && downpkt->lp_lgwdelay == 0
						&& txstatus != TX_EMITTING)
	{
		RTL_TRDBG(0,"PKT SEND status(=%d) != TX_EMITTING\n",txstatus);
	}
	if	(0 && downpkt->lp_lgwdelay && txstatus != TX_SCHEDULED)	// TODO
	{
		RTL_TRDBG(0,"PKT SEND status(=%d) != TX_SCHEDULED\n",txstatus);
	}

#ifdef LP_TP31
	// must be done only if packet was really sent
	DcTreatDownlink(downpkt);
#endif

RTL_TRDBG(1,"PKT SEND tms=%09u/%d rfchain=%d status=%d sz=%d left=%d freq=%d mod=0x%02x bdw=%s spf=%s ecc=%s pr=%d nocrc=%d ivp=%d pw=%d temppw=%d\n",
	tms,diff,txpkt.rf_chain,txstatus,txpkt.size,left,txpkt.freq_hz,
	txpkt.modulation,BandWidthTxt(txpkt.bandwidth),
	SpreadingFactorTxt(txpkt.datarate),CorrectingCodeTxt(txpkt.coderate),
	txpkt.preamble,txpkt.no_crc,txpkt.invert_pol,txpkt.rf_power,tempgain);


	if	(downpkt->lp_lgwdelay == 0)
	{
		duration	= WaitSendPacket(blo,tms,&txstatus);
	}

	// the packet is now passed to the sx13, compute time added by all
	// treatements LRC/LRR, only if lgwdelay or non blocking mode

	diff	= 0;
	if	(downpkt->lp_lgwdelay || (!blo && !LgwWaitSending))
	{
		u_int	scheduled;	// time before sx13 schedule request

		diff	= ABS(rtl_tmmsmono() - downpkt->lp_tms);

		scheduled		= downpkt->lp_lgwdelay;
		LastTmoaRequested	= ceil(downpkt->lp_tmoa/1000);
		LastTmoaRequested	= LastTmoaRequested +
					(LastTmoaRequested * 10)/100;
		RTL_TRDBG(1,"LGW DELAY tmao request=%dms + sched=%dms\n",
				LastTmoaRequested,scheduled);
		LastTmoaRequested	= LastTmoaRequested + scheduled + 70;
		CurrTmoaRequested	= LastTmoaRequested;
	}
	else
	{
	// TODO meme si no delay il faut compter la requete
		LastTmoaRequested	= 0;
		CurrTmoaRequested	= 0;
	}

	if	(TraceLevel >= 1)
	{
		char	buff[LP_MACLORA_SIZE_MAX*3];
		char	src[64];
		LoRaMAC_t	mf;
		u_char		*pt;
		char		*pktclass = "";

		memset	(&mf,0,sizeof(mf));
		if	(downpkt->lp_beacon == 0)
		{
			// we assumed this is a loramac packet
			LoRaMAC_decodeHeader(txpkt.payload,txpkt.size,&mf);	
			pt	= (u_char *)&mf.DevAddr;
			sprintf	(src,"%02x%02x%02x%02x",
						*(pt+3),*(pt+2),*(pt+1),*pt);
			if	(downpkt->lp_classb)
			{
				diff		= 0;
				pktclass	= "classb";
RTL_TRDBG(1,"PKT SEND classb period=%d sidx=%d sdur=%f window(%09u,%09u) %d/%d/%d\n",
			downpkt->lp_period,downpkt->lp_sidx,downpkt->lp_sdur,
			((downpkt->lp_period-1)*128)+downpkt->lp_gss0,
			downpkt->lp_gns0,
			downpkt->lp_idxtry,downpkt->lp_nbtry,downpkt->lp_maxtry);
			}
		}
		else
		{
			pktclass	= "beacon";
		}

		rtl_binToStr(txpkt.payload,txpkt.size,buff,sizeof(buff)-10);
		if	(downpkt->lp_lgwdelay == 0)
		{
			if	(blo || LgwWaitSending)
			{
RTL_TRDBG(1,"PKT SEND blocking %s dur=%ums data='%s' seq=%d devaddr=%s\n",
				pktclass,duration,buff,mf.FCnt,src);
			}
			else
			{
RTL_TRDBG(1,"PKT SEND noblock %s dur=%fms diff=%ums data='%s' seq=%d devaddr=%s\n",
	pktclass,downpkt->lp_tmoa/1000,diff,buff,mf.FCnt,src);
			}
		}
		else
		{
RTL_TRDBG(1,"PKT SEND async %s dur=%fms diff=%ums data='%s' seq=%d devaddr=%s\n",
	pktclass,downpkt->lp_tmoa/1000,diff,buff,mf.FCnt,src);
		}
	}

	DoPcap((char *)txpkt.payload,txpkt.size);

	return	0;
}

static	void	SetTrigTarget(t_lrr_pkt *downpkt,T_lgw_pkt_tx_t *txpkt)
{
	static	int	classbdelay	= -1;

	if	(classbdelay == -1)
	{
		classbdelay	= CfgInt(HtVarLrr,"classb",-1,"adjustdelay",0);
		RTL_TRDBG(1,"classb.adjustdelay=%d\n",classbdelay);
	}
#ifdef	WITH_GPS
	if	(downpkt->lp_beacon)
	{
		int	ret;
		uint32_t trig_tstamp;
		uint32_t trig_estim;
		struct timespec utc_time;

		utc_time.tv_sec	= downpkt->lp_gss;
		utc_time.tv_nsec= downpkt->lp_gns;
		ret	= lgw_utc2cnt(Time_reference_gps,utc_time,&trig_tstamp);
		if	(ret != LGW_GPS_SUCCESS)
		{
RTL_TRDBG(0,"PKT SEND beacon error lgw_utc2cnt() from (%u,%09u)\n",
			downpkt->lp_gss,downpkt->lp_gns);
			LgwBeaconLastDeliveryCause	= LP_C1_DELAY;
			return;	// TODO
		}
		LgwEstimTrigCnt(&trig_estim);
		txpkt->tx_mode 	= ON_GPS;
		txpkt->count_us	= trig_tstamp - Sx13xxStartDelay;
		downpkt->lp_lgwdelay	=
				ABS(txpkt->count_us - trig_estim ) / 1000;

RTL_TRDBG(1,"PKT SEND beacon trigtarget=%u trigonpps=%u trigestim=%u diffestim=%d pkt=(%u,%09u) utc=(%u,%09u)\n",
		txpkt->count_us,Time_reference_gps.count_us,trig_estim,
		downpkt->lp_lgwdelay,downpkt->lp_gss,downpkt->lp_gns,
		Time_reference_gps.utc.tv_sec,Time_reference_gps.utc.tv_nsec);

		return;
	}
#endif

#ifdef	WITH_GPS
	if	(downpkt->lp_classb)
	{
		int	ret;
		uint32_t trig_tstamp;
		uint32_t trig_estim;
		struct timespec utc_time;

		utc_time.tv_sec	= downpkt->lp_gss;
		utc_time.tv_nsec= downpkt->lp_gns;
		ret	= lgw_utc2cnt(Time_reference_gps,utc_time,&trig_tstamp);
		if	(ret != LGW_GPS_SUCCESS)
		{
RTL_TRDBG(0,"PKT SEND classb error lgw_utc2cnt() from (%u,%09u)\n",
			downpkt->lp_gss,downpkt->lp_gns);
			return;	// TODO
		}
		LgwEstimTrigCnt(&trig_estim);
		txpkt->tx_mode 	= TIMESTAMPED;
		txpkt->count_us	= trig_tstamp - Sx13xxStartDelay + classbdelay;
		downpkt->lp_lgwdelay	= 
				ABS(txpkt->count_us - trig_estim ) / 1000;
RTL_TRDBG(1,"PKT SEND classb trigtarget=%u trigonpps=%u trigestim=%u diffestim=%d adj=%d pkt=(%u,%09u) utc=(%u,%09u)\n",
	txpkt->count_us,Time_reference_gps.count_us,trig_estim,
	downpkt->lp_lgwdelay,classbdelay,downpkt->lp_gss,downpkt->lp_gns,
	Time_reference_gps.utc.tv_sec,Time_reference_gps.utc.tv_nsec);

		return;
	}
#endif

	if	(downpkt->lp_lgwdelay == 0)
	{
		if	(LgwLbtEnable == 0)
		{
			txpkt->tx_mode 	= IMMEDIATE;
		RTL_TRDBG(1,"LRR DELAY -> tx mode immediate tms=%u tus=%u\n",
			downpkt->lp_tms,downpkt->lp_tus);
			return;
		}

		// RDTP-857 LBT is enable HAL refuses IMMEDIATE mode
		// => we force TIMESTAMPED mode in 5ms
		uint32_t	trig_tstamp;


		lgw_reg_w(LGW_GPS_EN,0);
		lgw_get_trigcnt(&trig_tstamp);
		lgw_reg_w(LGW_GPS_EN,1);

		downpkt->lp_tus		= trig_tstamp + (5*1000); // + 5ms
		downpkt->lp_delay	= 0;
		downpkt->lp_bypasslbt	= 1;

		RTL_TRDBG(1,"LRR DELAY -> tx mode immediate with LBT tms=%u tus=%u\n",
			downpkt->lp_tms,downpkt->lp_tus);
		// and no return to force TIMESTAMPED mode
	}

	txpkt->tx_mode 	= TIMESTAMPED;
	txpkt->count_us	= downpkt->lp_tus;
	txpkt->count_us	= txpkt->count_us + (downpkt->lp_delay * 1000);
	txpkt->count_us	= txpkt->count_us - Sx13xxStartDelay;

RTL_TRDBG(3,"LGW DELAY trigtarget=%u trigorig=%u\n",
		txpkt->count_us,downpkt->lp_tus);

	return;
}

static	int	SendPacket(t_imsg  *msg)
{
	t_lrr_pkt	*downpkt;
	T_lgw_pkt_tx_t 	txpkt;
	t_channel	*chan;
	int	ret;
#ifndef USELIBLGW3
	bool	txenable[]	= LGW_RF_TX_ENABLE;
#endif
	int	blo	= 0;

	downpkt	= msg->im_dataptr;

	if	(downpkt->lp_channel >= MaxChannel ||
		TbChannel[downpkt->lp_channel].name[0] == '\0')
	{
		LgwNbChanDownError++;	
		return	0;
	}
	chan	= &TbChannel[downpkt->lp_channel];

	memset	(&txpkt,0,sizeof(txpkt));

//	txpkt.freq_hz = 868100000;
//	txpkt.modulation = MOD_LORA;
//	txpkt.bandwidth = BW_125KHZ;
//	txpkt.datarate = DR_LORA_SF10;
//	txpkt.coderate = CR_LORA_4_5;

	if	(downpkt->lp_chain >= LGW_RF_CHAIN_NB)
	{
		downpkt->lp_chain	= 0;
	}
#ifdef USELIBLGW3
	if	(LgwTxEnable[downpkt->lp_chain] == 0)
#else
	if	(txenable[downpkt->lp_chain] == 0)
#endif
	{
		RTL_TRDBG(1,"TX is disable on chain=%d => 0\n",
							downpkt->lp_chain);
		downpkt->lp_chain	= 0;
	}

	SetTrigTarget(downpkt,&txpkt);
	txpkt.rf_chain 		= downpkt->lp_chain;
	txpkt.freq_hz 		= chan->freq_hz;
	txpkt.modulation 	= chan->modulation;
	txpkt.bandwidth 	= chan->bandwidth;
	txpkt.datarate 		= DecodeSpreadingFactor(downpkt->lp_spfact);
	txpkt.coderate		= DecodeCorrectingCode(downpkt->lp_correct);
	txpkt.invert_pol	= LgwInvertPol;
	txpkt.no_crc 		= LgwNoCrc;
	txpkt.no_header 	= LgwNoHeader;
	txpkt.preamble 		= LgwPreamble;
	txpkt.rf_power 		=
		chan->power - AntennaGain[0] + CableLoss[0];	// TODO index
	if	(downpkt->lp_beacon)
	{
		txpkt.invert_pol	= LgwInvertPolBeacon;
		txpkt.no_crc		= 1;
		txpkt.no_header		= 1;
		txpkt.preamble 		= 10;
	}

	if ((downpkt->lp_flag&LP_RADIO_PKT_ACKMAC) == LP_RADIO_PKT_ACKMAC)
		txpkt.preamble = LgwPreambleAck;

	if ((downpkt->lp_flag&LP_RADIO_PKT_SZPREAMB) == LP_RADIO_PKT_SZPREAMB)
		txpkt.preamble = downpkt->lp_szpreamb;

	if ((downpkt->lp_flag&LP_RADIO_PKT_ACKDATA) == LP_RADIO_PKT_ACKDATA
	&& (downpkt->lp_flag&LP_RADIO_PKT_802154) == LP_RADIO_PKT_802154)
	{
		u_char	ackseq	= downpkt->lp_opaque[2];
		u_char	ackpend	= (downpkt->lp_opaque[0] >> 4) & 1;
		u_char	ackack	= (downpkt->lp_opaque[0] >> 5) & 1;

		u_char	dataseq	= downpkt->lp_payload[2];
		u_char	datapend= (downpkt->lp_payload[0] >> 4) & 1;
		u_char	dataack	= (downpkt->lp_payload[0] >> 5) & 1;

RTL_TRDBG(1,"PKT SEND ACK(seq=%u,p=%u,a=%u)+wait=%u+DATA(seq=%u,p=%u,a=%u)\n",
			ackseq,ackpend,ackack,
			LgwAckData802Wait,
			dataseq,datapend,dataack);
		txpkt.size = 5;		// size for an ACK frame 802
		memcpy	(txpkt.payload,downpkt->lp_opaque,txpkt.size);
		ret	= SendPacketNow(blo=1,downpkt,txpkt);
		if	(ret < 0)
		{
			RTL_TRDBG(0,"SendPacketNow() error => %d\n",ret);
		}
		usleep(LgwAckData802Wait*1000);	// 10 ms
	}

	txpkt.size = downpkt->lp_size;
	memcpy	(txpkt.payload,downpkt->lp_payload,downpkt->lp_size);
#if	0	// no more free when trying to send so we can retry ...
	free	(downpkt->lp_payload);
	downpkt->lp_payload	= NULL;
#endif

	ret	= SendPacketNow(blo,downpkt,txpkt);
	if	(ret < 0)
	{
		RTL_TRDBG(0,"SendPacketNow() error => %d\n",ret);
		return	0;
	}

	return	1;
}

static	void	FreeMsgAndPacket(t_imsg *msg,t_lrr_pkt *downpkt)
{
	if	(downpkt && downpkt->lp_payload)
	{
		free(downpkt->lp_payload);
		downpkt->lp_payload	= NULL;
	}
	rtl_imsgFree(msg);
}

static	int	Rx1StopByLbtTryRx2(t_imsg *msg,t_lrr_pkt *downpkt)
{
	if	(!msg || !downpkt)
		return	0;
	if	(Rx2Channel == NULL)
		return	0;

	if	(downpkt->lp_stopbylbt == 0)
	{	// not stopped
		return	0;
	}
	if	(downpkt->lp_delay == 0 || downpkt->lp_beacon || downpkt->lp_classb)
	{	// classC ou beacon ou classB
		return	0;
	}
	if	((downpkt->lp_flag&LP_RADIO_PKT_RX2)==LP_RADIO_PKT_RX2)
	{	// already RX2 (by LRC)
		return	0;
	}
	if	(downpkt->lp_rx2lrr)
	{	// already RX2 (by LRR)
		return	0;
	}

RTL_TRDBG(1,"PKT SEND RX1 stopped by LBT try RX2\n");

	downpkt->lp_rx2lrr = 1;
	downpkt->lp_delay += 1000;
	AutoRx2Settings(downpkt);
	return	1;
}

// called by radio thread lgw_gen.c:LgwMainLoop()
int	LgwDoSendPacket(time_t now)
{
	t_imsg	*msg;
	int	nbs	= 0;
	int	left;
	int	ret;
	static	int	classbuseall	= -1;

	if	(classbuseall == -1)
	{
		classbuseall	= CfgInt(HtVarLrr,"classb",-1,"useallslot",0);
		RTL_TRDBG(1,"classb.useallslot=%d\n",classbuseall);
	}

	if	((msg= rtl_imsgGet(LgwSendQ,IMSG_BOTH)) != NULL)
	{
		t_lrr_pkt	*downpkt;

		downpkt	= msg->im_dataptr;
		left	= rtl_imsgCount(LgwSendQ);
		if	(left > 0 && left > LgwNbPacketWait)
		{
			LgwNbPacketWait	= left;
		}
#ifdef	WITH_GPS
		if	(downpkt->lp_beacon && downpkt->lp_delay == 0)
		{
			int		destim;
			int		odelay;
			int		delay;	// postpone delay
			struct timespec *utc;

			// delayed thread/board and use UTC given by GPS+estim
			struct timespec lutc;
			struct timespec butc;
			utc	= &lutc;
			LgwEstimUtc(utc);
			destim	= LgwEstimUtcBoard(&butc);
			if	(destim != LGW_HAL_SUCCESS)
			{
				RTL_TRDBG(0,"can not estim UTC board for beacon\n");
				LgwBeaconLastDeliveryCause	= LP_C1_DELAY;
				return	0;
			}
			destim	= LgwDiffMsUtc(&butc,&lutc);
			delay	= LgwPacketDelayMsFromUtc(downpkt,&butc);
			odelay	= delay;
			delay	= odelay - 300;
			

RTL_TRDBG(1,
"PKT SEND beacon postpone=%d/%d pkt=(%u,%09u) eutc=(%u,%ums) butc=(%u,%09u) diff=%d\n",
					delay,odelay,
					downpkt->lp_gss,downpkt->lp_gns,
					utc->tv_sec,utc->tv_nsec/1000000,
					butc.tv_sec,butc.tv_nsec,destim);

			if	(delay <= 0)
			{
				RTL_TRDBG(0,"too old beacon %dms\n",delay);
				LgwBeaconLastDeliveryCause	= LP_C1_DELAY;
				return	0;
			}

			downpkt->lp_trip	= 0;
			downpkt->lp_delay	= delay;
			downpkt->lp_lgwdelay	= 1;
			rtl_imsgAddDelayed(LgwSendQ,msg,delay);
			return	0;
		}
#endif
		if	(downpkt->lp_beacon && downpkt->lp_delay != 0)
		{
			RTL_TRDBG(1,"PKT SEND beacon retrieved\n");
			goto	send_pkt;
		}
#ifdef	WITH_GPS
		if	(downpkt->lp_classb && downpkt->lp_delay == 0)
		{
			int	odelay;
			int	delay;
			int	ret;
			struct timespec eutc;
			struct timespec *utc;

			utc	= &eutc;
			LgwEstimUtc(utc);
			ret	= LgwNextPingSlot(downpkt,utc,300,&delay);
			if	(ret <= 0)
			{
				SetIndic(downpkt,0,-1,-1,LP_CB_DELAY);
				SendIndicToLrc(downpkt);
				FreeMsgAndPacket(msg,downpkt);
				RTL_TRDBG(0,"PKT SEND classb too late\n");
				return	0;
			}
			odelay	= delay;
			delay	= odelay - 200;
			if	(0 && CurrTmoaRequested >= odelay)
			{
				downpkt->lp_delay	= 0;
				rtl_imsgAddDelayed(LgwSendQ,msg,CurrTmoaRequested+3);
				return	0;
			}

RTL_TRDBG(1,
	"PKT SEND classb postpone=%d/%d period=%d sidx=%d sdur=%f pkt=(%u,%09u) eutc=(%u,%ums)\n",
			delay,odelay,
			downpkt->lp_period,downpkt->lp_sidx,downpkt->lp_sdur,
			downpkt->lp_gss,downpkt->lp_gns,
			utc->tv_sec,utc->tv_nsec/1000000);

			downpkt->lp_trip	= 0;
			downpkt->lp_delay	= delay;
			downpkt->lp_lgwdelay	= 1;
			rtl_imsgAddDelayed(LgwSendQ,msg,delay);
			return	0;
		}
#endif
		if	(downpkt->lp_classb && downpkt->lp_delay != 0)
		{
			RTL_TRDBG(1,"PKT SEND classb retrieved\n");
			if	(CurrTmoaRequested)
			{
RTL_TRDBG(1,"PKT SEND classb avoid collision repostpone=%d\n",
						CurrTmoaRequested);
				downpkt->lp_delay	= 0;
				rtl_imsgAddDelayed(LgwSendQ,msg,CurrTmoaRequested+3);
				return	0;
			}
			goto	send_pkt;
		}

		if	(downpkt->lp_delay == 0
			&& ABS(now - downpkt->lp_tms) > MaxReportDnImmediat)
		{	// mode immediate
		RTL_TRDBG(1,"PKT SEND NODELAY not sent after 60s => dropped\n");
				SetIndic(downpkt,0,LP_C1_MAXTRY,LP_C2_MAXTRY,-1);
				SendIndicToLrc(downpkt);
				FreeMsgAndPacket(msg,downpkt);
				return	0;
		}
		if	(downpkt->lp_delay == 0 && CurrTmoaRequested)
		{	// mode immediate
		RTL_TRDBG(1,"PKT SEND NODELAY avoid collision repostpone=%d\n",
						CurrTmoaRequested);
			rtl_imsgAddDelayed(LgwSendQ,msg,CurrTmoaRequested+3);
			return	0;
		}
send_pkt:
		if	(classbuseall && downpkt->lp_classb 
						&& downpkt->lp_nbslot <= 16)
		{
			t_lrr_pkt	pkt;
			t_imsg		*repeat;

			memcpy(&pkt,downpkt,sizeof(t_lrr_pkt));
			pkt.lp_payload = (u_char *)malloc(pkt.lp_size);
			memcpy(pkt.lp_payload,downpkt->lp_payload,pkt.lp_size);
			pkt.lp_delay	= 0;
			repeat	= rtl_imsgAlloc(IM_DEF,IM_LGW_SEND_DATA,NULL,0);
			rtl_imsgDupData(repeat,&pkt,sizeof(t_lrr_pkt));
			rtl_imsgAddDelayed(LgwSendQ,repeat,1000);
		}
		ret	= SendPacket(msg);
		if	(ret > 0)
		{
			SetIndic(downpkt,1,-1,-1,-1);
			nbs	+= ret;
		}
		else
		{
RTL_TRDBG(1,"SendPacket() beacon=%d classb=%d classc=%d rx2=%d lbt=%d error => %d\n",
			downpkt->lp_beacon,downpkt->lp_classb,
			downpkt->lp_delay == 0 ? 1 : 0,
			downpkt->lp_rx2lrr,
			downpkt->lp_stopbylbt,ret);

			if (LgwLbtEnable && downpkt->lp_delay == 0 
						&& downpkt->lp_stopbylbt)
			{	// retry packet immediate if stopped by LBT
				downpkt->lp_stopbylbt	= 0;
				rtl_imsgAddDelayed(LgwSendQ,msg,3000);
				return	0;
			}
			if (LgwLbtEnable && downpkt->lp_stopbylbt 
					&& Rx1StopByLbtTryRx2(msg,downpkt))
			{	// retry classA/RX1 on RX2 if stopped by LBT
				downpkt->lp_stopbylbt	= 0;
				rtl_imsgAddDelayed(LgwSendQ,msg,950);
				return	0;
			}
			SetIndic(downpkt,0,-1,-1,-1);
		}
		SendIndicToLrc(downpkt);

		FreeMsgAndPacket(msg,downpkt);
	}
	return	nbs;
}
