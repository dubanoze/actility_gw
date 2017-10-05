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

#undef	WITH_SX1301_X1		// !!!
#ifndef WITH_SX1301_X8
#define	WITH_SX1301_X8		// !!!
#endif
#include "semtech.h"
#include "headerloramac.h"

#include "xlap.h"
#include "define.h"
#include "infrastruct.h"
#include "struct.h"
#include "cproto.h"
#include "extern.h"

#ifdef WITH_GPS
sx1301ar_tref_t Gps_time_ref[SX1301AR_MAX_BOARD_NB];
int Gps_ref_valid; 
time_t		LgwTmmsUtcTime;		// tmms mono of the last UTC/GPS
struct timespec	LgwCurrUtcTime;		// last UTC + nsec computation
struct timespec	LgwBeaconUtcTime;	// last beacon
#endif /* WITH_GPS */

static	int	_LgwStarted;
static	int	LgwAntennas[SX1301AR_MAX_BOARD_NB][SX1301AR_BOARD_CHIPS_NB];	// Antenna id connected to each chip
#ifdef	TEST_ONLY
static	int	LgwWaitSending	= 1;
#else
static	int	LgwWaitSending	= 0;
#endif

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
	return	"SX1301_AR";
}

// First version: if at least one chip is not free the status will
// be 'not free'
int IsBoardStatusTxFree(int board,sx1301ar_tstat_t *txstatus)
{
	int	chip;

	for (chip=0; chip<SX1301AR_BOARD_CHIPS_NB; chip++)
	{
		sx1301ar_tx_status(board,chip,txstatus);
		if (*txstatus != TX_FREE)
		{
			RTL_TRDBG(3,"IsBoardStatusTxFree: chip%d status not TX_FREE = %d\n",
				chip, *txstatus);
			return 0;
		}
	}
	RTL_TRDBG(3,"IsBoardStatusTxFree: status = TX_FREE\n");
	return 1;
}

int	LgwTxFree(uint8_t board,uint8_t *txstatus)
{
	sx1301ar_tstat_t	status;

	*txstatus	= 0;

	IsBoardStatusTxFree(board, &status);
	*txstatus	= status;
	if	(*txstatus != TX_FREE)
		return	0;
	return	1;
}

char	*PktStatusTxt(sx1301ar_pstat_t status)
{
	static	char	buf[64];

	switch	(status)
	{
	case	STAT_UNDEFINED :
		return	"UNDEFINED";
	case	STAT_NO_CRC :
		return	"CRCNO";
	case	STAT_CRC_ERR :
		return	"CRCERR";
	case	STAT_CRC_OK :
		return	"CRCOK";
	}
	sprintf	(buf,"CRC?(0x%x)",status);
	return	buf;
}

char	*BandWidthTxt(bandwidth)
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

#if 0
static	unsigned int	LgwTimeStamp()
{
	return	0;
}
#endif

int	LgwLINKUP()
{
	return	1;
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
			if	((c->datarate & up->modrate) == up->modrate)
				return	c;
		}
	}

not_found :
RTL_TRDBG(0,"no chan(%d) found for frz=%d mod=0x%02x bdw=0x%02x dtr=0x%02x\n",
	sortindex,up->freq_hz,up->modulation,up->bandwidth,up->modrate);

	return	NULL;
}


static	void LgwDumpRfConf(FILE *f, int board, int rfi, sx1301ar_chip_cfg_t *rfconf)
{
	if	(rfconf->enable == 0)
		return;
	RTL_TRDBG(1,"chip%d enab=%d frhz=%d rfchain=%d antennaid=%d\n",rfi,rfconf->enable,
		rfconf->freq_hz, rfconf->rf_chain, LgwAntennas[board][rfi]);

	if	(f == NULL)
		return;
	fprintf(stderr,"chip%d enab=%d frhz=%d rfchain=%d antennaid=%d\n",rfi,rfconf->enable,
		rfconf->freq_hz, rfconf->rf_chain, LgwAntennas[board][rfi]);
}

static	void LgwDumpIfConf(FILE *f, int ifi, sx1301ar_chan_cfg_t *ifconf, int chan)
{
	if	(ifconf->enable == 0)
		return;

	RTL_TRDBG(1,"phychan%d enab=%d frhz=%d halchan=%d(%d:%d) bandw=0x%x datar=0x%x\n",
		ifi,ifconf->enable,
		ifconf->freq_hz,
		chan,chan>>4,chan&0x0F,
		ifconf->bandwidth,ifconf->modrate);

	if	(f == NULL)
		return;
	fprintf(f,"phychan%d enab=%d frhz=%d halchan=%d(%d:%d) bandw=0x%x datar=0x%x\n",
		ifi,ifconf->enable,
		ifconf->freq_hz,
		chan,chan>>4,chan&0x0F,
		ifconf->bandwidth,ifconf->modrate);
	fflush(f);
}

static	void LgwDumpBdConf(FILE *f, int board, sx1301ar_board_cfg_t *bdconf)
{
	int	rfc, i;
	char	key[80], tmp[20];

	key[0] = '\0';
	for (i=0; i<SX1301AR_BOARD_AES_KEY_SIZE/8; i++)
	{
		sprintf(tmp, "%02X", bdconf->aes_key[i]);
		strcat(key, tmp);
	}

	RTL_TRDBG(1,"board%d rxfrhz=%d public=%d roomtemp=%d ad9361temp=%d aeskey='%s'\n",
		board, bdconf->rx_freq_hz, bdconf->loramac_public, bdconf->room_temp_ref,
		bdconf->ad9361_temp_ref, key);

	if	(f)
	{
		fprintf(f,"board%d rxfrhz=%d public=%d roomtemp=%d ad9361temp=%d aeskey='%s'\n",
			board, bdconf->rx_freq_hz, bdconf->loramac_public, bdconf->room_temp_ref,
			bdconf->ad9361_temp_ref, key);
	}
	for	(rfc = 0 ; rfc < SX1301AR_BOARD_RFCHAIN_NB ; rfc++)
	{
		RTL_TRDBG(1,"  rfchain[%d]: rxenable=%d txenable=%d rssi=%f rssioffsetcoeffa=%d rssioffsetcoeffb=%d\n",
			rfc, bdconf->rf_chain[rfc].rx_enable, bdconf->rf_chain[rfc].tx_enable,
			bdconf->rf_chain[rfc].rssi_offset, bdconf->rf_chain[rfc].rssi_offset_coeff_a,
			bdconf->rf_chain[rfc].rssi_offset_coeff_b);
		if (f)
		{
			fprintf(f,"  rfchain[%d]: rxenable=%d txenable=%d rssi=%f rssioffsetcoeffa=%d rssioffsetcoeffb=%d\n",
				rfc, bdconf->rf_chain[rfc].rx_enable, bdconf->rf_chain[rfc].tx_enable,
				bdconf->rf_chain[rfc].rssi_offset, bdconf->rf_chain[rfc].rssi_offset_coeff_a,
				bdconf->rf_chain[rfc].rssi_offset_coeff_b);
		}
	}
}

static	void LgwDumpLutConf(FILE *f,int l, sx1301ar_tx_gain_t *lut)
{
	RTL_TRDBG(1,"lut%d power=%2.2f fpga_gain=%d atten=%d vref=%d word=%d coeff_a=%d coeff_b=%d\n",
		l, lut->rf_power, lut->fpga_dig_gain, lut->ad9361_gain.atten, 
		lut->ad9361_gain.auxdac_vref, lut->ad9361_gain.auxdac_word,
		lut->ad9361_tcomp.coeff_a, lut->ad9361_tcomp.coeff_b);

	if	(f == NULL)
		return;
	fprintf(f,"lut%d power=%2.2f fpga_gain=%d atten=%d vref=%d word=%d coeff_a=%d coeff_b=%d\n",
		l, lut->rf_power, lut->fpga_dig_gain, lut->ad9361_gain.atten, 
		lut->ad9361_gain.auxdac_vref, lut->ad9361_gain.auxdac_word,
		lut->ad9361_tcomp.coeff_a, lut->ad9361_tcomp.coeff_b);
	fflush(f);
}

#include "spi_linuxdev_ar.h"
#ifdef CISCOMS
#define LINUXDEV_PATH_DEFAULT   "/dev/spidev1.0"
#else
#define LINUXDEV_PATH_DEFAULT   "/dev/spidev0.0"
#endif

int	LgwConfigureLut(FILE *f, sx1301ar_tx_gain_lut_t *txlut, int board, int rfc, int config)
{
	char	lutsection[64];
	int	lut;
	char	*pt;

	*txlut = sx1301ar_init_tx_gain_lut();
	sprintf	(lutsection,"lut/%d/%d",board,rfc);
	pt	= CfgStr(HtVarLgw,lutsection,-1,"0","");
	if	(!pt || !*pt)
	{
		RTL_TRDBG(0,"LUT '%s' not found, use lut/0/0\n",lutsection);
		strcpy	(lutsection,"lut/0/0");
	}

	memset(txlut,0,sizeof(sx1301ar_tx_gain_lut_t));
	for (lut = 0; lut < SX1301AR_BOARD_MAX_LUT_NB; lut++)
	{
		char	var[64];
		int	pow, dig, att, dvr, dwo, ca, cb;

		sprintf	(var,"%d",lut);
		pt	= CfgStr(HtVarLgw,lutsection,-1,var,"");
		if	(!pt || !*pt)	continue;
		txlut->size++;
		sscanf	(pt,"%d%d%d%d%d%d%d",&pow,&dig,&att,&dvr,&dwo,&ca,&cb);
		txlut->lut[lut].rf_power			= (int8_t)pow;	// signed !!!
		txlut->lut[lut].fpga_dig_gain		= (uint8_t)dig;
		txlut->lut[lut].ad9361_gain.atten	= (uint16_t)att;
		txlut->lut[lut].ad9361_gain.auxdac_vref	= (uint8_t)dvr;
		txlut->lut[lut].ad9361_gain.auxdac_word	= (uint16_t)dwo;
		txlut->lut[lut].ad9361_tcomp.coeff_a	= (int16_t)ca;
		txlut->lut[lut].ad9361_tcomp.coeff_b	= (int16_t)cb;
		if (f)
			LgwDumpLutConf(f, lut, &(txlut->lut[lut]));
	}
	if	(config)
	{
		RTL_TRDBG(1,"%s %s configured nblut=%d\n",
			lutsection,txlut->size==0?"not":"",txlut->size);
	}
	return 0;
}

sx1301ar_bband_t	LgwGetFreqBand()
{
	if (!IsmBand || !*IsmBand)
	{
		RTL_TRDBG(1,"IsmBand empty, use freq band UNKNOWN\n");
		return BRD_FREQ_BAND_UNKNOWN;
	}

	if (strstr(IsmBand, "868"))
	{
		RTL_TRDBG(1,"freq band used for '%s' is EU868\n", IsmBand);
		return BRD_FREQ_BAND_EU868;
	}
		
	RTL_TRDBG(1,"freq band used for '%s' is US915\n", IsmBand);
	return BRD_FREQ_BAND_US915;
}

int	LgwConfigure(int hot,int config)
{
	unsigned long long ull = 0, ull2 = 0;
	sx1301ar_board_cfg_t bdconf;
	sx1301ar_chip_cfg_t rfconf;
	sx1301ar_chan_cfg_t ifconf;
	int	board;
	int	rfc;	// rfchain
	int	rfi;
	int	ifi;
	int	ret;
	int	i;
	char	def[40];
	char	section[64];
	char	*key;
	char	file[PATH_MAX];
	FILE	*f	= NULL;

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

	for	(board = 0 ; board < LgwBoard && board < SX1301AR_MAX_BOARD_NB ; board++)
	{
		ull = 0;
		ull2 = 0;
		sprintf	(section,"board:%d",board);
		bdconf = sx1301ar_init_board_cfg();
		bdconf.freq_band	= LgwGetFreqBand();
		bdconf.rx_freq_hz	= CfgInt(HtVarLgw,section,-1,"freqhz",0);
		bdconf.loramac_public	= CfgInt(HtVarLgw,section,-1,"public",1);

		// use HtVarLrr instead of HtVarLgw because aeskey should be
		// set in custom.ini file
		key = CfgStr(HtVarLrr,section,-1,"aeskey",NULL);
		if (key && *key)
		{
			i = sscanf(key, "%16llx%16llx", &ull, &ull2);
			if ( i != 2 )
			{
				RTL_TRDBG(0, "ERROR: failed to parse hex string for aeskey\n");
			}
			else
			{
				RTL_TRDBG(3, "aeskey is configured to %016llX%016llX\n", ull, ull2 );
			}
		}
		bdconf.room_temp_ref	= CfgInt(HtVarLgw,section,-1,"roomtemp",SX1301AR_DEFAULT_ROOM_TEMP_REF);
		bdconf.ad9361_temp_ref	= CfgInt(HtVarLgw,section,-1,"ad9361temp",SX1301AR_DEFAULT_AD9361_TEMP_REF);
            	if (ull)
		{
			for (i=0; i<8; i++)
			{
				bdconf.aes_key[i]   = (uint8_t)((ull  >> (56 - i*8)) & 0xFF);
				bdconf.aes_key[i+8] = (uint8_t)((ull2 >> (56 - i*8)) & 0xFF);
			}
		}
		sprintf(def, "%f", SX1301AR_DEFAULT_RSSI_OFFSET);
		for	(rfc = 0 ; rfc < SX1301AR_BOARD_RFCHAIN_NB ; rfc++)
		{
			sprintf	(section,"rfconf:%d:%d",board,rfc);
			bdconf.rf_chain[rfc].rssi_offset	= atof(CfgStr(HtVarLgw,section,-1,"rssioffset",def));
			bdconf.rf_chain[rfc].rssi_offset_coeff_a = CfgInt(HtVarLgw,section,-1,"rssioffsetcoeffa",0);
			bdconf.rf_chain[rfc].rssi_offset_coeff_b = CfgInt(HtVarLgw,section,-1,"rssioffsetcoeffb",0);
			bdconf.rf_chain[rfc].rx_enable	= CfgInt(HtVarLgw,section,-1,"rxenable",1);
			bdconf.rf_chain[rfc].tx_enable	= CfgInt(HtVarLgw,section,-1,"txenable",1);
			LgwConfigureLut(f, &bdconf.rf_chain[rfc].tx_lut, board, rfc, config);
		}
		LgwDumpBdConf(f,board,&bdconf);
		if	(config)
		{
			ret	= sx1301ar_conf_board(board,&bdconf);
			if	(ret <= LGW_HAL_ERROR)
			{
				RTL_TRDBG(0,"BD%d cannot be configured ret=%d '%s'\n",
					board, ret, sx1301ar_err_message(sx1301ar_errno));
				if	(f)
				{
					fprintf(f,"BD%d cannot be configured ret=%d '%s'\n",
						board, ret, sx1301ar_err_message(sx1301ar_errno));
					fclose(f);
				}
				return	-1;
			}
		}

		for	(rfi = 0 ; rfi < SX1301AR_BOARD_CHIPS_NB ; rfi++)
		{
			sprintf	(section,"chip:%d:%d",board,rfi);
			rfconf = sx1301ar_init_chip_cfg();
			rfconf.enable	= CfgInt(HtVarLgw,section,-1,"enable",0);
			rfconf.freq_hz	= CfgInt(HtVarLgw,section,-1,"freqhz",0);
			rfconf.rf_chain = CfgInt(HtVarLgw,section,-1,"rfchain",0);
			LgwAntennas[board][rfi] = CfgInt(HtVarLgw,section,-1,"antid",0);
			if	(rfconf.enable == 0 || rfconf.freq_hz == 0)	
				continue;
			LgwDumpRfConf(f,board,rfi,&rfconf);
			if	(config)
			{
				ret	= sx1301ar_conf_chip(board,rfi,&rfconf);
				if	(ret <= LGW_HAL_ERROR)
				{
					RTL_TRDBG(0,"RF%d cannot be configured ret=%d '%s'\n",
									rfi,ret, sx1301ar_err_message(sx1301ar_errno));
					if	(f)
					{
						fprintf(f,"RF%d cannot be configured ret=%d '%s'\n",
									rfi,ret, sx1301ar_err_message(sx1301ar_errno));
						fclose(f);
					}
					return	-1;
				}
			}
		}

		for	(rfi = 0 ; rfi < SX1301AR_BOARD_CHIPS_NB ; rfi++)
		{
			sprintf	(section,"chip:%d:%d",board,rfi);
			rfconf.enable	= CfgInt(HtVarLgw,section,-1,"enable",0);
			rfconf.freq_hz	= CfgInt(HtVarLgw,section,-1,"freqhz",0);
			if	(rfconf.enable == 0 || rfconf.freq_hz == 0)	
				break;

			// SX1301AR_CHIP_MULTI_NB+1 in order to configure the "stand-alone" channel
			for	(ifi = 0 ; ifi < SX1301AR_CHIP_MULTI_NB+2 ; ifi++)
			{
				uint8_t	chan	= 0;

				sprintf	(section,"ifconf:%d:%d:%d",board,rfi,ifi);
				memset(&ifconf,0,sizeof(ifconf));
				ifconf = sx1301ar_init_chan_cfg();

				// for compatibility with existing configurations, if "stand-alone" channel
				// is not configured just ignore it
				if (rfi >= SX1301AR_CHIP_MULTI_NB && CfgStr(HtVarLgw,section,-1,"enable",NULL) == NULL)
					continue;

				if (rfi != 0 && CfgStr(HtVarLgw,section,-1,"enable",NULL) == NULL)
				{
					// Print this trace only once
					if (ifi == 0)
						RTL_TRDBG(1,"No ifconf for chip %d, use the same as board 0 chip 0\n", rfi);
					sprintf	(section,"ifconf:0:0:%d",ifi);
				}

				ifconf.enable	= CfgInt(HtVarLgw,section,-1,"enable",0);
				ifconf.freq_hz	= CfgInt(HtVarLgw,section,-1,"freqhz",0);
				if	(ifi != SX1301AR_CHIP_MULTI_NB+1 && (ifconf.enable == 0 || ifconf.freq_hz == 0))
					continue;
				ifconf.bandwidth= CfgInt(HtVarLgw,section,-1,"bandwidth",0);
				if	(ifconf.bandwidth == 0)
					ifconf.bandwidth	= BW_125KHZ;
				ifconf.modrate	= CfgInt(HtVarLgw,section,-1,"datarate",0);
				if	(ifconf.modrate == 0)
					ifconf.modrate		= DR_LORA_MULTI;
				else
					ifconf.modrate		= DecodeSpreadingFactor(ifconf.modrate);

				// Tektelic requires that all channels must be configured,
				// even FSK channel
				if (ifi == SX1301AR_BOARD_CHIPS_NB+1)
				{
					// use same freq as for stand-alone chan
					sprintf	(section,"ifconf:%d:%d:%d",board,rfi,ifi-1);
					ifconf.freq_hz	= CfgInt(HtVarLgw,section,-1,"freqhz",0);
					ifconf.bandwidth	= BW_125KHZ;
					ifconf.modrate		= MR_10000;
					ifconf.enable		= 1;
				}

				chan	= ((uint8_t)rfi<<4)+(uint8_t)ifi;

				LgwDumpIfConf(f,ifi,&ifconf,chan);
				if	(config)
				{
					ret	= sx1301ar_conf_chan(board,chan,&ifconf);
					if	(ret <= LGW_HAL_ERROR)
					{
						RTL_TRDBG(0,"IF%d cannot be configured ret=%d '%s'\n",
										ifi,ret, sx1301ar_err_message(sx1301ar_errno));
						if	(f)
						{
							fprintf(f,"IF%d cannot be configured ret=%d '%s'\n",
										ifi,ret, sx1301ar_err_message(sx1301ar_errno));
							fclose(f);
						}
						return	-1;
					}
				}
			}
		}
	}

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

int	LgwStart()
{
	int	ret, b;
	FILE	*chk=NULL;
	char	file[PATH_MAX];

	sprintf	(file,"%s/var/log/lrr/radioparams.txt",RootAct);
	chk	= fopen(file,"a");
	for (b=0; b<LgwBoard; b++)
	{
		RTL_TRDBG(1,"RADIO starting (board %d) ...\n", b);
		ret	= sx1301ar_start(b);
		if	(ret <= LGW_HAL_ERROR)
		{
			RTL_TRDBG(0,"BOARD%d RADIO cannot be started ret=%d '%s'\n",
				b, ret, sx1301ar_err_message(sx1301ar_errno));
			if	(chk)
			{
				fprintf(chk,"BOARD%d RADIO cannot be started ret=%d '%s'\n",
					b, ret, sx1301ar_err_message(sx1301ar_errno));
				fclose(chk);
			}
			return	-1;
		}

		RTL_TRDBG(1,"RADIO board %d started ret=%d\n", b, ret);
		if	(chk)
			fprintf(chk,"RADIO board %d started ret=%d\n",b,ret);
	}
	if (chk)
		fclose(chk);
	_LgwStarted	= 1;
	return	0;
}

int	LgwStarted()
{
	return	_LgwStarted;
}

void	LgwStop()
{
	int	b;
	int	ret = -1;
	int	err = 0;

	RTL_TRDBG(1,"RADIO stopping ...\n");
	for (b=0; b<LgwBoard; b++)
	{
		ret = sx1301ar_stop(b);
		if (ret != 0)
		{
			RTL_TRDBG(0, "ERROR: Board %d error stopping radio ret=%d '%s'\n",
				b, ret, sx1301ar_err_message(sx1301ar_errno));
			err = 1;
		}
		// TODO: close all spi devices
	}
	if (!err)
	{
		RTL_TRDBG(1,"RADIO stopped\n");
		_LgwStarted	= 0;
	}
}

#ifdef WITH_GPS
void	LgwGpsTimeUpdated(struct timespec *utc_from_gps, struct timespec * ubxtime_from_gps)
{
	int x; /* return value for sx1301ar_* functions */
	uint32_t cnt_pps; /* internal timestamp counter captured on PPS edge */
	uint32_t hs_pps; /* high speed counter captured on PPS edge */
	int	board = 0;
	time_t	now;
	now = rtl_tmmsmono();

	for (board=0; board<LgwBoard; board++)
	{
		/* Get timestamp value captured on PPS edge (use mutex to protect hardware access) */
		x = sx1301ar_get_trigcnt( board, &cnt_pps );
		if( x != 0 )
		{
			RTL_TRDBG(0,"ERROR: board%d sx1301ar_get_trigcnt failed; %s\n", board, sx1301ar_err_message( sx1301ar_errno ) );
			return;
		}
		x = gwloc_get_hspps( board, &hs_pps );
		if( x != 0 )
		{
			RTL_TRDBG(0,"ERROR: board%d gwloc_get_hspps failed; %s\n", board, sx1301ar_err_message( sx1301ar_errno ) );
			return;
		}

		/* Attempt sync */
		x = sx1301ar_synchronize( board, &(Gps_time_ref[board]), *utc_from_gps, cnt_pps, hs_pps, NULL);
		if( x == 0 )
		{
			RTL_TRDBG(3, "Sync successful board%d (XTAL err: %.9lf)\n", board, Gps_time_ref[board].xtal_err ); /* too verbose */
		}
		else
		{
			RTL_TRDBG(3, "Sync board%d failed x=%d: %s\n", board, x, sx1301ar_err_message( sx1301ar_errno ) ); /* too verbose */
		}
	}
	LgwCurrUtcTime.tv_sec	= utc_from_gps->tv_sec;
	LgwCurrUtcTime.tv_nsec	= 0;
	LgwTmmsUtcTime = now;
}
#endif /* WITH_GPS */


int FindChipFromFreq(uint32_t freq, int rfchain, int board)
{
	int		chip, chan, r;
	uint32_t	f;
	char		section[64];

	for (chip=0; chip<SX1301AR_BOARD_CHIPS_NB; chip++)
	{
		for (chan=0; chan<SX1301AR_CHIP_MULTI_NB+1; chan++)
		{
			sprintf	(section,"ifconf:%d:%d:%d",board,chip,chan);
			f = CfgInt(HtVarLgw,section,-1,"freqhz",0);
			if (f == freq)
			{
				// check the rfchain, in diversity mode the same freq
				// could be configured on 2 different chips
				sprintf	(section,"chip:%d:%d",board,chip);
				r = CfgInt(HtVarLgw,section,-1,"rfchain",0);
				if (r == rfchain)
					return chip;
			}
		}
	}
	return -1;
}

static	int	ProceedRecvPacket(T_lgw_pkt_rx_t    *p,time_t tms, int board)
{
	t_imsg		*msg;
	t_lrr_pkt	uppkt;
	int		sz, i;
	u_char		*data;
	t_channel	*chan;
	struct		timespec	tv;

	u_int		numchan	= -1;
	char		*namchan= "?";
	u_int		numband	= -1;
	int		ret;
	uint32_t	curtus;
	int		delayUplinkRecv=0;
	int		chipid;

	struct timespec fetch_time;
	struct tm * fetch_tm;

	char gpstm[SX1301AR_BOARD_CHIPS_NB][50];

	clock_gettime(CLOCK_REALTIME,&tv);

	chan	= FindChannelForPacket(p);
	if	(chan)
	{
		numchan	= chan->channel;
		namchan	= (char *)chan->name;
		numband	= chan->subband;
	}

RTL_TRDBG(1,"PKT RECV board%d tms=%09u tus=%09u status=%s sz=%d freq=%d mod=0x%02x bdw=%s spf=%s ecc=%s channel=%d nam='%s' G%d modul=%s\n",
		board, tms,p->count_us,PktStatusTxt(p->status),p->size,
		p->freq_hz, p->modulation,BandWidthTxt(p->bandwidth),
		SpreadingFactorTxt(p->modrate),CorrectingCodeTxt(p->coderate),
		numchan,namchan,numband, p->modulation == MOD_LORA ? "lora" : "not lora");

	delayUplinkRecv = 0;
	for (i=0; i<SX1301AR_BOARD_RFCHAIN_NB; i++)
	{
		gpstm[i][0] = '\0';
#ifdef WITH_GPS
		if (UseGpsTime)
		{
			if (sx1301ar_cnt2utc(Gps_time_ref[board], p->count_us, &fetch_time) == 0)
			{
				fetch_tm = gmtime( &(fetch_time.tv_sec) );
				sprintf(gpstm[i], "gpstime=%02i:%02i:%02i.%03li ", fetch_tm->tm_hour, fetch_tm->tm_min, fetch_tm->tm_sec, (fetch_time.tv_nsec)/1000000 );
			}
		}
#endif /* WITH_GPS */

		chipid = 0;
		if (p->rsig[i].is_valid)
		{
			chipid = FindChipFromFreq(p->freq_hz, i, board);
			ret = sx1301ar_get_instcnt(board, chipid, &curtus);
			if (ret == 0 && p->status == STAT_CRC_OK)
			{
				// calculate delay between the time the packet was really received
				// and now, the time we are informed of this packet
				delayUplinkRecv = (ABS(curtus-p->count_us))/1000;
				if (delayUplinkRecv > 500)
					delayUplinkRecv = 500;
				RTL_TRDBG(3,"PKT RECV delayuplinkrecv=%d (curtus=%09u diff=%d)\n",
					delayUplinkRecv, curtus, curtus-p->count_us);
			}
		}
RTL_TRDBG(1,"    rf%d: valid=%d finetime=%d timestamp=%09u %schip=%d halchan=%d snr=%f rssi_chan=%f rssi_sig=%f rssi_sig_std=%d antennaid=%d delay=%d\n",
		i, p->rsig[i].is_valid, p->rsig[i].fine_received, p->rsig[i].fine_tmst, gpstm[i],
		chipid, p->rsig[i].chan, p->rsig[i].snr, p->rsig[i].rssi_chan,
		p->rsig[i].rssi_sig, p->rsig[i].rssi_sig_std, LgwAntennas[board][chipid], delayUplinkRecv);
	}

	DoPcap((char *)p->payload,p->size);

//	if	(p->status != STAT_NO_CRC && p->status != STAT_CRC_OK)
	if	(p->status == STAT_CRC_BAD)
	{
		LgwNbCrcError++;
		return	0;
	}

	if	(TraceLevel >= 1)
	{
	char	buff[512];
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

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_flag	= LP_RADIO_PKT_UP;
	if	(p->status == STAT_NO_CRC)
	{
		uppkt.lp_flag	= uppkt.lp_flag | LP_RADIO_PKT_NOCRC;
	}

	uppkt.lp_lrrid	= LrrID;
	uppkt.lp_tms	= tms - delayUplinkRecv;

	if	(!uppkt.lp_tms)	uppkt.lp_tms	= 1;

	uppkt.lp_gss	= (time_t)tv.tv_sec;
	uppkt.lp_gns	= (u_int)tv.tv_nsec;

	uppkt.lp_tus		= p->count_us;
	uppkt.lp_channel	= chan->channel;
	uppkt.lp_subband	= chan->subband;
	uppkt.lp_spfact		= CodeSpreadingFactor(p->modrate);
	uppkt.lp_correct	= CodeCorrectingCode(p->coderate);
	uppkt.lp_size		= p->size;

	int nbpkt = 0;
	for (i=0; i<SX1301AR_BOARD_RFCHAIN_NB; i++)
	{
		if (!p->rsig[i].is_valid)
			continue;

#ifdef WITH_GPS
		if (UseGpsTime && gpstm[i][0])	// check if we got a valid cnt2utc value
		{
			uppkt.lp_gss = fetch_time.tv_sec;
			if (p->rsig[i].fine_received)
			{
				uppkt.lp_flag	= uppkt.lp_flag | LP_RADIO_PKT_FINETIME;
				uppkt.lp_gns = p->rsig[i].fine_tmst;
			}
			else
			{
				uppkt.lp_flag	= uppkt.lp_flag & ~LP_RADIO_PKT_FINETIME;
				uppkt.lp_gns = fetch_time.tv_nsec;
			}
		}
#endif

		data	= (u_char *)malloc(p->size);
		if	(!data)
			return	-1;
		memcpy	(data,p->payload,p->size);

		uppkt.lp_payload	= data;

		//lp_chain =  antennaid(4b) + board(3b) + rfchain(1b)
		uppkt.lp_chain		= LgwAntennas[board][chipid] << 4 ;
		uppkt.lp_chain		|= (board&0x0f) << 1 ;
//		uppkt.lp_chain		|= (p->rsig[i].chan&0x10) >> 4;	// chip num is on 4 MSB of chan
		// set rf chain instead of chip number because that's what is
		// required when sending downlink response
		uppkt.lp_chain		|= i&0x01;	// set rf chain
		// WARNING: we consider 'i' is chip number in LgwAntennas[board][i] and after
		// that we consider 'i' is rfchain number in 'uppkt.lp_chain |= i&0x01'
		// it could be a problem !

		uppkt.lp_rssi		= p->rsig[i].rssi_chan;
		uppkt.lp_snr		= p->rsig[i].snr;


#ifdef LP_TP31
		uppkt.lp_tmoa	= TmoaLrrPacketUp(p);
		RTL_TRDBG(3,"TmoaLrrPacketUp()=%f\n", uppkt.lp_tmoa);
		DcTreatUplink(&uppkt);
#endif

		msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_RECV_DATA,NULL,0);
		if	(!msg)
			return	-2;

		sz	= sizeof(t_lrr_pkt);
		if	( rtl_imsgDupData(msg,&uppkt,sz) != msg)
		{
			rtl_imsgFree(msg);
			return	-3;
		}

		RTL_TRDBG(3,"PKT RECV add pkt received on chip %d\n", i);
		rtl_imsgAdd(MainQ,msg);
		nbpkt += 1;
	}
	return	nbpkt;
}

int	LgwDoRecvPacket(time_t tms)
{
	T_lgw_pkt_rx_t	rxpkt[LGW_RECV_PKT];
	T_lgw_pkt_rx_t	*p;
	uint8_t		nbpkt;
	int		i, b, ret;
	int		nb	= 0;
	static	int	lastboard = -1;
	time_t		exacttms;	// updated tms

//	for (b=0; b<LgwBoard; b++)
	b = (lastboard + 1) % LgwBoard;
	lastboard = b;
	{
		ret	= sx1301ar_fetch(b,rxpkt,LGW_RECV_PKT,&nbpkt);
		if	(ret != LGW_HAL_SUCCESS || nbpkt <= 0)
		{
			RTL_TRDBG(9,"PKT board%d nothing to Recv=%d ret=%d\n",b,nbpkt,ret);
			return	0;
		}

		// need to get tms here because sx1301ar_fetch takes at least 65 ms
		exacttms	= rtl_tmmsmono();
		LgwNbPacketRecv	+= nbpkt;
		for	(i = 0 ; i < nbpkt ; i++)
		{
			p	= &rxpkt[i];
			if	((ret=ProceedRecvPacket(p,exacttms,b)) < 0)
			{
				RTL_TRDBG(1,"PKT RECV board%d not treated ret=%d\n",b,ret);
			}
			else
			{
				nb+=ret;
			}
		}
	}

	return	nb;
}

/*!
* \fn int8_t GetTxCalibratedEIRP(int8_t tx_requested, float antenna_gain, float cable_loss, uint8_t board, uint8_t rfc)
* \brief Get the LUT-calibrated EIRP value
* \param tx_requested: Requested EIRP value set by the user (dBm)
* \param antenna_gain: Gain of the antenna (dBm)
* \param cable_loss: Loss of power due to the cable (dBm)
* \param board: Board index used for searching in LUT table
* \param rfc: RF chain used for searching in LUT table
* \return The theorical LUT-calibrated EIRP value
*
* EIRP stands for Equivalent Isotropically Radiated Power. This is the amount of power that an antenna would emit.
* EIRP takes into account the losses in transmission line and includes the gain of the antenna.
* This function takes the user-requested Tx EIRP and returns the nearest inferior workable value, based on the LUT calibration table.
*
*/
int8_t GetTxCalibratedEIRP(int8_t tx_requested, float antenna_gain, float cable_loss, uint8_t board, uint8_t rfc)
{
	int8_t	tx_found;
	float	tx_tmp;
	int	i, nb_lut_max;
	sx1301ar_tx_gain_lut_t	txlut;

	tx_tmp = tx_requested - antenna_gain + cable_loss;
	LgwConfigureLut(NULL, &txlut, board, rfc, 0);
	nb_lut_max = SX1301AR_BOARD_MAX_LUT_NB;
	tx_found = txlut.lut[0].rf_power; /* If requested value is less than the lowest LUT power value */
	for (i = (nb_lut_max-1); i >= 0; i--)
	{
		if ( tx_tmp >= (float)txlut.lut[i].rf_power)
		{
			tx_found = txlut.lut[i].rf_power;
			break;
		}
	}

	return ((int)roundf((float)tx_found + antenna_gain - cable_loss));
}

static	u_int	WaitSendPacket(int blo,time_t tms,sx1301ar_tstat_t *txstatus, int board)
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
		IsBoardStatusTxFree(board, txstatus);
#if	0
		u_int	t;
		t	= LgwTimeStamp();
RTL_TRDBG(3,"PKT sending j=%d status=%d t=%u\n",j,*txstatus,t);
#endif
		usleep(1000);	// * 3000 => 3s max
	} while ((*txstatus != TX_FREE) && (j < 3000));
	diff	= rtl_tmmsmono();
	diff	= ABS(diff - tms);

	RTL_TRDBG(3,"PKT sent j=%d status=%d\n",j,*txstatus);
	if	(*txstatus != TX_FREE)
	{
		RTL_TRDBG(0,"PKT SEND status(=%d) != TX_FREE\n",*txstatus);
	}
	return	diff;
}

static	int	SendPacketNow(int blo,t_lrr_pkt *downpkt,T_lgw_pkt_tx_t txpkt, int board)
{
	sx1301ar_tstat_t txstatus = 0;
	int	left;
	int	ret;
	int	diff	= 0;
	int	duration = 0;
	u_int	tms;

	left	= LgwNbPacketWait;

	IsBoardStatusTxFree(board, &txstatus);
	if	(downpkt->lp_lgwdelay == 0 && txstatus != TX_FREE)
	{
		RTL_TRDBG(1,"PKT board%d busy status=%d left=%d\n",board,txstatus,left-1);
		LgwNbBusySend++;
		return	-1;
	}
	if	(0 && downpkt->lp_lgwdelay && txstatus != TX_FREE) // TODO
	{
		RTL_TRDBG(1,"PKT board%d busy status=%d left=%d\n",board,txstatus,left-1);
		LgwNbBusySend++;
		return	-1;
	}

	ChangeChannelFreq(downpkt,&txpkt); //	NFR703

	if	(txpkt.rf_power > LgwPowerMax)
		txpkt.rf_power	= LgwPowerMax;

	tms	= rtl_tmmsmono();
	diff	= (time_t)tms - (time_t)downpkt->lp_tms;
	if	((ret=sx1301ar_send(board,&txpkt)) != LGW_HAL_SUCCESS)
	{
		RTL_TRDBG(0,"PKT board%d send error=%d\n",board,ret);
		return	-2;
	}
	if (downpkt->lp_beacon) {
		LgwBeaconSentCnt++;
		LgwBeaconLastDeliveryCause = 0;
	}
	LgwNbPacketSend++;
//	sx1301ar_tx_status(board,0,&txstatus);
	IsBoardStatusTxFree(board, &txstatus);
	if	(downpkt->lp_lgwdelay == 0 && txstatus != TX_EMITTING)
	{
		RTL_TRDBG(0,"PKT SEND board%d  status(=%d) != TX_EMITTING\n",board,txstatus);
	}
	if	(0 && downpkt->lp_lgwdelay && txstatus != TX_SCHEDULED)	// TODO
	{
		RTL_TRDBG(0,"PKT SEND board%d status(=%d) != TX_SCHEDULED\n",board,txstatus);
	}

#ifdef LP_TP31
	// must be done only if packet was really sent
	DcTreatDownlink(downpkt);
#endif

RTL_TRDBG(1,"PKT SEND board%d tms=%09u/%d status=%d sz=%d left=%d freq=%d mod=0x%02x bdw=%s spf=%s ecc=%s pr=%d nocrc=%d ivp=%d pw=%.2f rfchain=%d\n",
	board,tms,diff,txstatus,txpkt.size,left,txpkt.freq_hz,
	txpkt.modulation,BandWidthTxt(txpkt.bandwidth),
	SpreadingFactorTxt(txpkt.modrate),CorrectingCodeTxt(txpkt.coderate),
	txpkt.preamble,txpkt.no_crc,txpkt.invert_pol,txpkt.rf_power,txpkt.rf_chain);

	if	(downpkt->lp_lgwdelay == 0)
	{
		duration	= WaitSendPacket(blo,tms,&txstatus, board);
	}

	// the packet is now passed to the sx13, compute time added by all
	// treatements LRC/LRR, only if lgwdelay or non blocking mode

	diff	= 0;
	if	(downpkt->lp_lgwdelay || (!blo && !LgwWaitSending))
	{
		u_int	scheduled;	// time before sx13 schedule request

		diff	= ABS(rtl_tmmsmono() - downpkt->lp_tms);

		scheduled		= downpkt->lp_lgwdelay;
		LastTmoaRequested[board]	= ceil(downpkt->lp_tmoa/1000);
		LastTmoaRequested[board]	= LastTmoaRequested[board] +
					(LastTmoaRequested[board] * 10)/100;
		RTL_TRDBG(1,"LGW DELAY tmao request=%dms + sched=%dms\n",
				LastTmoaRequested[board],scheduled);
		LastTmoaRequested[board]	= LastTmoaRequested[board] + scheduled + 70;
		CurrTmoaRequested[board]	= LastTmoaRequested[board];
	}
	else
	{
	// TODO meme si no delay il faut compter la requete
		LastTmoaRequested[board]	= 0;
		CurrTmoaRequested[board]	= 0;
	}

	if	(TraceLevel >= 1)
	{
		char	buff[1024];
		char	src[64] = "";
		char	pktclass[16] = "";
		LoRaMAC_t	mf;
		u_char		*pt;

		memset(&mf,0,sizeof(mf));

		if (downpkt->lp_beacon == 0) {
			// we assumed this is a loramac packet
			LoRaMAC_decodeHeader(txpkt.payload,txpkt.size,&mf);	
			pt	= (u_char *)&mf.DevAddr;
			sprintf	(src,"%02x%02x%02x%02x",*(pt+3),*(pt+2),*(pt+1),*pt);
			if (downpkt->lp_classb) {
				diff = 0;
				strcpy(pktclass, "classb");
				RTL_TRDBG(1,"PKT SEND classb period=%d sidx=%d sdur=%f window(%09u,%09u) %d/%d/%d\n",
					downpkt->lp_period,downpkt->lp_sidx,downpkt->lp_sdur,
				((downpkt->lp_period-1)*128)+downpkt->lp_gss0,
				downpkt->lp_gns0,
				downpkt->lp_idxtry,downpkt->lp_nbtry,downpkt->lp_maxtry);
			} else {
				strcpy(pktclass, "class A");
			}
		}
		else
		{
			strcpy(pktclass, "beacon");
			strcpy(src, "<broadcast>");
		}

		rtl_binToStr(txpkt.payload,txpkt.size,buff,sizeof(buff)-10);
		if	(downpkt->lp_lgwdelay == 0)
		{
			if	(blo || LgwWaitSending)
			{
			RTL_TRDBG(1,"PKT SEND blocking board%d %s dur=%ums data='%s' seq=%d devaddr=%s\n",
						board,pktclass,duration,buff,mf.FCnt,src);
			}
			else
			{
	RTL_TRDBG(1,"PKT SEND noblock board%d %s dur=%fms diff=%ums data='%s' seq=%d devaddr=%s\n",
	board,pktclass,downpkt->lp_tmoa/1000,diff,buff,mf.FCnt,src);
			}
		}
		else
		{
	RTL_TRDBG(1,"PKT SEND async board%d %s dur=%fms diff=%ums data='%s' seq=%d devaddr=%s\n",
	board,pktclass, downpkt->lp_tmoa/1000,diff,buff,mf.FCnt,src);
		}
	}

	DoPcap((char *)txpkt.payload,txpkt.size);

	return	0;
}

static	void	SetTrigTarget(t_lrr_pkt *downpkt,T_lgw_pkt_tx_t *txpkt)
{
	uint32_t	diffus = 0;
	static int      classbdelay = -1;

	if (classbdelay == -1) {
		classbdelay = CfgInt(HtVarLrr, "classb", -1, "adjustdelay", 0);
		RTL_TRDBG(1, "classb.adjustdelay=%d\n", classbdelay);
	}
	
	if	(downpkt->lp_lgwdelay == 0)
	{
		txpkt->tx_mode 	= TX_IMMEDIATE;
		RTL_TRDBG(1,"LRR DELAY -> tx mode immediate\n");
		return;
	}

	if (downpkt->lp_beacon) {
		txpkt->tx_mode  = TX_ON_GPS;
		txpkt->utc_time.tv_sec	= downpkt->lp_gss;
		txpkt->utc_time.tv_usec	= downpkt->lp_gns/1000;
		txpkt->use_utc	= 1;
		downpkt->lp_lgwdelay = 0;	// tektelic can handle several downlink, do not wait for sending
		RTL_TRDBG(1, "PKT SEND beacon utc=(%u,%09u)\n",
			downpkt->lp_gss, downpkt->lp_gns);
		return;
	}

	if (downpkt->lp_classb) {
		txpkt->tx_mode  = TX_ON_GPS;
		txpkt->utc_time.tv_sec	= downpkt->lp_gss;
		txpkt->utc_time.tv_usec	= downpkt->lp_gns/1000;
		txpkt->use_utc	= 1;
		downpkt->lp_lgwdelay = 0;	// tektelic can handle several downlink, do not wait for sending
		RTL_TRDBG(1,"PKT SEND classb utc=(%u,%09u)\n",
			downpkt->lp_gss, downpkt->lp_gns);
		return;
	}

	txpkt->tx_mode 	= TX_TIMESTAMPED;

	txpkt->count_us	= downpkt->lp_tus;
	txpkt->count_us	= txpkt->count_us + (downpkt->lp_delay * 1000);
	txpkt->count_us	= txpkt->count_us - Sx13xxStartDelay;

	diffus	= ABS(txpkt->count_us - downpkt->lp_tus);
RTL_TRDBG(3,"LGW DELAY trigtarget=%u trigorig=%u delayus=%u\n",
	txpkt->count_us,downpkt->lp_tus,diffus);

	return;
}

static	int	SendPacket(t_imsg  *msg)
{
	t_lrr_pkt	*downpkt;
	T_lgw_pkt_tx_t 	txpkt;
	t_channel	*chan;
	int	ret;
	int	blo	= 0;
	int	board	= 0;

	downpkt	= msg->im_dataptr;

	if	(downpkt->lp_channel >= MaxChannel ||
		TbChannel[downpkt->lp_channel].name[0] == '\0')
	{
		LgwNbChanDownError++;	
		RTL_TRDBG(1,"SendPacket: ChanDownError += 1 chan=%d maxchan=%d name=%s\n",
			downpkt->lp_channel, MaxChannel, (downpkt->lp_channel >= MaxChannel?"":TbChannel[downpkt->lp_channel].name[0]));
		return	0;
	}
	chan	= &TbChannel[downpkt->lp_channel];

	memset	(&txpkt,0,sizeof(txpkt));

//	txpkt.freq_hz = 868100000;
//	txpkt.modulation = MOD_LORA;
//	txpkt.bandwidth = BW_125KHZ;
//	txpkt.modrate = DR_LORA_SF10;
//	txpkt.coderate = CR_LORA_4_5;

	SetTrigTarget(downpkt,&txpkt);
	board			= (downpkt->lp_chain&0x0F) >> 1;
//	antennaid		= downpkt->lp_chain >> 4;
	txpkt.rf_chain 		= downpkt->lp_chain&0x01;
	txpkt.freq_hz 		= chan->freq_hz;
	txpkt.modulation 	= chan->modulation;
	txpkt.bandwidth 	= chan->bandwidth;
	txpkt.modrate 		= DecodeSpreadingFactor(downpkt->lp_spfact);
	txpkt.coderate		= DecodeCorrectingCode(downpkt->lp_correct);
	txpkt.invert_pol	= LgwInvertPol;
	txpkt.no_crc 		= LgwNoCrc;
	txpkt.no_header 	= LgwNoHeader;
	txpkt.preamble 		= LgwPreamble;
	txpkt.rf_power 		=
		chan->power - AntennaGain[0] + CableLoss[0];	// TODO index

	if (downpkt->lp_beacon) {
		txpkt.invert_pol	= LgwInvertPolBeacon;
		txpkt.no_crc		= 1;
		txpkt.no_header		= 1;
		txpkt.preamble 		= 10;
/*	Done in SetTrigTarget
		txpkt.use_utc 		= 1;
		txpkt.utc_time.tv_sec	= downpkt->lp_gss;
		txpkt.utc_time.tv_usec	= downpkt->lp_gns/1000;
*/
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
		ret	= SendPacketNow(blo=1,downpkt,txpkt,board);
		if	(ret < 0)
		{
			RTL_TRDBG(0,"SendPacketNow() error => %d\n",ret);
		}
		usleep(LgwAckData802Wait*1000);	// 10 ms
	}

	txpkt.size = downpkt->lp_size;
	memcpy	(txpkt.payload,downpkt->lp_payload,downpkt->lp_size);
	free	(downpkt->lp_payload);
	downpkt->lp_payload	= NULL;


	ret	= SendPacketNow(blo=0,downpkt,txpkt,board);
	if	(ret < 0)
	{
		RTL_TRDBG(0,"SendPacketNow() error => %d\n",ret);
		return	0;
	}

	return	1;
}

// called by radio thread lgw_gen.c:LgwMainLoop()
int	LgwDoSendPacket(time_t now)
{
	t_imsg	*msg;
	int	nbs	= 0;
	int	left;
	int	ret;
	time_t	ref_age;
	int	board;
	static int	classbuseall = -1;

	if (classbuseall == -1) {
		classbuseall = CfgInt(HtVarLrr, "classb", -1, "useallslot", 0);
		RTL_TRDBG(1, "classb useallslot=%d\n", classbuseall);
	}

#ifdef WITH_GPS
	ref_age = time( NULL ) - Gps_time_ref[0].systime;
	Gps_ref_valid = (ref_age >= 0) && (ref_age < 60);
#endif /* WITH_GPS */

	left = 0;

	for (board=0; board<LgwBoard; board++)
		left	+= rtl_imsgCount(LgwSendQ[board]);

	if	(left > 0 && left > LgwNbPacketWait)
		LgwNbPacketWait	= left;

	for (board=0; board<LgwBoard; board++)
	{
		if	((msg = rtl_imsgGet(LgwSendQ[board], IMSG_BOTH)) != NULL)
		{
			t_lrr_pkt	*downpkt;

			downpkt	= msg->im_dataptr;

			if (downpkt->lp_beacon) {
				RTL_TRDBG(1,"PKT SEND beacon retrieved board %d\n", board);
				goto send_pkt;
			}

			if (downpkt->lp_classb) {
				RTL_TRDBG(1, "PKT SEND classb retrieved\n");
				goto send_pkt;
			}

			if	(downpkt->lp_delay == 0 && CurrTmoaRequested[board])
			{	// mode immediate
				if	(ABS(now - downpkt->lp_tms) > MaxReportDnImmediat)
				{
			RTL_TRDBG(1,"PKT SEND NODELAY board%d not sent after 3s => dropped\n", board);
					SetIndic(downpkt,0,LP_C1_MAXTRY,LP_C2_MAXTRY,-1);
					SendIndicToLrc(downpkt);
					rtl_imsgFree(msg);
					return	0;
				}
			RTL_TRDBG(1,"PKT SEND NODELAY board%d avoid collision repostpone=%d\n",
							board, CurrTmoaRequested[board]);
				rtl_imsgAddDelayed(LgwSendQ[board],msg,CurrTmoaRequested[board]+3);
				return	0;
			}

send_pkt:
			if (classbuseall && downpkt->lp_classb && downpkt->lp_nbslot <= 16) {
				t_lrr_pkt    pkt;
				t_imsg     * repeat;

				memcpy(&pkt, downpkt, sizeof(t_lrr_pkt));
				pkt.lp_payload = (u_char *)malloc(pkt.lp_size);
				memcpy(pkt.lp_payload, downpkt->lp_payload, pkt.lp_size);
				pkt.lp_delay = 0;
				repeat = rtl_imsgAlloc(IM_DEF, IM_LGW_SEND_DATA, NULL, 0);
				rtl_imsgDupData(repeat, &pkt, sizeof(t_lrr_pkt));
				rtl_imsgAddDelayed(LgwSendQ[board], repeat, 1000);
			}

			ret	= SendPacket(msg);
			if	(ret > 0)
			{
				SetIndic(downpkt,1,-1,-1,-1);
				nbs	+= ret;
			}
			else
			{
				RTL_TRDBG(1,"SendPacket() error board %d => %d\n", board, ret);
				SetIndic(downpkt,0,-1,-1,-1);
			}
			SendIndicToLrc(downpkt);
			rtl_imsgFree(msg);
		}
	}
	return	nbs;
}

