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
#include <sys/vfs.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#ifdef	WITH_GPS
#include <termios.h>
#endif

#include "rtlbase.h"
#include "rtlimsg.h"
#include "rtllist.h"
#include "rtlhtbl.h"

#include "semtech.h"

#include "xlap.h"
#include "infrastruct.h"
#include "struct.h"

#include "headerloramac.h"

#define	MIN(a, b)	((a) < (b) ? (a) : (b))

typedef unsigned char u8;
typedef unsigned short u16;
u16 crc_ccitt(u16 crc, const u8 *buffer, int len);
int	NtpdStarted();

#include "_whatstr.h"
#include "define.h"
#include "cproto.h"
#include "extern.h"

static	int	SendToLrc(t_lrr_pkt *uppkt,u_char *buff,int sz);
static	void	SendDtcSyncToLrc(t_xlap_link *lktarget);
static	int	SendStatToAllLrc(u_char *buff,int sz,int delay);

char	*ExtLrrRestart		= "/tmp/lrrrestart";
char	*ExtRadioRestart	= "/tmp/lrrradiorestart";

u_int	VersionMaj;
u_int	VersionMin;
u_int	VersionRel;
u_int	VersionFix;

u_int	VersionMajRff;
u_int	VersionMinRff;
u_int	VersionRelRff;
u_int	VersionFixRff;
time_t	RffTime;

int			PingRttPeriod	= 60;		// sec
int			AutoRebootTimer	= 14400;	// 4*3600
int			AutoRebootTimerNoUplink	= 3600;	// 3600 (1 hour)
int			AutoRestartLrrMaxNoUplink = 3; 
int			AutoRevSshTimer	= 1200;		// 20 mn
int			IfaceDaemon	= 0;
int			LapTest		= 0;
int			LapTestDuration	= 43200;	// 12*3600

int			Sx13xxPolling	= 10;	// ms
#ifdef	USELIBLGW3
int			Sx13xxStartDelay= 0;	// ms
#else
int			Sx13xxStartDelay= SX13XX_START_DELAY;	// ms
#endif
#if defined(FCMLB) || defined(FCPICO) || defined(GEMTEK) || defined(FCLAMP)
char			*SpiDevice	= "/dev/spidev1.0";
#elif defined(REF_DESIGN_V2)
char			*SpiDevice[SX1301AR_MAX_BOARD_NB];
#endif
#ifdef	WITH_TTY
char			*TtyDevice	= "/dev/ttyACM0";
#endif
int			CfgRadioState	= 0;	// config flag save CfgRadio
int			CfgRadioStop	= 0;	// config flag
int			CfgRadioDnStop	= 0;	// config flag

int			DownRadioStop	= 0;	// state of downlink radio
int			MainWantStop	= 0;
int			MainStopDelay	= 5;

float			AntennaGain[NB_ANTENNA];
float			CableLoss[NB_ANTENNA];

char 			* CustomVersion_Hw;
char 			* CustomVersion_Os;
char 			* CustomVersion_Hal;
char 			* CustomVersion_Build;
char 			* CustomVersion_Config;
char 			* CustomVersion_Custom1;
char 			* CustomVersion_Custom2;
char 			* CustomVersion_Custom3;

int			NbItf;
t_wan_itf		TbItf[NB_ITF_PER_LRR];

int			NbMfs;
t_mfs			TbMfs[NB_MFS_PER_LRR];

u_int			MemTotal;
u_int			MemFree;
u_int			MemBuffers;
u_int			MemCached;
u_int			MemUsed;

t_xlap_link		TbLapLrc[NB_LRC_PER_LRR];
struct	list_head	*LapList;
u_int			LapFlags	= LK_TCP_CLIENT|LK_SSP_SLAVE|
					LK_TCP_RECONN|LK_LNK_SAVEDNSENT;

t_lrc_link		TbLrc[NB_LRC_PER_LRR];
int			MasterLrc = -1;


t_lrr_config	ConfigIpInt;

t_avdv_ui	CmnLrcAvdvTrip;
t_avdv_ui	CpuAvdvUsed;
u_short		CpuLoad1;
u_short		CpuLoad5;
u_short		CpuLoad15;

int		PowerEnable	= 0;
char		*PowerDevice	= "";
int		PowerDownLev	= 0;
int		PowerUpLev	= 0;
int		PowerState	= '?';		// 'U' || 'D' : up || down
u_int		PowerDownCnt	= 0;
u_int		PowerDownCntP	= 0;

int		TempEnable	= 0;
int		TempPowerAdjust	= 0;
int		TempExtAdjust	= 0;
char		*TempDevice	= "";
int		CurrTemp	= -273;


int		CurrLrcOrder	= -1;		// current lrc/order index 

unsigned int	LrcNbPktTrip;
unsigned int	LrcAvPktTrip;
unsigned int	LrcDvPktTrip;
unsigned int	LrcMxPktTrip;
unsigned int	LrcNbDisc;

struct	timespec	Currtime;	// updated each sec in DoClockSc

struct timespec 	UptimeSyst;	// uptime system from /proc/uptime
char			UptimeSystStr[128];
struct timespec 	UptimeProc;	// uptime process
char			UptimeProcStr[128];

u_char	SickRestart	= 0;
int	GpsPositionOk	= 0;
float	GpsLatt		= 0;
float	GpsLong		= 0;
int	GpsAlti		= 0;
u_int	GpsUpdateCnt	= 0;
u_int	GpsUpdateCntP	= 0;
u_int	GpsDownCnt	= 0;
u_int	GpsDownCntP	= 0;
u_int	GpsUpCnt	= 0;
u_int	GpsUpCntP	= 0;
u_char	GpsStatus	= '?';		/* 'U' || 'D' : up || down */

int	StatRefresh	= 3600;
int	RfCellRefresh	= 300;
int	ConfigRefresh	= 3600*24;
int	WanRefresh	= 300;
int	GpsStRefresh	= 30;
float	GpsStRateLow	= 0.85;
float	GpsStRateHigh	= 0.90;
char	*RootAct	= NULL;
char	*System		= NULL;
int	ServiceStopped	= 0;
int	LapState	= 0;

char    *TraceFile      = NULL;
char    *TraceStdout    = NULL;
int     TraceSize       = 1*1000*1000;     // for each file 1M
int     TraceLevel      = 0;
int     TraceLevelP	= 0;		// level persistent
int     TraceDebug      = 0;

char	ConfigDefault[PATH_MAX];
char	ConfigCustom[PATH_MAX];
char	DirCommands[PATH_MAX];

char	*IsmBand		= "eu868";
char	*IsmBandAlter		= "";
int	IsmFreq			= 868;
int	IsmAsymDownlink		= 0;
int	IsmAsymDnModulo		= 8;
int	IsmAsymDnOffset		= 0;
char	*RfRegionId		= NULL;		// not "" can reallocated
u_int	RfRegionIdVers		= 0;

char	*ConfigFileParams	= "_parameters.sh";
char	*ConfigFileSystem	= "_system.sh";
char	*ConfigFileCustom	= "custom.ini";
char	*ConfigFileDynCalib	= "dyncalib.ini";
char	*ConfigFileDynLap	= "dynlap.ini";
char	*ConfigFileGps		= "gpsman.ini";
char	*ConfigFileDefine	= "defines.ini";
char	*ConfigFileLrr		= "lrr.ini";
char	*ConfigFileChannel	= "channels.ini";
char	*ConfigFileState	= "_state.ini";
#ifdef	WIRMAMS
char	*ConfigFileLgw		= "lgwms.ini";
char	*ConfigFileLgwCustom	= "lgw.ini";
#endif
#if	defined(WITH_SX1301_X1) && !defined(WIRMAMS)
char	*ConfigFileLgw		= "lgw.ini";
#endif
#ifdef	WITH_SX1301_X8
char	*ConfigFileLgw		= "lgwx8.ini";
char	*ConfigFileLgwCustom	= "lgw.ini";
#endif

char	*ConfigFileLowLvLgw	= "lowlvlgw.ini";

char	*ConfigFileBootSrv	= "bootserver.ini";


pthread_t	MainThreadId;

void	*HtVarLrr;	// hash table for variables LRR lrr.ini
void	*HtVarLgw;	// hash table for variables LGW lgw.ini
void	*HtVarSys;	// hash table for variables "system"
void	*MainQ;		// command/timer/data messages queue for main thread
void	*LgwQ;		// command/timer messages queue for lora gateway thread
#ifdef REF_DESIGN_V2
void	*LgwSendQ[SX1301AR_MAX_BOARD_NB];	// data messages queue for lora gateway thread
#else
void	*LgwSendQ;	// data messages queue for lora gateway thread
#endif
void	*StoreQ;	// uplink radio packet storage

void	*MainTbPoll;

#ifdef	LP_TP31
int	StorePktCount	= 30000;
#else
int	StorePktCount	= 0;
#endif
float	StoreMemUsed	= 50;	// 50%
int	ReStorePerSec	= 3;	// possible values 10,5,3,2,1 (DoClockMs())
int	ReStoreCtrlOutQ	= 1;
int	ReStoreCtrlAckQ	= 1;
//	FIX3323
int	MaxStorePktCount;	// max reach on wan refresh period
time_t	MaxStorePktTime;	// time of max

pthread_attr_t	LgwThreadAt;
pthread_t	LgwThread;
int		LgwThreadStarted;
int		LgwThreadStopped;
int		LgwUsbIndex	= 0;
pthread_attr_t	GpsThreadAt;
pthread_t	GpsThread;
int		GpsThreadStarted;
int		GpsThreadStopped;
#ifdef	WITH_USB	// check connexion with the board does not work
int		LgwLinkUp	= 0;
#else
int		LgwLinkUp	= 0;
#endif

int		MacWithBeacon	= 0;
int		MacWithFcsDown	= 0;
int		MacWithFcsUp	= 0;
unsigned int	MacNbFcsError;


unsigned int	LrcNbPktDrop;

int	OnConfigErrorExit	= 0;
int	OnStartErrorExit	= 0;


int			AdjustDelay	= 0;
u_int			NbLrc		= 0;
u_int			LrrID		= 0;
u_short			LrrIDExt	= 0;
u_char			LrrIDPref	= 0;
u_char			LrrIDGetFromTwa	= 0;	// NFR684: get lrrid from TWA
u_int			LrrIDFromTwa	= 0;	// NFR684: lrrid got from TWA
u_int			LrrIDGetFromBS	= 0;	// NFR997
u_int			LrrIDFromBS	= 0;
float			LrrLat		= 90.0;
float			LrrLon		= 90.0;
int			LrrAlt		= 0;
u_int			UseGps		= 0;
u_int			UseGpsPosition	= 0;
u_int			UseGpsTime	= 0;
u_int			UseLgwTime	= 0;
u_int			Redundancy	= 0;
u_int			SimulN		= 0;
u_int			QosAlea		= 0;
u_int			TbLrrID[NB_LRC_PER_LRR];
float			TbLrrLat[NB_LRC_PER_LRR];
float			TbLrrLon[NB_LRC_PER_LRR];
int			TbLrrAlt[NB_LRC_PER_LRR];

u_int			TbDevAddrIncl[10];
int			NbDevAddrIncl;
u_int			TbDevAddrExcl[10];
int			NbDevAddrExcl;

int			MaxReportDnImmediat	= 60000;

int			LogUseRamDir	= 0;

static	int	RecvRadioPacket(t_lrr_pkt *uppkt,int late);
static	void	StartLgwThread();
static	void	CancelLgwThread();
#ifdef WITH_GPS
static	void	StartGpsThread();
static	void	CancelGpsThread();
#endif /* WITH_GPS */
static  void    DoHtmlFile();

static	int	DevAddrIncl(u_int   devaddr)
{
	int	i;

	if	(NbDevAddrIncl <= 0)
		return	1;

	for	(i = 0 ; i < NbDevAddrIncl ; i++)
	{
		if	(devaddr == TbDevAddrIncl[i])
			return	1;
	}
	return	0;
}

static	int	DevAddrExcl(u_int   devaddr)
{
	int	i;

	if	(NbDevAddrExcl <= 0)
		return	0;

	for	(i = 0 ; i < NbDevAddrExcl ; i++)
	{
		if	(devaddr == TbDevAddrExcl[i])
			return	1;
	}
	return	0;
}

int	OkDevAddr(char *dev)
{
	u_int	devaddr;

	devaddr	= (u_int)strtoul(dev,0,16);
	if	(DevAddrExcl(devaddr) == 1)	// excluded
	{
	RTL_TRDBG(1,"devaddr %08x excluded in configuration\n",devaddr);
	return	0;
	}
	if	(DevAddrIncl(devaddr) == 0)	// included
	{
	RTL_TRDBG(1,"devaddr %08x not included in configuration\n",devaddr);
	return	0;
	}
	return	1;
}

static void SendLrrUID(t_xlap_link *lk)
{
	t_lrr_pkt	uppkt;
	char		*pt = NULL;
	int		len;

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	// get LRRGID environment variable
	pt = CfgStr(HtVarSys,"",-1,"LRRGID","");
	if (pt && *pt)
	{
		len = MIN(sizeof(uppkt.lp_gwuid)-1, strlen(pt));
		strncpy((char *)uppkt.lp_gwuid, pt, len);
		uppkt.lp_gwuid[len] = '\0';
	}
	// get LRROUI environment variable
	pt = CfgStr(HtVarSys,"",-1,"LRROUI","");
	if (pt && *pt)
	{
		len = MIN(sizeof(uppkt.lp_oui)-1, strlen(pt));
		strncpy((char *)uppkt.lp_oui, pt, len);
		uppkt.lp_oui[len] = '\0';
	}

	uppkt.lp_type	= LP_TYPE_LRR_INF_LRRUID;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_lrruid);
	LapPutOutQueue(lk,(u_char *)&uppkt,uppkt.lp_szh);
	RTL_TRDBG(1,"LRR send lrruid='%s-%s' to lrc='%s' sz=%d\n", 
		uppkt.lp_oui,uppkt.lp_gwuid, lk->lk_rhost,uppkt.lp_szh);
	// set a timer in case lrc do not handle this message
	rtl_imsgAdd(MainQ,
	rtl_timerAlloc(IM_DEF,IM_TIMER_LRRUID_RESP,IM_TIMER_LRRUID_RESP_V,NULL,0));
}

void	DoLrrUIDConfigRefresh(int delay)
{
	t_lrr_pkt	uppkt;
	char		*pt = NULL;
	int		len;

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_lrrid	= LrrID;
	// get LRRGID environment variable
	pt = CfgStr(HtVarSys,"",-1,"LRRGID","");
	if (pt && *pt)
	{
		len = MIN(sizeof(uppkt.lp_gwuid)-1, strlen(pt));
		strncpy((char *)uppkt.lp_gwuid, pt, len);
		uppkt.lp_gwuid[len] = '\0';
	}
	// get LRROUI environment variable
	pt = CfgStr(HtVarSys,"",-1,"LRROUI","");
	if (pt && *pt)
	{
		len = MIN(sizeof(uppkt.lp_oui)-1, strlen(pt));
		strncpy((char *)uppkt.lp_oui, pt, len);
		uppkt.lp_oui[len] = '\0';
	}

	uppkt.lp_type	= LP_TYPE_LRR_INF_CONFIG_LRRUID;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_config_lrruid);
	RTL_TRDBG(1,"LRR send config lrruid='%s-%s' sz=%d\n", 
		uppkt.lp_oui,uppkt.lp_gwuid, uppkt.lp_szh);

	SendStatToAllLrc((u_char *)&uppkt,uppkt.lp_szh,delay);
}

#ifdef TEKTELIC
// prototype required to avoid a warning
char *strcasestr(const char *haystack, const char *needle);

// File "/tmp/position" contains:
//   Latitude: 4548.39528N
//   Longtitude: 00445.78207E	<= yes it is 'Longtitude' !
//   Altitude: 295.4
void GetGpsPositionTektelic()
{
	char	buf[80], *pt;
	char	*fn = "/tmp/position";
	FILE	*f;
	short	fp;
	double	sp;

	f = fopen(fn, "r");
	if (f)
	{
		GpsPositionOk = 0;
		while (fgets(buf, sizeof(buf)-1, f))
		{
			if ((pt = strcasestr(buf, "latitude:")))
			{
				pt += 9;
				sscanf(pt, "%2hd%lf", &fp, &sp);
				GpsLatt = (double)fp + (sp/60);
				if (strchr(pt, 'S'))
					GpsLatt *= -1;
			}
			if ((pt = strcasestr(buf, "longitude:")))
			{
				pt += 11;
				sscanf(pt, "%3hd%lf", &fp, &sp);
				GpsLong = (double)fp + (sp/60);
				if (strchr(pt, 'W'))
					GpsLong *= -1;
			}
			if ((pt = strcasestr(buf, "altitude:")))
			{
				pt += 9;
				GpsAlti = atoi(pt);
			}
			if ((pt = strcasestr(buf, "lock:")))
			{
				pt += 5;
				if (strcasestr(pt, "yes"))
					GpsPositionOk = 1;
			}
		}
		fclose(f);
		GpsUpdateCnt += 1;
		RTL_TRDBG(3,"GetGpsPositionTektelic: lat=%.2f long=%.2f, alt=%d, ok=%d\n",
			GpsLatt, GpsLong, GpsAlti, GpsPositionOk);
	}
	else
	{
		if (GpsPositionOk)
			GpsUpdateCnt += 1;
		RTL_TRDBG(3,"GetGpsPositionTektelic: can not open '%s' file !\n", fn);
		GpsPositionOk = 0;
	}
}
#endif

static void SendLrrID(t_xlap_link *lk)
{
	int		locallrrid	= LrrID;
	float		locallatt	= LrrLat;
	float		locallong	= LrrLon;
	int		localalti	= LrrAlt;
	t_lrr_pkt	uppkt;

	if	(SimulN)
	{
		int	i;
		int	idx = -1;

		for	(i = 0 ; idx == -1 && i < NbLrc ; i++)
		{
			if	(&TbLapLrc[i] == lk)
			{
				idx	= i;
			}
		}
		if	(idx == -1)
			return;
		locallrrid	= TbLrrID[idx];
		locallatt	= TbLrrLat[idx];
		locallong	= TbLrrLon[idx];
		localalti	= TbLrrAlt[idx];
	}

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_lrrid		= locallrrid;
	uppkt.lp_lrridext	= LrrIDExt;
	uppkt.lp_lrridpref	= LrrIDPref;
	uppkt.lp_type	= LP_TYPE_LRR_INF_LRRID;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE;
	uppkt.lp_gss	= (time_t)UptimeProc.tv_sec;
	uppkt.lp_gns	= (u_int)UptimeProc.tv_nsec;
	LapPutOutQueue(lk,(u_char *)&uppkt,uppkt.lp_szh);
	RTL_TRDBG(1,"LRR send lrrid=%02x-%04x-%08x to lrc='%s'\n",
	uppkt.lp_lrridpref,uppkt.lp_lrridext,uppkt.lp_lrrid,
	lk->lk_rhost);

#ifdef TEKTELIC
	GetGpsPositionTektelic();
#endif
	if	(UseGpsPosition == 0)
	{	// use static GPS coordo
		uppkt.lp_u.lp_gpsco.li_gps	= 1;
		uppkt.lp_u.lp_gpsco.li_latt	= locallatt;
		uppkt.lp_u.lp_gpsco.li_long	= locallong;
		uppkt.lp_u.lp_gpsco.li_alti	= localalti;
	}
	else
	{	// dynamic coordo will sent later
		if	(UseGpsPosition && GpsPositionOk)
		{	// dynamic coordo are ready
			uppkt.lp_u.lp_gpsco.li_gps	= 2;
			uppkt.lp_u.lp_gpsco.li_latt	= GpsLatt;
			uppkt.lp_u.lp_gpsco.li_long	= GpsLong;
			uppkt.lp_u.lp_gpsco.li_alti	= GpsAlti;
		}
		else
		{	// dynamic coordo will sent later
			uppkt.lp_u.lp_gpsco.li_gps	= 0;
			uppkt.lp_u.lp_gpsco.li_latt	= locallatt;
			uppkt.lp_u.lp_gpsco.li_long	= locallong;
			uppkt.lp_u.lp_gpsco.li_alti	= localalti;
		}
	}
	uppkt.lp_type	= LP_TYPE_LRR_INF_GPSCO;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_gpsco);
	LapPutOutQueue(lk,(u_char *)&uppkt,uppkt.lp_szh);
RTL_TRDBG(1,"GPS send position (%f,%f,%d) mode=%d to lrc='%s'\n",
		uppkt.lp_u.lp_gpsco.li_latt,
		uppkt.lp_u.lp_gpsco.li_long,
		uppkt.lp_u.lp_gpsco.li_alti,
		uppkt.lp_u.lp_gpsco.li_gps,lk->lk_rhost);

	SendCapabToLrc(lk);
#ifdef	LP_TP31
	SendDtcSyncToLrc(lk);
#endif
	DoHtmlFile();
}

void	LoadDevAddr(char *lst)
{
	char	*delim	= " ,;|";
	char	*pt;
	char	*ctxt;
	u_int	devaddr;

	NbDevAddrIncl	= 0;
	NbDevAddrExcl	= 0;
	if	(!lst || !*lst)
		return;

	pt	= strtok_r(lst,delim,&ctxt);
	while	(pt && *pt)
	{
		if	(*pt == '!' || *pt == '-')
		{
			pt++;
			if	(sscanf(pt,"%x",&devaddr) == 1)
			{
				RTL_TRDBG(1,"devaddr %08x refused\n",devaddr);
				TbDevAddrExcl[NbDevAddrExcl]	= devaddr;
				NbDevAddrExcl++;
			}
		}	
		else
		{
			if	(sscanf(pt,"%x",&devaddr) == 1)
			{
				RTL_TRDBG(1,"devaddr %08x accepted\n",devaddr);
				TbDevAddrIncl[NbDevAddrIncl]	= devaddr;
				NbDevAddrIncl++;
			}
		}
		pt	= strtok_r(NULL,delim,&ctxt);
	}
}

static	void	ReadRffInfos()
{
	char	file[1024];
	char	line[1024];
	FILE	*f;
	char	*pt;

	VersionMajRff	= 0;
	VersionMinRff	= 0;
	VersionRelRff	= 0;
	VersionFixRff	= 0;
	RffTime	= 0;

	sprintf	(file,"%s/usr/etc/lrr/saverff_done",RootAct);
	f	= fopen(file,"r");
	if	(!f)
		return;

	while	(fgets(line,sizeof(line)-1,f))
	{
		if	((pt=strstr(line,"RFFVERSION=")))
		{
			pt	= strchr(line,'='); pt++;
			sscanf	(pt,"%d.%d.%d_%d",
			&VersionMajRff,&VersionMinRff,&VersionRelRff,&VersionFixRff);
			continue;
		}
		if	((pt=strstr(line,"RFFDATE=")))
		{
			pt	= strchr(line,'='); pt++;
			RffTime	= (time_t)rtl_iso8601_to_Unix(pt,0);
			continue;
		}
	}


	fclose(f);
}

void	MfsUsage(t_mfs *mfs)
{
	char		*path;
	struct	statfs	vfs;
	uint64_t	mega	= 1024*1024;
	uint64_t	bsize;
	uint64_t	info;

//	RTL_TRDBG(1,"MFS usage(%s)\n",path);

	mfs->fs_size	= 0;
	mfs->fs_used	= 0;
	mfs->fs_avail	= 0;
	mfs->fs_puse	= 0;
	if	(!mfs)
		return;
	path	= mfs->fs_name;
	if	(!path || !*path)
		return;


	if	(fstatfs(mfs->fs_fd,&vfs) != 0)
	{
		RTL_TRDBG(0,"cannot statfs(%s) fd=%d\n",path,mfs->fs_fd);
		return;
	}

	bsize	= vfs.f_bsize;
	info	= bsize * vfs.f_blocks;
	info	= info / mega;
	mfs->fs_size	= (u_int)info;

	if	(mfs->fs_size <= 0)
	{
		RTL_TRDBG(0,"cannot statfs(%s) size==0\n",path);
		return;
	}

	info	= bsize * vfs.f_bfree ;
	info	= info / mega;
	mfs->fs_avail	= (u_int)info;

	mfs->fs_used	= mfs->fs_size - mfs->fs_avail;
	mfs->fs_puse	= 
		(u_int)(100.0*(double)mfs->fs_used / (double)mfs->fs_size);

	RTL_TRDBG(3,"mfs=%s space=%u used=%u free=%u puse=%u\n",
		path,mfs->fs_size,mfs->fs_used,mfs->fs_avail,mfs->fs_puse);
}

void	CompAllMfsInfos(char shortlongtime)
{
	int	i;

	for	(i = 0 ; i < NB_MFS_PER_LRR ; i++)
	{
		if	(TbMfs[i].fs_enable == 0)	continue;
		if	(TbMfs[i].fs_exists == 0)	continue;
		MfsUsage(&TbMfs[i]);
	}
}

double	CompAllCpuInfos(char shortlongtime)
{
	static	u_int	tick	= 0;
	static	u_int	prev	= 0;
	FILE	*f;
	char	tmp[1024];
	int	ret;
	double	used	= 0.0;
	u_int	u,n,s,i;

	if	(shortlongtime == 'L')
	{
		float	l1,l5,l15;
		CpuLoad1	= 0;
		CpuLoad5	= 0;
		CpuLoad15	= 0;
		f	= fopen("/proc/loadavg","r");
		if	(!f)
			return	0.0;
		ret	= fscanf(f,"%f %f %f",&l1,&l5,&l15);
		fclose	(f);
		if	(ret != 3)
			return	0.0;
		CpuLoad1	= (u_short)(l1 * 100.0);
		CpuLoad5	= (u_short)(l5 * 100.0);
		CpuLoad15	= (u_short)(l15 * 100.0);
		return	0.0;
	}

	f	= fopen("/proc/stat","r");
	if	(!f)
		return	used;

	ret	= fscanf(f,"%s %u %u %u %u",tmp,&u,&n,&s,&i);
	fclose	(f);
	if	(ret != 5)
		return	used;
	s	= i;
	i	= ABS(i - prev);
	prev	= s;

	if	(tick == 0)
	{
		tick	= sysconf(_SC_CLK_TCK);
		return	used;			// first call return 0.0
	}

	used	= (double)i/(double)tick;	// idle
	used	= (1.0 - used)*100.0;		// used %
//	printf	("cpuused=%f\n",used);
	AvDvUiAdd(&CpuAvdvUsed,(u_int)used,Currtime.tv_sec);
	return	used;
}

void	CompAllMemInfos(char shortlongtime)
{
	int	fd;
	char	buff[1024];
	int	sz;
	char	*pt;

	MemTotal	= 0;
	MemFree		= 0;
	MemBuffers	= 0;
	MemCached	= 0;
	MemUsed		= 0;
	
	fd	= open("/proc/meminfo",0);
	if	(fd < 0)
		return;
	sz	= read(fd,buff,sizeof(buff)-1);
	close	(fd);
	if	(sz <= 0)
		return;
	buff[sz]	= '\0';

	pt	= buff;
	pt	= strstr(pt,"MemTotal:");
	if	(!pt || !*pt)
		return;
	pt	+= strlen("MemTotal:");
	MemTotal	= strtoul(pt,0,0);
	pt	= strstr(pt,"MemFree:");
	if	(!pt || !*pt)
		return;
	pt	+= strlen("MemFree:");
	MemFree	= strtoul(pt,0,0);
	pt	= strstr(pt,"Buffers:");
	if	(!pt || !*pt)
		return;
	pt	+= strlen("Buffers:");
	MemBuffers	= strtoul(pt,0,0);
	pt	= strstr(pt,"Cached:");
	if	(!pt || !*pt)
		return;
	pt	+= strlen("Cached:");
	MemCached	= strtoul(pt,0,0);

	MemUsed	= MemTotal - MemFree;
}

char	*DoConfigFileCustom(char *file)
{
	static	char	path[1024];

	sprintf	(path,"%s/%s",ConfigCustom,file);
	RTL_TRDBG(0,"search '%s'\n",path);
	if	(access(path,R_OK) == 0)
		return	path;
	RTL_TRDBG(0,"no custom configuration file '%s'\n",path);
	return	NULL;
}

char	*DoConfigFileDefault(char *file,char *suff)
{
	static	char	path[1024];

	if	(suff && *suff)
	{
		char	tmp[128];
		char	*pt;

		strcpy	(tmp,file);
		pt	= strstr(tmp,".ini");
		if	(pt)
		{
			*pt	= '\0';
			sprintf	(path,"%s/%s_%s.ini",ConfigDefault,tmp,suff);
		}
		else
			sprintf	(path,"%s/%s_%s",ConfigDefault,tmp,suff);
	}
	else
		sprintf	(path,"%s/%s",ConfigDefault,file);
	RTL_TRDBG(0,"search '%s'\n",path);
	if	(access(path,R_OK) == 0)
		return	path;
#if	0
	RTL_TRDBG(0,"cannot find default configuration file '%s'\n",path);
	sleep	(1);
	exit	(1);
#endif
	return	NULL;	// ism.band vs ism.bandlocal
}

void	SaveConfigFileState()
{
	char	path[1024];
	FILE	*f;

	sprintf	(path,"%s/%s",ConfigCustom,ConfigFileState);
	f	= fopen(path,"w");
	if	(!f)
		return;

	fprintf	(f,";;;; file generated by LRR process do not change it\n");
	fprintf	(f,"[lrr]\n");

	if	(CfgRadioState)
	{
		fprintf	(f,"\tradiostopped=%d\n",CfgRadioStop);
		fprintf	(f,"\tradiodnstopped=%d\n",CfgRadioDnStop);
	}
	else
	{
		fprintf	(f,";\tradiostopped=%d\n",CfgRadioStop);
		fprintf	(f,";\tradiodnstopped=%d\n",CfgRadioDnStop);
	}

	if	(LrrIDFromTwa != 0)
	{
		fprintf	(f,"\tuidfromtwa=0x%08x\n",LrrIDFromTwa);
	}

	if	(TraceLevelP > 0)
	{
		fprintf	(f,"[trace]\n");
		fprintf	(f,"\tlevelp=%d\n",TraceLevelP);
	}
	else
	{
		fprintf	(f,";[trace]\n");
		fprintf	(f,";\tlevelp=%d\n",TraceLevelP);
	}

	if	(LrrIDFromBS != 0)
		fprintf	(f,"\tuidfrombootsrv=0x%08x\n",LrrIDFromBS);

	fclose	(f);
}

char	*LrrPktFlagsTxt(unsigned int type)
{
	static	char	buf[64];

	buf[0]	= '\0';
	if	(type & LP_INFRA_PKT_INFRA)
	{
		strcat(buf,"I");
		return	buf;
	}

	strcat(buf,"R");
	if	(type & LP_RADIO_PKT_UP)
		strcat(buf,"U");
	if	(type & LP_RADIO_PKT_DOWN)
		strcat(buf,"D");
	if	(type & LP_RADIO_PKT_802154)
		strcat(buf,"8");
	if	(type & LP_RADIO_PKT_ACKMAC)
		strcat(buf,"A");
	if	(type & LP_RADIO_PKT_ACKDATA)
		strcat(buf,"+");
	if	(type & LP_RADIO_PKT_DELAY)
		strcat(buf,"Y");
	if	(type & LP_RADIO_PKT_PINGSLOT)
		strcat(buf,"B");
	if	(type & LP_RADIO_PKT_LORA_E)
		strcat(buf,"E");
	if	(type & LP_RADIO_PKT_RX2)
		strcat(buf,"2");

	return	buf;
}

static	void	CBHtDumpLrr(char *var,void *value)
{
	RTL_TRDBG(9,"var='%s' val='%s'\n",var,(char *)rtl_htblGet(HtVarLrr,var));
//	printf("var='%s' val='%s'\n",var,(char *)rtl_htblGet(HtVarLrr,var));
}

#ifdef CISCOMS

// Folowing functions csn* are used to get a LRR ID from a Cisco S/N
int csnCheckformat(char *str)
{
	char	*pt;
	int	i;

	pt = str;

	if (!pt || !*pt)
		return 0;

	// check LLL
	for (i=0; i<3; i++)
		if (!isupper(*pt++))
			return 0;
	// check YYWW
	for (i=0; i<4; i++)
		if (!isdigit(*pt++))
			return 0;
	// check SSSS
	for (i=0; i<4; i++)
	{
		if (!isupper(*pt) && !isdigit(*pt))
			return 0;
		pt++;
	}
	return 1;
}

// encode alphanum character
// '0' - '9' => 0 - 9
// 'A' - 'Z' => 10 - 36
int csnCodeAlNum(char c)
{
	int	res;
	if (isdigit(c))
		res = c - '0';
	else
		res = c - 'A' + 10;
	RTL_TRDBG(4, "csnCodeAlNum(%c) = %d\n", c, res);
	return res;
}

// get a LRR ID from a Cisco S/N
uint32_t csnCode(char *str)
{
	uint32_t	res;
	uint32_t	lll, yy, ww, ssss;
	char	*pt, tmp[10];



	if (!csnCheckformat(str))
	{
		RTL_TRDBG(0, "Incorrect format Cisco S/N '%s', must be LLLDDDDAAAA where L is an uppercase letter, D a digit, and A an uppercase letter or a digit\n", str);
		return -1;
	}

	pt = str;
	// keep L % 4
	lll = 	((*pt-'A') % 4) * 4*4 +
		((*(pt+1)-'A') % 4) * 4 +
		(*(pt+2)-'A') % 4;
	RTL_TRDBG(4, "csnCode(%s): lll=%d\n", str, lll);

	// keep YY % 3
	pt += 3;
	tmp[0] = *pt++;
	tmp[1] = *pt++;
	tmp[2] = '\0';
	yy = atoi(tmp) % 3;
	RTL_TRDBG(4, "csnCode(%s): yy=%d (%s)\n", str, yy, tmp);

	// keep WW % 26
	tmp[0] = *pt++;
	tmp[1] = *pt++;
	tmp[2] = '\0';
	ww = (atoi(tmp)-1) % 26;
	RTL_TRDBG(4, "csnCode(%s): ww=%d (%s)\n", str, ww, tmp);

	// keep S % 18 for the first one, and the full value for the others
	ssss = 	(csnCodeAlNum(*pt) % 18) * 36*36*36 +
	 	csnCodeAlNum(*(pt+1)) * 36*36 +
	 	csnCodeAlNum(*(pt+2)) * 36 +
	 	csnCodeAlNum(*(pt+3));
	RTL_TRDBG(4, "csnCode(%s): ssss=%d\n", str, ssss);

	res =	(lll * 3*26*18*36*36*36) +
		(yy * 26*18*36*36*36) +
		(ww * 18*36*36*36) +
		ssss;

	RTL_TRDBG(3, "csnCode(%s) = 0x%08x\n", str, res);
	return res;
}
#endif

static	u_int	DoLrrID(char *str,u_short *ext,u_char *pref)
{
	u_int	ret	= 0;

	if	(!str || !*str || !ext || !pref)
		return	0;

	*pref	= LP_LRRID_NO_PREF;
	*ext	= 0;

	if	(strcmp(str,"hostname_mac/32") == 0 )
	{
#ifdef	WIRMAV2
#if	0
		char	*pt;
		char	host[HOST_NAME_MAX+1];

		if	(gethostname(host,HOST_NAME_MAX) != 0)
		{
			RTL_TRDBG(0,"cannot get hostname\n");
			exit(1);
		}
		pt	= host;
		while	(*pt && *pt != '_')	pt++;
		if	(*pt == '_')
		{
			pt++;
			sscanf	(pt,"%x",&ret);
			return	ret;
		}
#endif
#endif
#if defined(RBPI_V1_0) || defined(NATRBPI_USB)
		char	*pt;
		uint64_t	serial;
		char		line[1024];
		FILE		*f;
		f	= fopen("/proc/cpuinfo","r");
		if	(f)
		while	(fgets(line,sizeof(line)-1,f))
		{
			if	(!strstr(line,"Serial"))	continue;
			pt	= strchr(line,':');
			if	(!pt)				return	0;
			serial	= strtoull(pt+1, NULL, 16);
			ret	= (u_int)serial;
			*pref	= LP_LRRID_RBPI_CPUID;	// raspberry cpu id
			*ext	= 0;			// not used
			return	ret;
		}
#endif
#ifdef	CISCOMS
		char * pt = NULL;
		pt = CfgStr(HtVarSys,"",-1,"CISCOSN","");
		if (pt && *pt)
		{
			*pref	= LP_LRRID_CISCO_SN;	// cisco id
			*ext	= 0;			// not used
			ret = csnCode(pt);
			return ret;
		}
#endif
		*pref	= LP_LRRID_FULL_MAC;		// full mac address
		ret	= FindEthMac32(ext);
		if	(ret)
			return	ret;
		return	0;
	}

	*pref	= LP_LRRID_BY_CONFIG;			// set by configuration
	*ext	= 0x00;					// not used
	if	(*str == '0' && (*(str+1) == 'x' || *(str+1) == 'X') )
		sscanf	(str,"%x",&ret);
	else
		ret	= (u_int)atoi(str);
	return	ret;
}

// Test if getting lrrid from TWA is configured
static	void	DoLrrIDFromTwa(char *str)
{
	if (LrrIDGetFromBS)	// NFR997
	{
		RTL_TRDBG(1,"Getting lrrid from twa is disabled (got from bootserver)\n");
		return;
	}

	if	(!str || !*str || strcmp(str,"local") == 0)
	{
		RTL_TRDBG(1,"Getting lrrid from twa is disabled\n");
		return;
	}

	if	(strcmp(str,"fromtwa") == 0 || strcmp(str,"fromtwaonly") == 0)
	{
		char	*pt;
		int	ret	= 0;

		pt	= CfgStr(HtVarLrr,"lrr",-1,"uidfromtwa","0");
		if	(*pt == '0' && (*(pt+1) == 'x' || *(pt+1) == 'X') )
			sscanf	(pt,"%x",&ret);
		else
			ret	= (u_int)atoi(pt);
		RTL_TRDBG(1,"Getting lrrid from twa is activated %08x\n",ret);
		if	(ret != 0)
		{	// twa already give us a lrrid
			LrrIDGetFromTwa	= 0;
			LrrIDFromTwa	= ret;
			LrrID		= ret;
		}
		else
		{	// request lrrid from twa
			LrrIDGetFromTwa = 1;
			if	(strcmp(str,"fromtwaonly") == 0)
				LrrIDGetFromTwa = 2;
		}
	}
	else
		RTL_TRDBG(1,"Getting lrrid from twa is disabled\n");
}

// the custom configuration can not overload defines "defines.ini"
// the defines are loaded in the 2 hash table HtVarLrr and HtVarLgw
static	void	LoadConfigDefine(int hot,int dumponly)
{
	int	err;
	char	*file;

	file	= DoConfigFileCustom(ConfigFileSystem);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarSys)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	file	= DoConfigFileCustom(ConfigFileParams);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarSys)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	file	= DoConfigFileDefault(ConfigFileDefine,NULL);
	if	(file)
	{
		RTL_TRDBG(0,"load default '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	file	= DoConfigFileDefault(ConfigFileDefine,NULL);
	if	(file)
	{
		RTL_TRDBG(0,"load default '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	LgwForceDefine(HtVarLrr);
	LgwForceDefine(HtVarLgw);
}

static	int	AdjustFreqForCalibration(char *ism)
{
	char	*pt;
	int	f;

	pt	= ism;
	while	(*pt < '0' || *pt > '9') pt++;
	if	(!*pt)
		return	868;
	f	= atoi(pt);
	if	(f < 100 || f >= 1000)
		return	868;

	int	minDiff[3];
	int	nb = sizeof(minDiff)/sizeof(int);
	int	i,idxmin;
	int	min	= INT_MAX;

	// TODO we only have 3 LUT tables
	minDiff[0]	= 868;
	minDiff[1]	= 915;
	minDiff[2]	= 920;

	idxmin	= 0;
	for	(i = 0 ; i < nb ; i++)
	{
		int	ret;

		ret	= ABS(minDiff[i]-f);
		if	(ret < min)
		{
			min	= ret;
			idxmin	= i;
		}
	}

	return	minDiff[idxmin];
}

// Create or remove symbolic link on trace directory depending on the configuration
void LinkTraceDir()
{
	char	*trdir1, *trdir2, *trdir, tmp[256];
	int	disabled = 0;

	// read lrr.ini:[trace].ramdir
	trdir1	= CfgStr(HtVarLrr,"trace",-1,"ramdir",NULL);
	// read default lrr.ini:[<system>].ramdir
	trdir2	= CfgStr(HtVarLrr,System,-1,"ramdir",NULL);

	// ramdir set to empty in lrr.ini -> feature disabled
	if (trdir1 && !*trdir1)
		disabled = 1;

	// ramdir not set in lrr.ini and also not set in default lrr.ini
	if (!trdir1 && (!trdir2 || !*trdir2))
		disabled = 1;

	if (disabled)
	{
		// ramdir function disabled
		// remove link on var/log/lrr if it's a symbolic link
		system("[ -L \"$ROOTACT/var/log/lrr\" ] && rm \"$ROOTACT/var/log/lrr\"");
		LogUseRamDir = 0;
		return;
	}

	// trdir = ramdir of lrr.ini if set, or ramdir of default lrr.ini 
	trdir = trdir1 ? trdir1 : trdir2;

	// ramdir function enabled
	LogUseRamDir = 1;
	// create target dir if needed
	sprintf(tmp, "[ ! -d \"%s\" ] && mkdir -p \"%s\"", trdir, trdir);
	system(tmp);

	// remove var/log/lrr if it's a standard directory, in order to create a symbolic link
	system("[ -d \"$ROOTACT/var/log/lrr\" ] && rm -rf \"$ROOTACT/var/log/lrr\"");

	// remove var/log/lrr if it's a link, to create it correctly, because
	// it's difficult to check if an existing one is the good one
	system("[ -L \"$ROOTACT/var/log/lrr\" ] && rm \"$ROOTACT/var/log/lrr\"");

	// create link
	sprintf(tmp, "ln -s \"%s\" \"$ROOTACT/var/log/lrr\"", trdir);
	system(tmp);
}

static	void	LoadConfigLrr(int hot,int dumponly)
{
	int	err;
	int	i;
	char	*file;
	char	*strid;
	char	*stridmode;
	char	*pt;
	char	section[128];
	char 	tmpstrvalue[8] = {0};

	file	= DoConfigFileDefault(ConfigFileLrr,NULL);
	if	(file)
	{
		RTL_TRDBG(0,"load default '%s'\n",file);
	}
	if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}
	file	= DoConfigFileCustom(ConfigFileLrr);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}
	file	= DoConfigFileCustom(ConfigFileGps);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}
	file	= DoConfigFileCustom(ConfigFileCustom);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);

		exit(1);
	}
	if (LrrIDGetFromBS)	// NFR997
	{
		file	= DoConfigFileCustom(ConfigFileDynLap);
		if	(file)
		{
			RTL_TRDBG(0,"load custom '%s'\n",file);
		}
		if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
		{
			RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
			exit(1);
		}
	}
	file	= DoConfigFileCustom(ConfigFileState);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	rtl_htblDump(HtVarLrr,CBHtDumpLrr);

	if	(!dumponly)
		DecryptHtbl(HtVarLrr);

	IsmBand		= CfgStr(HtVarLrr,"ism",-1,"band",IsmBand);
	IsmBandAlter	= CfgStr(HtVarLrr,"ism",-1,"bandalter","");
	if	(!IsmBandAlter || !*IsmBandAlter)
		IsmBandAlter	= CfgStr(HtVarLrr,"ism",-1,"bandlocal","");
	if	(strlen(IsmBandAlter) == 0)
	{
#ifdef	LP_TP31
		if	(strcmp(IsmBand,"eu868") == 0)
			IsmBandAlter	= strdup("eu868_2015");
		else
			IsmBandAlter	= strdup(IsmBand);
#else
		IsmBandAlter	= strdup(IsmBand);
#endif
	}

	if	(strcmp(IsmBand,"eu868") == 0)
	{
		IsmFreq		= 868;
	}	else
	if	(strcmp(IsmBand,"us915") == 0)
	{
		IsmAsymDownlink	= 1;
		IsmFreq		= 915;
	}	else
	if	(strcmp(IsmBand,"au915") == 0)
	{
		IsmAsymDownlink	= 1;
		IsmAsymDnModulo	= 8;
		IsmAsymDnOffset	= 0;
		IsmFreq		= 915;
	}	else
	if	(strcmp(IsmBand,"sg920") == 0)
	{
		IsmFreq		= 920;
	}	else
	if	(strcmp(IsmBand,"as923") == 0)
	{
		IsmFreq		= 920;
	}	else
	if	(strcmp(IsmBand,"kr920") == 0)
	{
		IsmFreq		= 920;
	}	else
	if	(strcmp(IsmBand,"cn779") == 0)
	{
		IsmFreq		= 779;
	}	else
	if	(strcmp(IsmBand,"cn470") == 0)
	{
		IsmAsymDownlink	= 1;
		IsmFreq		= 470;
	}	else
	{
		IsmFreq		= AdjustFreqForCalibration(IsmBand);
	}

	IsmFreq		= CfgInt(HtVarLrr,"ism",-1,"freq",IsmFreq);
	IsmAsymDownlink	= CfgInt(HtVarLrr,"ism",-1,"asymdownlink",
							IsmAsymDownlink);
	IsmAsymDnModulo = CfgInt(HtVarLrr,"ism",-1,"asymdownlinkmodulo",
							IsmAsymDnModulo);
	IsmAsymDnOffset = CfgInt(HtVarLrr,"ism",-1,"asymdownlinkoffset",
							IsmAsymDnOffset);
#if	defined(WIRMAAR) || defined(WIRMAV2) || defined(FCPICO) || defined(FCLAMP) || defined(FCLOC) || defined(FCMLB) || defined(CISCOMS) || defined(GEMTEK) || defined(WIRMANA) || defined(MTAC)
	LinkTraceDir();		// see LogUseRamDir
#endif
	TraceLevel	= CfgInt(HtVarLrr,"trace",-1,"level",TraceLevel);
	TraceLevelP	= CfgInt(HtVarLrr,"trace",-1,"levelp",0);
	if		(TraceLevelP > 0)
			TraceLevel	= TraceLevelP;
	TraceDebug	= CfgInt(HtVarLrr,"trace",-1,"debug",TraceDebug);
	if		(dumponly)	TraceDebug	= 0;
	TraceSize	= CfgInt(HtVarLrr,"trace",-1,"size",TraceSize);
#ifdef	WIRMAV2
	TraceSize	= 10*1000*1000;
#endif
	TraceFile	= CfgStr(HtVarLrr,"trace",-1,"file","TRACE.log");
	TraceStdout	= CfgStr(HtVarLrr,"trace",-1,"stdout",NULL);
	StatRefresh	= CfgInt(HtVarLrr,"lrr",-1,"statrefresh",StatRefresh);
	RfCellRefresh	= CfgInt(HtVarLrr,"lrr",-1,"rfcellrefresh",RfCellRefresh);
	ConfigRefresh	= CfgInt(HtVarLrr,"lrr",-1,"configrefresh",ConfigRefresh);
	WanRefresh	= CfgInt(HtVarLrr,"lrr",-1,"wanrefresh",WanRefresh);
	GpsStRefresh	= CfgInt(HtVarLrr,"lrr",-1,"gpsstatusrefresh",GpsStRefresh);
	sprintf(tmpstrvalue, "%.2f", GpsStRateHigh);
	GpsStRateHigh	= atof(CfgStr(HtVarLrr,"lrr",-1,"gpsstatusratehigh", tmpstrvalue));
	sprintf(tmpstrvalue, "%.2f", GpsStRateLow);
	GpsStRateLow	= atof(CfgStr(HtVarLrr,"lrr",-1,"gpsstatusratelow", tmpstrvalue));

	LrrLat		= atof(CfgStr(HtVarLrr,"lrr",-1,"lat","90.0"));
	LrrLon		= atof(CfgStr(HtVarLrr,"lrr",-1,"lon","90.0"));
	LrrAlt		= CfgInt(HtVarLrr,"lrr",-1,"alt",0);

	AdjustDelay	= CfgInt(HtVarLrr,"lrr",-1,"adjustdelay",0);
	NbLrc		= CfgInt(HtVarLrr,"lrr",-1,"nblrc",1);
	if	(NbLrc > NB_LRC_PER_LRR)	NbLrc	= NB_LRC_PER_LRR;
	GpsDevice	= CfgStr(HtVarLrr, System, -1, "gpsdevice", GpsDevice);
	UseGpsPosition	= CfgInt(HtVarLrr,"lrr",-1,"usegpsposition",UseGpsPosition);
	UseGpsTime	= CfgInt(HtVarLrr,"lrr",-1,"usegpstime",UseGpsTime);
	UseLgwTime	= CfgInt(HtVarLrr,"lrr",-1,"uselgwtime",UseLgwTime);
	Redundancy	= CfgInt(HtVarLrr,"lrr",-1,"redundancy",Redundancy);
	CfgRadioState	= CfgInt(HtVarLrr,"lrr",-1,"radiostatesaved",0);
	CfgRadioStop	= CfgInt(HtVarLrr,"lrr",-1,"radiostopped",0);
	CfgRadioDnStop	= CfgInt(HtVarLrr,"lrr",-1,"radiodnstopped",0);

	LgwThreadStopped= CfgRadioStop;


	SimulN		= CfgInt(HtVarLrr,"lrr",-1,"simulN",0);
	QosAlea		= CfgInt(HtVarLrr,"lrr",-1,"qosalea",0);
	pt		= CfgStr(HtVarLrr,"lrr",-1,"devaddr","");
	LoadDevAddr(pt);

#ifdef	LP_TP31
	StorePktCount	= 
		CfgInt(HtVarLrr,"uplinkstorage",-1,"pktcount",StorePktCount);
	pt	= CfgStr(HtVarLrr,"uplinkstorage",-1,"memused","");
	if	(pt && *pt)
		StoreMemUsed	= atof(pt);
	ReStorePerSec	= 
		CfgInt(HtVarLrr,"uplinkstorage",-1,"rstrpersec",ReStorePerSec);
	ReStoreCtrlOutQ	= 
		CfgInt(HtVarLrr,"uplinkstorage",-1,"ctrloutq",ReStoreCtrlOutQ);
	ReStoreCtrlAckQ	= 
		CfgInt(HtVarLrr,"uplinkstorage",-1,"ctrlackq",ReStoreCtrlAckQ);
#endif

	if	(UseGpsPosition || UseGpsTime)
		UseGps	= 1;

	for	(i = 0 ; i < NbLrc ; i++)
	{
		strcpy	(TbLapLrc[i].lk_name,
				CfgStr(HtVarLrr,"laplrc",i,"name","slave"));
		TbLapLrc[i].lk_addr = CfgStr(HtVarLrr,"laplrc",i,"addr","0.0.0.0");
		TbLapLrc[i].lk_port = CfgStr(HtVarLrr,"laplrc",i,"port","2404");

		TbLapLrc[i].lk_type = (u_int)CfgInt(HtVarLrr,"laplrc",i,
					"type",LapFlags);
		TbLapLrc[i].lk_type	= LapFlags;	// TODO


		TbLapLrc[i].lk_t1 = (u_int)CfgInt(HtVarLrr,"laplrc",i,
					"iec104t1",DEFAULT_T1);
		TbLapLrc[i].lk_t2 = (u_int)CfgInt(HtVarLrr,"laplrc",i,
					"iec104t2",DEFAULT_T2);
		TbLapLrc[i].lk_t3 = (u_int)CfgInt(HtVarLrr,"laplrc",i,
					"iec104t3",LRR_DEFAULT_T3);


		if	(SimulN == 0)	// legacy case
		{
		TbLrrID[i] = LrrID;
		TbLrrLat[i] = LrrLat;
		TbLrrLon[i] = LrrLon;
		TbLrrAlt[i] = LrrAlt;
		}
		else			// simulate several lrr
		{
		TbLrrID[i] = (u_int)CfgInt(HtVarLrr,"laplrc",i,"uid",LrrID);
		TbLrrLat[i] = atof(CfgStr(HtVarLrr,"laplrc",i,"lat","90.0"));
		TbLrrLon[i] = atof(CfgStr(HtVarLrr,"laplrc",i,"lon","90.0"));
		TbLrrAlt[i] = CfgInt(HtVarLrr,"laplrc",i,"alt",0);
		}
RTL_TRDBG(1,"laplrc=%d addr='%s:%s' flg=%x t1=%d t2=%d t3=%d \n\t\t\tlrrid=%x lat=%f lon=%f alt=%d\n",
		i,TbLapLrc[i].lk_addr,TbLapLrc[i].lk_port,
		TbLapLrc[i].lk_type,TbLapLrc[i].lk_t1,
		TbLapLrc[i].lk_t2,TbLapLrc[i].lk_t3,
		TbLrrID[i],TbLrrLat[i],TbLrrLon[i],TbLrrAlt[i]);
	}

	for	(i = 0; i < NB_ITF_PER_LRR ; i++)
	{
		char	*pt;

		sprintf	(section,"%s/netitf",System);

		TbItf[i].it_enable	= CfgInt(HtVarLrr,section,i,"enable",0);
		TbItf[i].it_type	= CfgInt(HtVarLrr,section,i,"type",0);

		pt			= CfgStr(HtVarLrr,section,i,"name",0);
		if	(!pt || !*pt)
		{
			TbItf[i].it_enable	= 0;
			pt			= "";
		}
		TbItf[i].it_name	= strdup(pt);
		NbItf++;
	}
	FindIpInterfaces(&ConfigIpInt);
	for	(i = 0; i < NB_ITF_PER_LRR ; i++)
	{
RTL_TRDBG(1,"netitf=%d enable=%d name='%s' exists=%d type=%d\n",i,
	TbItf[i].it_enable,TbItf[i].it_name,TbItf[i].it_exists,TbItf[i].it_type);
	}

	for	(i = 0; i < NB_MFS_PER_LRR ; i++)
	{
		char	*pt;

		sprintf	(section,"%s/mfs",System);

		TbMfs[i].fs_enable	= CfgInt(HtVarLrr,section,i,"enable",0);
		pt			= CfgStr(HtVarLrr,section,i,"type","?");
		if	(!pt || !*pt)
			pt		= "?";
		TbMfs[i].fs_type	= *pt;

		pt			= CfgStr(HtVarLrr,section,i,"name",0);
		if	(!pt || !*pt)
		{
			TbMfs[i].fs_enable	= 0;
			pt			= "";
		}
		TbMfs[i].fs_name	= strdup(pt);
		TbMfs[i].fs_exists	= 1;
		TbMfs[i].fs_fd		= open(TbMfs[i].fs_name,O_RDONLY|O_CLOEXEC);
		if	(TbMfs[i].fs_fd < 0)
		{
			TbMfs[i].fs_exists	= 0;
		}
		NbMfs++;
		RTL_TRDBG(1,"mfs=%d mfs=%s enable=%d exists=%d type=%c\n",
		i,TbMfs[i].fs_name,TbMfs[i].fs_enable,TbMfs[i].fs_exists,
		TbMfs[i].fs_type);
	}

	sprintf	(section,"%s/power",System);
	PowerEnable	= CfgInt(HtVarLrr,section,-1,"enable",PowerEnable);
	pt		= CfgStr(HtVarLrr,section,-1,"device",PowerDevice);
	if	(!pt || !*pt)	PowerEnable	= 0;
	PowerDownLev	= CfgInt(HtVarLrr,section,-1,"down",PowerDownLev);
	PowerUpLev	= CfgInt(HtVarLrr,section,-1,"up",PowerUpLev);
	if	(PowerDownLev <= 0)		PowerEnable	= 0;
	if	(PowerUpLev <= 0)		PowerEnable	= 0;
	if	(PowerEnable)	PowerDevice	= strdup(pt);

	sprintf	(section,"%s/temperature",System);
	TempEnable	= CfgInt(HtVarLrr,section,-1,"enable",TempEnable);
	pt		= CfgStr(HtVarLrr,section,-1,"device",TempDevice);
	if	(!pt || !*pt)	TempEnable	= 0;
	if	(TempEnable)
	{
		TempDevice	= strdup(pt);
		TempPowerAdjust	= 
		CfgInt(HtVarLrr,section,-1,"poweradjust",TempPowerAdjust);
		TempExtAdjust	= 
		CfgInt(HtVarLrr,section,-1,"extadjust",TempExtAdjust);
	}
		

	Sx13xxPolling	= CfgInt(HtVarLrr,System,-1,"sx13xxpolling",
							Sx13xxPolling);
	Sx13xxStartDelay= CfgInt(HtVarLrr,System,-1,"sx13xxstartdelay",
							Sx13xxStartDelay);
#if defined(FCMLB) || defined(FCPICO) || defined(GEMTEK) || defined(FCLAMP)
	SpiDevice	= CfgStr(HtVarLrr,System,-1,"spidevice", SpiDevice);
	if (SpiDevice && *SpiDevice)
		setenv("SPIDEVICE", strdup(SpiDevice),1);
	RTL_TRDBG(1,"SpiDevice '%s' (system=%s)\n", SpiDevice, System);
#elif defined(REF_DESIGN_V2)
	sprintf	(section,"%s/spidevice",System);
	for (i=0; i<SX1301AR_MAX_BOARD_NB; i++)
	{
		SpiDevice[i]	= CfgStr(HtVarLrr,section,i,"device", "");
		// TODO: check why using setenv
		if (SpiDevice[i] && *SpiDevice[i])
		{
			char	tmp[40];
			if (i == 0)
				setenv("SPIDEVICE", strdup(SpiDevice[i]),1);
			sprintf(tmp, "SPIDEVICE%d", i);
			setenv(tmp, strdup(SpiDevice[i]),1);
		}
		RTL_TRDBG(1,"SpiDevice[%d] '%s' (system=%s)\n", i, SpiDevice[i], System);
	}
#endif /* defined(REF_DESIGN_V2) */

#ifdef	WITH_TTY
	TtyDevice	= CfgStr(HtVarLrr,System,-1,"ttydevice", TtyDevice);
	if (TtyDevice && *TtyDevice)
		setenv("TTYDEVICE", strdup(TtyDevice),1);
	RTL_TRDBG(1,"TtyDevice '%s' (system=%s)\n", TtyDevice, System);
#endif

	IfaceDaemon	= CfgInt(HtVarLrr,"ifacefailover",-1,"enable",
								IfaceDaemon);
	AutoRebootTimer	= CfgInt(HtVarLrr,"lrr",-1,"autoreboottimer",
							AutoRebootTimer);
	if	(AutoRebootTimer > 0 && AutoRebootTimer < 60)
		AutoRebootTimer	= 60;

	AutoRebootTimerNoUplink = CfgInt(HtVarLrr, "lrr", -1, "autoreboottimer_nouplink", AutoRebootTimerNoUplink);

	if	(AutoRebootTimerNoUplink > 0 && AutoRebootTimerNoUplink < 60)
		AutoRebootTimerNoUplink = 60;

	AutoRestartLrrMaxNoUplink = CfgInt(HtVarLrr, "lrr", -1, "autorestartlrrcnt_nouplink", AutoRestartLrrMaxNoUplink);

	AutoRevSshTimer	= CfgInt(HtVarLrr,"lrr",-1,"autoreversesshtimer",
							AutoRevSshTimer);
	if	(AutoRevSshTimer > 0 && AutoRevSshTimer < 60)
		AutoRevSshTimer	= 60;

	PingRttPeriod	= CfgInt(HtVarLrr,"lrr",-1,"pingrttperiod",
							PingRttPeriod);
	if	(PingRttPeriod > 0 && PingRttPeriod < 60)
		PingRttPeriod	= 60;

	strid		= CfgStr(HtVarLrr,"lrr",-1,"uid","0");
	LrrID		= DoLrrID(strid,&LrrIDExt,&LrrIDPref);
	// NFR684
	stridmode	= CfgStr(HtVarLrr,"lrr",-1,"uidmode","local");
	DoLrrIDFromTwa(stridmode);

	if (LrrIDGetFromBS)	// NFR997
		LrrID	= (u_int) CfgInt(HtVarLrr,"lrr",-1,"uidfrombootsrv",0);

{
RTL_TRDBG(1,"LrrID '%s' lrrid=%u lrrid=%08x lrridext=%04x lrridpref=%02x\n",
		strid,LrrID,LrrID,LrrIDExt,LrrIDPref);
RTL_TRDBG(1,"ism='%s' ismused='%s'\n",
		IsmBand,IsmBandAlter);
RTL_TRDBG(1,"asym=%d asymmodulo=%d asymoffset=%d\n",
		IsmAsymDownlink,IsmAsymDnModulo,IsmAsymDnOffset);
RTL_TRDBG(1,"lat=%f lon=%f alt=%d Dev='%s' UseGpsPosition=%d\n",
		LrrLat,LrrLon,LrrAlt,GpsDevice,UseGpsPosition);
RTL_TRDBG(1,"UseGpsTime=%d UseLgwTime=%d\n",
		UseGpsTime,UseLgwTime);
RTL_TRDBG(1,"nblrc=%d redundancy=%d simulN=%d qosalea=%d\n",
		NbLrc,Redundancy,SimulN,QosAlea);
RTL_TRDBG(1,"sx13xxpolling=%d sx13xxstartdelay=%d\n",
		Sx13xxPolling,Sx13xxStartDelay);
RTL_TRDBG(1,"ifacedaemon=%d autoreboottimer=%d autoreversesshtimer=%d\n",
			IfaceDaemon,AutoRebootTimer,AutoRevSshTimer);
}

	for	(i = 0 ; i < NB_ANTENNA ; i++)
	{
		AntennaGain[i] = atof(CfgStr(HtVarLrr,"antenna",i,"gain", "0"));
		CableLoss[i] = atof(CfgStr(HtVarLrr,"antenna",i,"cableloss", "0"));
		if (CableLoss[i] == 0.0)
			CableLoss[i] = atof(CfgStr(HtVarLrr,"antenna",i,"cablelost", "0"));
		if (AntennaGain[i] > ANTENNA_GAIN_MAX) {
			RTL_TRDBG(1, "Warning: use antenna gain max %.1f dBm\n", ANTENNA_GAIN_MAX);
			AntennaGain[i] = ANTENNA_GAIN_MAX;
		}
		if (CableLoss[i] > ANTENNA_GAIN_MAX) {
			RTL_TRDBG(1, "Warning: use antenna cable loss max %.1f dBm\n", ANTENNA_GAIN_MAX);
			CableLoss[i] = ANTENNA_GAIN_MAX;
		}
		RTL_TRDBG(1, "Antenna %d gain=%.1f dBm cableloss=%.1f dBm\n", i, AntennaGain[i], CableLoss[i]);
	}

	OnConfigErrorExit = CfgInt(HtVarLrr,"exitonerror",-1,"configure",0);
	OnStartErrorExit = CfgInt(HtVarLrr,"exitonerror",-1,"start",0);

	/* NFR590 */
	CustomVersion_Hw = CfgStr(HtVarLrr, "versions", -1, "hardware_version", "");
	CustomVersion_Os = CfgStr(HtVarLrr, "versions", -1, "os_version", "");
	CustomVersion_Hal = CfgStr(HtVarLrr, "versions", -1, "hal_version", "");
	CustomVersion_Build = CfgStr(HtVarLrr, "versions", -1, "custom_build_version", "");
	CustomVersion_Config = CfgStr(HtVarLrr, "versions", -1, "configuration_version", "");
	CustomVersion_Custom1 = CfgStr(HtVarLrr, "versions", -1, "custom1_version", "");
	CustomVersion_Custom2 = CfgStr(HtVarLrr, "versions", -1, "custom2_version", "");
	CustomVersion_Custom3 = CfgStr(HtVarLrr, "versions", -1, "custom3_version", "");

	RTL_TRDBG(1, "Versions:\n\tlrr=%d.%d.%d_%d\n\thardware=%s\n\tos=%s\n\thal=%s\n\tcustom_build=%s\n\tconfiguration=%s\n\tcustom1=%s\n\tcustom2=%s\n\tcustom3=%s\n", \
		VersionMaj, VersionMin, VersionRel, VersionFix, \
		CustomVersion_Hw, CustomVersion_Os, CustomVersion_Hal, CustomVersion_Build, \
		CustomVersion_Config, CustomVersion_Custom1, CustomVersion_Custom2, CustomVersion_Custom3);

	if	(CfgInt(HtVarLrr,"labonly",-1,"justreadconfig",0)>=1)
		exit(1);
}

// search bootserver.ini
static	int	LoadConfigBootSrv(int dumponly)
{
	int	err;
	char	*file;

	file	= DoConfigFileCustom(ConfigFileBootSrv);
	if	(!file)
		return 0;

	RTL_TRDBG(0,"load custom '%s'\n",file);

	if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLrr)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	rtl_htblDump(HtVarLrr,CBHtDumpLrr);

	if	(!dumponly)
		DecryptHtbl(HtVarLrr);

	return 1;
}

static	void	CBHtDumpLgw(char *var,void *value)
{
	RTL_TRDBG(9,"var='%s' val='%s'\n",var,(char *)rtl_htblGet(HtVarLgw,var));
}

static	void	LoadConfigLgw(int hot)
{
	int	err;
	char	*file;

	file	= DoConfigFileDefault(ConfigFileLowLvLgw,NULL);
	if	(file)
	{
		RTL_TRDBG(0,"load default '%s'\n",file);
		if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
		{
			RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
			exit(1);
		}
	}
	file	= DoConfigFileDefault(ConfigFileLowLvLgw,System);
	if	(file)
	{
		RTL_TRDBG(0,"load default '%s'\n",file);
		if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
		{
			RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
			exit(1);
		}
	}

	file	= DoConfigFileDefault(ConfigFileLgw,IsmBandAlter);
	if	(file)
	{
		RTL_TRDBG(0,"load default '%s'\n",file);
		if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
		{
			RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
			exit(1);
		}
	}
	else
	{
		file	= DoConfigFileDefault(ConfigFileLgw,IsmBand);
		if	(file)
		{
			RTL_TRDBG(0,"load default '%s'\n",file);
			if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
			{
				RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
				exit(1);
			}
		}
	}

	file	= DoConfigFileCustom(ConfigFileLowLvLgw);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
		if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
		{
			RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
			exit(1);
		}
	}


#if defined(WIRMAMS) || defined(REF_DESIGN_V2)
	file	= DoConfigFileCustom(ConfigFileLgwCustom);
#else
	file	= DoConfigFileCustom(ConfigFileLgw);
#endif
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	file	= DoConfigFileCustom(ConfigFileDynCalib);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
		if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
		{
			RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
			exit(1);
		}
	}

	rtl_htblDump(HtVarLgw,CBHtDumpLgw);
}

static	void	LoadConfigChannel(int hot)
{
	int	err;
	char	*file;
	char	*pt;
	u_int	vers;

#if	0
	if	(RfRegionId && *RfRegionId)
#endif
	// here variable RfRegionId is not yet affected, but HtVarLgw is loaded
	// => test the key/ini instead of the variable
	pt	= CfgStr(HtVarLgw,"gen",-1,"rfregionid","");
	vers		= CfgInt(HtVarLgw,"gen",-1,"rfregionidvers",0);
	if	(pt && *pt)
	{
		RTL_TRDBG(0,"rfregionid='%s.%u' dont use default channels cfg\n",
			pt,vers);
		goto	onlycustom;
	}

	file	= DoConfigFileDefault(ConfigFileChannel,IsmBandAlter);
	if	(file)
	{
		RTL_TRDBG(0,"load default '%s'\n",file);
		if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
		{
			RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
			exit(1);
		}
	}
	else
	{
		file	= DoConfigFileDefault(ConfigFileChannel,IsmBand);
		if	(file)
		{
			RTL_TRDBG(0,"load default '%s'\n",file);
			if	((err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
			{
				RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
				exit(1);
			}
		}
	}
onlycustom:
	file	= DoConfigFileCustom(ConfigFileChannel);
	if	(file)
	{
		RTL_TRDBG(0,"load custom '%s'\n",file);
	}
	if	(file && (err=rtl_iniParse(file,CfgCBIniLoad,HtVarLgw)) < 0)
	{
		RTL_TRDBG(0,"parse '%s' error=%d\n",file,err);
		exit(1);
	}

	rtl_htblDump(HtVarLgw,CBHtDumpLgw);

}

static	void	ServiceStatusResponse()
{
	FILE	*f;
	char	tmp[256];

	f	= fopen(SERVICE_STATUS_FILE,"w");
	if	(f)
	{
		sprintf	(tmp,"STATUS=RUNNING+SIG+IMSG");
		fprintf	(f,"%s",tmp);
		fclose	(f);
		RTL_TRDBG(0,"Service status '%s'\n",tmp);
	}
}

static	void	ServiceStatus(int sig)
{
	FILE	*f;
	char	tmp[256];

	// as lgw thread can have lock a mutex to write traces
	// do not write trace

//	signal	(SIGUSR1,SIG_IGN);
//	RTL_TRDBG(0,"Service status sig=%d\n",sig);

	f	= fopen(SERVICE_STATUS_FILE,"w");
	if	(f)
	{
		sprintf	(tmp,"STATUS=RUNNING+SIG");
		fprintf	(f,"%s",tmp);
		fclose	(f);
//		RTL_TRDBG(0,"Service status '%s'\n",tmp);
	}
	rtl_imsgAdd(MainQ,
		rtl_imsgAlloc(IM_DEF,IM_SERVICE_STATUS_RQST,NULL,0));

//	signal	(SIGUSR1,ServiceStatus);
}

static	void	ServiceStop(int sig)
{

	// as lgw thread can have lock a mutex to write traces
	// do not write trace while lgw thread is not dead

	rtl_traceunlock();
	ServiceStopped	= 1;
	CancelLgwThread();
#ifdef WITH_GPS
	CancelGpsThread();
#endif
	RTL_TRDBG(0,"service stopping sig=%d ...\n",sig);

#ifndef	WITH_TTY
	if	(LgwStarted())
		LgwStop();
#endif

#ifdef WITH_GPS
	if (GpsStarted());
		GpsStop();
#endif

	RTL_TRDBG(0,"LgwNbPacketSend=%u\n",LgwNbPacketSend);
	RTL_TRDBG(0,"LgwNbPacketWait=%u\n",LgwNbPacketWait);
	RTL_TRDBG(0,"LgwNbPacketRecv=%u\n",LgwNbPacketRecv);

	RTL_TRDBG(0,"LgwNbStartOk=%u\n",LgwNbStartOk);
	RTL_TRDBG(0,"LgwNbStartFailure=%u\n",LgwNbStartFailure);
	RTL_TRDBG(0,"LgwNbConfigFailure=%u\n",LgwNbConfigFailure);
	RTL_TRDBG(0,"LgwNbLinkDown=%u\n",LgwNbLinkDown);


	RTL_TRDBG(0,"LgwNbBusySend=%u\n",LgwNbBusySend);
	RTL_TRDBG(0,"LgwNbSyncError=%u\n",LgwNbSyncError);
	RTL_TRDBG(0,"LgwNbCrcError=%u\n",LgwNbCrcError);
	RTL_TRDBG(0,"LgwNbSizeError=%u\n",LgwNbSizeError);
	RTL_TRDBG(0,"LgwNbChanUpError=%u\n",LgwNbChanUpError);
	RTL_TRDBG(0,"LgwNbChanDownError=%u\n",LgwNbChanDownError);
	RTL_TRDBG(0,"LgwNbDelayError=%u\n",LgwNbDelayError);
	RTL_TRDBG(0,"LgwNbDelayReport=%u\n",LgwNbDelayReport);
	RTL_TRDBG(0,"MacNbFcsError=%u\n",MacNbFcsError);
	RTL_TRDBG(0,"LrcNbPktDrop=%u\n",LrcNbPktDrop);
	RTL_TRDBG(0,"LrcNbDisc=%u\n",LrcNbDisc);

	rtl_htblDestroy(HtVarLrr);	
	rtl_htblDestroy(HtVarLgw);	

	sleep(1);
	exit(0);
}

static	char	*DoFilePid()
{
	static	char	file[128];

	sprintf(file,"/var/run/%s.pid",SERVICE_NAME);
	return	file;
}

static	void	ServiceWritePid()
{
	FILE	*f;

	f	= fopen(DoFilePid(),"w");
	if	(f)
	{
		fprintf(f,"%d\n",getpid());
		fclose(f);
	}
}


#ifdef WITH_GPS
static void StartGpsThread()
{
	if (GpsThreadStarted) {
		RTL_TRDBG(0, "gps thread already started\n");
		return;
	}
	if (pthread_attr_init(&GpsThreadAt)) {
		RTL_TRDBG(0, "cannot init gps thread attr err=%s\n", STRERRNO);
		exit(1);
	}

	if (pthread_create(&GpsThread, &GpsThreadAt, GpsRun, NULL)) {
		RTL_TRDBG(0, "cannot create gps thread err=%s\n", STRERRNO);
		exit(1);
	}
	GpsThreadStarted = 1;
	RTL_TRDBG(0, "gps thread is started\n");

}
#endif /* WITH_GPS */

#ifdef WITH_GPS
static void CancelGpsThread()
{
	if (GpsThreadStarted)
	{
		void * res;
		GpsThreadStarted = 0;
		pthread_cancel(GpsThread);
		pthread_join(GpsThread, &res);
		if (res == PTHREAD_CANCELED) {
			RTL_TRDBG(0, "gps thread was canceled\n");
		}
		else {
			RTL_TRDBG(0, "gps thread was not canceled !!! (res=%d)\n", res);
		}
	}
	else
	{
		RTL_TRDBG(0, "gps thread already canceled\n");
	}
}
#endif /* WITH_GPS */

#ifdef WITH_GPS
void ReStartGpsThread()
{
	CancelGpsThread();
	GpsStop();
	GpsThreadStopped = 0;
	StartGpsThread();
}
#endif /* WITH_GPS */

static	void	StartLgwThread()
{
	if	(LgwThreadStarted)
	{
		RTL_TRDBG(0,"lgw thread already started\n");
		return;
	}
	if	(pthread_attr_init(&LgwThreadAt))
	{
		RTL_TRDBG(0,"cannot init thread attr err=%s\n",STRERRNO);
		exit(1);
	}

	if	(pthread_create(&LgwThread,&LgwThreadAt,LgwRun,NULL))
	{
		RTL_TRDBG(0,"cannot create thread err=%s\n",STRERRNO);
		exit(1);
	}
	LgwThreadStarted	= 1;
	RTL_TRDBG(0,"lgw thread is started\n");
}

static	void	CancelLgwThread()
{
	if	(LgwThreadStarted)
	{
		void	*res;
		LgwThreadStarted	= 0;
#if	0
		// as lgw thread can have lock a mutex to write traces
		// do not write trace while lgw thread is not dead
		RTL_TRDBG(0,"sending cancel to lgw thread ...\n");
#endif
		pthread_cancel(LgwThread);
		pthread_join(LgwThread,&res);
		if	(res == PTHREAD_CANCELED)
		{
			RTL_TRDBG(0,"lgw thread was canceled\n");
		}
		else
		{
			RTL_TRDBG(0,"lgw thread was not canceled !!!\n");
		}
	}
	else
	{
		RTL_TRDBG(0,"lgw thread already canceled\n");
	}
}

void	ReStartLgwThread()
{
	CancelLgwThread();
	LgwStop();
	rtl_htblReset(HtVarLgw);
	LoadConfigDefine(1,1);
	LoadConfigLgw(1);
	LoadConfigChannel(1);
	LgwGenConfigure(1,1);
	ChannelConfigure(1,1);
	DownRadioStop		= 0;
	LgwThreadStopped	= 0;
	StartLgwThread();
}

#if	0
static	void	AveragePktTrip(u_int delta)
{
	static	int	cumDelta;
	static	u_short	count	= 1;

	if	(delta > LrcMxPktTrip)
		LrcMxPktTrip	= delta;

	cumDelta= cumDelta + delta;
	if	(cumDelta < 0)	// overflow => reset
	{
		LrcMxPktTrip	= 0;
		cumDelta	= delta;
		count		= 1;
	}
	if	(count == 0)	// loop => reset
	{
		LrcMxPktTrip	= 0;
		cumDelta	= delta;
		count	= 1;
	}

	LrcAvPktTrip	= cumDelta / count;
	count++;
}
#endif

#ifdef	LP_TP31
void	SetIndic(t_lrr_pkt *pkt,int delivered,int c1,int c2,int c3)
{
	if	(delivered >= 0)
		pkt->lp_delivered	= delivered;
	if	(c1 >= 0)
		pkt->lp_cause1		= c1;
	if	(c2 >= 0)
		pkt->lp_cause2		= c2;
	if	(c3 >= 0)
		pkt->lp_cause3		= c3;
}

void	SendIndicToLrc(t_lrr_pkt *pkt)
{
	u_char		buff[1024];
	int		szh;
	t_imsg		*msg;
	int		sz;

	if	(!pkt)
		return;
	if	(pkt->lp_beacon)
		return;
	if	(pkt->lp_type != LP_TYPE_LRR_PKT_RADIO)
		return;
	if	(pkt->lp_deveui == 0)
		return;

	if	(pkt->lp_trip > 0xFFFF)
		pkt->lp_trip	= 0xFFFF;

	if	(MainThreadId == pthread_self())
	{
RTL_TRDBG(1,"PKT SEND INDIC deveui=%016llx fcnt=%u deliv=%u causes=(%02x,%02x,%02x) rtt=%u\n",
		pkt->lp_deveui,pkt->lp_fcntdn,
		pkt->lp_delivered,pkt->lp_cause1,pkt->lp_cause2,pkt->lp_cause3,
		pkt->lp_trip);

		szh	 = LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_pkt_sent_indic);
		pkt->lp_szh	= szh;
		pkt->lp_type	= LP_TYPE_LRR_PKT_SENT_INDIC;
		pkt->lp_u.lp_sent_indic.lr_u.lr_indic.lr_delivered	= 
							pkt->lp_delivered;
		pkt->lp_u.lp_sent_indic.lr_u.lr_indic.lr_cause1		= 
							pkt->lp_cause1;
		pkt->lp_u.lp_sent_indic.lr_u.lr_indic.lr_cause2		= 
							pkt->lp_cause2;
		pkt->lp_u.lp_sent_indic.lr_u.lr_indic.lr_cause3		= 
							pkt->lp_cause3;
		pkt->lp_u.lp_sent_indic.lr_u.lr_indic.lr_trip		= 
							pkt->lp_trip;

		memcpy	(buff,pkt,szh);
		SendToLrc(NULL,buff,szh+0);
	}
	else
	{
	RTL_TRDBG(3,"PKT POST INDIC deveui=%016llx fcnt=%u deliv=%u causes=(%02x,%02x,%02x)\n",
		pkt->lp_deveui,pkt->lp_fcntdn,
		pkt->lp_delivered,pkt->lp_cause1,pkt->lp_cause2,pkt->lp_cause3);

		msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_SENT_INDIC,NULL,0);
		if	(!msg)
			return;
		sz	= sizeof(t_lrr_pkt);
		if	( rtl_imsgDupData(msg,pkt,sz) != msg)
		{
			rtl_imsgFree(msg);
			return;
		}
		rtl_imsgAdd(MainQ,msg);
	}
}
#else
void	SetIndic(t_lrr_pkt *pkt,int delivered,int c1,int c2,int c3)
{
	return;
}
void	SendIndicToLrc(t_lrr_pkt *pkt)
{
	return;
}
#endif


//
//	send a downlink packet to the radio thread
//
static	void	SendRadioPacket(t_lrr_pkt *downpkt,u_char *data,int sz)
{
	u_char	buff[1024];
	u_char	crcbuff[1024];
	u16	crc	= 0;

	char	dst[64];
#if	0
	char	src[64];
	int	ret;
#endif
	int	osz;
	int	ack = 0;
	int	m802 = 0;
	int	lrcdelay = 0;
	int	seqnum = 0;
	int	watteco;
	int	majv;
	int	minv;
	int	delay;
	int	ret;
	int	classb;

	ret	= LgwStarted();
	if	(LgwThreadStopped || !LgwThreadStarted || !ret)
	{
		RTL_TRDBG(1,"Radio thread KO start=%d cmdstart=%d cmdstop=%d\n",
			ret,LgwThreadStarted,LgwThreadStopped);
		SetIndic(downpkt,0,
		LP_C1_RADIO_STOP,LP_C2_RADIO_STOP,LP_CB_RADIO_STOP);
		SendIndicToLrc(downpkt);
		return;
	}

	if	(DownRadioStop)
	{
		RTL_TRDBG(1,"Radio stopped in downlink direction\n");
		SetIndic(downpkt,0,
		LP_C1_RADIO_STOP_DN,LP_C2_RADIO_STOP_DN,LP_CB_RADIO_STOP_DN);
		SendIndicToLrc(downpkt);
		return;
	}

	if	(IsmAsymDownlink /* || strcmp(IsmBand,"us915") == 0 */)
	{
		if	(downpkt->lp_channel <= MAXUP_CHANNEL)
		{
			u_int	mindnchan	= (MAXUP_CHANNEL+1); // ie:127
			u_char	downchan;

			downchan = (downpkt->lp_channel%IsmAsymDnModulo);
			downchan += mindnchan + IsmAsymDnOffset;

			RTL_TRDBG(1,"MAC SEND channel conv %d -> %d (us915)\n",
				downpkt->lp_channel,downchan);
			downpkt->lp_channel	= downchan;
		}
	}

	ack	= (downpkt->lp_flag&LP_RADIO_PKT_ACKMAC)==LP_RADIO_PKT_ACKMAC;
	m802	= (downpkt->lp_flag&LP_RADIO_PKT_802154)==LP_RADIO_PKT_802154;
	lrcdelay= (downpkt->lp_flag&LP_RADIO_PKT_DELAYED)==LP_RADIO_PKT_DELAYED;
	classb	= (downpkt->lp_flag&LP_RADIO_PKT_PINGSLOT)==LP_RADIO_PKT_PINGSLOT;

	if	(classb)
	{	
		// retrieve slots parameters
		LgwInitPingSlot(downpkt);
		// set slots parameters
		LgwResetPingSlot(downpkt);
	}

	if	(downpkt->lp_postponed)
	{
		RTL_TRDBG(2,"MAC SEND retrieve a postponed downlink packet\n");
		if	((ret=LgwSendPacket(downpkt,seqnum,m802,ack)) < 0)
		{
			RTL_TRDBG(1,"LgwSendPacket() error ret=%d\n",ret);
			SetIndic(downpkt,0,-1,-1,-1);
			SendIndicToLrc(downpkt);
		}
		return;
	}

	if 	(lrcdelay)	// NFR595
	{
		lrcdelay		= downpkt->lp_delay;
		downpkt->lp_tms		= 0;
		downpkt->lp_trip	= 0;
		downpkt->lp_delay	= 0;
		downpkt->lp_classc	= 1;
		downpkt->lp_classcmc	= 0;
	}

	if	(downpkt->lp_tms && downpkt->lp_lk)
	{
		t_xlap_link	*lk;
		t_lrc_link	*lrc;

		lk		= (t_xlap_link *)downpkt->lp_lk;
		lrc		= (t_lrc_link *)lk->lk_userptr;
		downpkt->lp_trip= ABS(rtl_tmmsmono() - downpkt->lp_tms);
#if	0
		AveragePktTrip(downpkt->lp_trip);
#endif
		RTL_TRDBG(9,"AvDvUiAdd p=%p d=%u t=%u\n",
			&lrc->lrc_avdvtrip,downpkt->lp_trip,Currtime.tv_sec);
		AvDvUiAdd(&lrc->lrc_avdvtrip,downpkt->lp_trip,Currtime.tv_sec);
		AvDvUiAdd(&CmnLrcAvdvTrip,downpkt->lp_trip,Currtime.tv_sec);
	}

	if      ((downpkt->lp_flag & LP_RADIO_PKT_DELAY) != LP_RADIO_PKT_DELAY)
	{
		downpkt->lp_delay	= 0;
		downpkt->lp_classc	= 1;
		downpkt->lp_classcmc	= 0;
	}


	osz	= sz;
	if	(m802 && MacWithFcsDown)
	{
		crc	= crc_ccitt(0,data,sz);
		memcpy	(crcbuff,data,sz);
		crcbuff[sz]	= crc & 0x00ff;
		crcbuff[sz+1]	= crc >> 8;
		sz	= sz + 2;
		data	= crcbuff;
	}

	downpkt->lp_size	= sz;
	downpkt->lp_payload	= (u_char *)malloc(sz);
	if	(!downpkt->lp_payload)
	{
RTL_TRDBG(0,"ERROR alloc payload %d\n",sz);
		return;
	}
	memcpy	(downpkt->lp_payload,data,sz);
	TmoaLrrPacket(downpkt);


	RTL_TRDBG(2,"MAC SEND sz=%d m802=%d ack=%d crcccitt=0x%04x tmoa=%fms\n",
			sz,m802,ack,crc,downpkt->lp_tmoa/1000.0);

	if	(lrcdelay)	// NFR595
	{
		t_imsg	*msg;
		int	sz;
		// replace the message in mainQ
		downpkt->lp_postponed	= lrcdelay;
		RTL_TRDBG(2,"MAC SEND LRC request a delayed downlink => postpone=%d\n",
			lrcdelay);
		sz	= sizeof(t_lrr_pkt);
		msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_POST_DATA,NULL,0);
		if	(!msg)
		{
			return;
		}
		if	( rtl_imsgDupData(msg,downpkt,sz) != msg)
		{
			rtl_imsgFree(msg);
			return;
		}
		rtl_imsgAddDelayed(MainQ,msg,lrcdelay);
		return;
	}
	if	(m802 == 0)
	{
		LoRaMAC_t	mf;
		u_char		*pt;
		u_int64_t	deveui	= 0xFFFFFFFFFFFFFFFF;
		u_short		fcntdn	= 0xFFFF;

		memset	(&mf,0,sizeof(mf));
		LoRaMAC_decodeHeader(data,osz,&mf);
		seqnum	= mf.FCnt;
/*
		rtl_binToStr(mf.DevAddr,sizeof(mf.DevAddr),dst,sizeof(dst)-1);
*/

		pt	= (u_char *)&mf.DevAddr;

		sprintf	(dst,"%02x%02x%02x%02x",*(pt+3),*(pt+2),*(pt+1),*pt);

		watteco	= (downpkt->lp_majorv & 0x80)?1:0;
		majv	= downpkt->lp_majorv & ~0x80;
		minv	= downpkt->lp_minorv;

#ifdef	LP_TP31
		deveui	= downpkt->lp_deveui;
		fcntdn	= downpkt->lp_fcntdn;
#endif

RTL_TRDBG(1,"MACLORA SEND seq=%d ack=%d devaddr=%s w=%d major=%d minor=%d len=%d tmoa=%fms flg='%s' deveui=%016llx/%u\n",
		mf.FCnt,ack,dst,watteco,majv,minv,
		downpkt->lp_size,downpkt->lp_tmoa/1000.0,
		LrrPktFlagsTxt(downpkt->lp_flag),deveui,fcntdn);

		if	(downpkt->lp_classb)
		{
RTL_TRDBG(1,"MACLORA SEND classb window=(%u,%09u) nb=%d slot1=%d slot2=%d\n",
			downpkt->lp_gss,downpkt->lp_gns,downpkt->lp_nbslot,
			downpkt->lp_firstslot,downpkt->lp_firstslot2);
		}
	}
	else
	{

RTL_TRDBG(1,"MAC802 SEND tmoa=%fms flg='%s'\n",
		downpkt->lp_tmoa/1000.0,
		LrrPktFlagsTxt(downpkt->lp_flag));

#if	0
		mac802154_t	mf;
		memset	(&mf,0,sizeof(mf));
		ret	= mac802154_parse(data,sz,&mf);
		if	(ret <= 0)
		{
			rtl_binToStr(data,sz,(char *)buff,sizeof(buff)-10);
RTL_TRDBG(0,"ERROR MAC802 SEND data='%s'\n",buff);
			return;
		}
		seqnum	= mf.seq;
		rtl_binToStr(mf.dest_addr,sizeof(mf.dest_addr),dst,sizeof(dst)-1);
		rtl_binToStr(mf.src_addr,sizeof(mf.src_addr),src,sizeof(src)-1);
RTL_TRDBG(2,"MAC802 SEND seq=%d ack=%d dst=%04x/%s src=%04x/%s len=%d\n",
			mf.seq,ack,
			mf.dest_pid,dst,
			mf.src_pid,src,
			mf.payload_len);

RTL_TRDBG(2,"MAC802 SEND '%s'\n", mac802154_ip6typetxt(mac802154_ip6type(&mf)));
#endif
	}

	static	int	fmargin	= -1;
	if	(fmargin == -1)
	{
		fmargin	= 100;	// by default margin feature was activated
		fmargin	= CfgInt(HtVarLrr,"lrr",-1,"dnmargin",fmargin);
	}
	static	int	margin	= -1;
	if	(margin == -1)
	{
		margin	= fmargin;
		margin	= CfgInt(HtVarLrr,System,-1,"dnmargin",margin);
		if	(margin && margin < 100)
			margin	= 100;
		RTL_TRDBG(1,"down link dnmargin=%d\n",margin);
	}
	static	int	margincpu	= -1;
	if	(margin && margincpu == -1)
	{
		margincpu= 75;
		margincpu= CfgInt(HtVarLrr,System,-1,"dnmargincpu",margincpu);
		if	(margincpu < 75)
			margincpu	= 75;
		if	(margincpu > margin)
			margin	= margincpu;
		RTL_TRDBG(1,"down link dnmargin=%d dnmargincpu=%d\n",
							margin,margincpu);
	}

	delay	= downpkt->lp_delay - downpkt->lp_trip;
	if (/*!downpkt->lp_classb &&*/ margin && downpkt->lp_tms && downpkt->lp_delay && delay > margin)
	{
		t_imsg	*msg;
		int	sz;

		// dont block sx1301 and give "more" chance to small packets
		delay	= delay - margincpu;
		if	(delay < margincpu)	delay	= margincpu;
RTL_TRDBG(1,"MAC SEND trip=%d avtrip=%u dvtrip=%u mxtrip=%u long delay postpone=%d in MainQ\n",
		downpkt->lp_trip,LrcAvPktTrip,LrcDvPktTrip,LrcMxPktTrip,delay);
		// replace the message in mainQ
		downpkt->lp_postponed	= delay;
		sz	= sizeof(t_lrr_pkt);
		msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_POST_DATA,NULL,0);
		if	(!msg)
		{
			return;
		}
		if	( rtl_imsgDupData(msg,downpkt,sz) != msg)
		{
			rtl_imsgFree(msg);
			return;
		}
		rtl_imsgAddDelayed(MainQ,msg,delay);
		return;
	}

RTL_TRDBG(1,"MAC SEND trip=%d avtrip=%u dvtrip=%u mxtrip=%u no postpone delay=%d\n",
		downpkt->lp_trip,LrcAvPktTrip,LrcDvPktTrip,LrcMxPktTrip,delay);

	if	(TraceLevel > 3)
	{
		rtl_binToStr(data,sz,(char *)buff,sizeof(buff)-10);
		RTL_TRDBG(4,"MAC SEND\n>>>'%s'\n",buff);
	}

	if	((ret=LgwSendPacket(downpkt,seqnum,m802,ack)) < 0)
	{
		RTL_TRDBG(1,"LgwSendPacket() error ret=%d\n",ret);
		SetIndic(downpkt,0,-1,-1,-1);
		SendIndicToLrc(downpkt);
	}
}


static	void	SendRadioBeacon(t_lrr_pkt *pktbeacon,u_char *data,int sz)
{
	t_lrr_pkt	downpkt;
	t_lrr_beacon_dn	*beacon	= &(pktbeacon->lp_u.lp_beacon_dn);

	struct	timeval	tv;
	char	when[128];
	int	ret;

	LgwBeaconRequestedCnt++;
	LgwBeaconLastDeliveryCause	= LP_CB_NA;

	ret	= LgwStarted();
	if	(LgwThreadStopped || !LgwThreadStarted || !ret)
	{
		RTL_TRDBG(1,"Radio thread KO start=%d cmdstart=%d cmdstop=%d\n",
			ret,LgwThreadStarted,LgwThreadStopped);
		LgwBeaconLastDeliveryCause	= LP_CB_RADIO_STOP;
		return;
	}

	if	(DownRadioStop)
	{
		RTL_TRDBG(1,"Radio stopped in downlink direction\n");
		LgwBeaconLastDeliveryCause	= LP_CB_RADIO_STOP_DN;
		return;
	}

	memset	(&downpkt,0,sizeof(t_lrr_pkt));
	memcpy	(&downpkt,pktbeacon,LP_PRE_HEADER_PKT_SIZE);

	// as we use ON_GPS mode a beacon can only be sent on PPS
	beacon->be_gns	= 0;

	memset	(&tv,0,sizeof(tv));
	tv.tv_sec		= beacon->be_gss;
	tv.tv_usec		= beacon->be_gns / 1000;
	rtl_gettimeofday_to_iso8601date(&tv,NULL,when);

	if	(LgwBeaconUtcTime.tv_sec == beacon->be_gss)
	{
RTL_TRDBG(1,"MAC SEND beacon already programmed at utc='%s' (%u,%09u) lk=%p\n",
		when,beacon->be_gss,beacon->be_gns,pktbeacon->lp_lk);
		LgwBeaconRequestedDupCnt++;
		return;
	}

RTL_TRDBG(1,"MAC SEND beacon to send at utc='%s' (%u,%09u) lk=%p\n",
		when,beacon->be_gss,beacon->be_gns,pktbeacon->lp_lk);


	LgwBeaconUtcTime.tv_sec	= beacon->be_gss;
	LgwBeaconUtcTime.tv_nsec= beacon->be_gns;

#ifndef TEKTELIC	// No need for tektelic, use directly utc time
	if	(!UseGpsTime)
	{
	RTL_TRDBG(1,"beacon packets need use of GPS time (usegpstime=1)\n");
		return;
	}
	if	(!Gps_ref_valid || GpsFd < 0)
	{
	RTL_TRDBG(1,"beacon packets need correct GPS time synchro(%d) fd=%d\n",
			Gps_ref_valid,GpsFd);
		return;
	}
#endif

	downpkt.lp_beacon	= 1;
	downpkt.lp_type		= LP_TYPE_LRR_PKT_RADIO;
	downpkt.lp_flag		= LP_RADIO_PKT_DOWN;
	downpkt.lp_gss		= beacon->be_gss;
	downpkt.lp_gns		= beacon->be_gns;
	downpkt.lp_tms		= rtl_tmmsmono();
	downpkt.lp_trip		= 0;
	downpkt.lp_delay	= 0;
	downpkt.lp_lgwdelay	= 1;
	downpkt.lp_lk		= pktbeacon->lp_lk;

	downpkt.lp_chain	= beacon->be_chain;
	downpkt.lp_channel	= beacon->be_channel;
	downpkt.lp_spfact	= beacon->be_spfact;
	if	(Rx2Channel && downpkt.lp_channel == UNK_CHANNEL)
	{
		downpkt.lp_channel	= Rx2Channel->channel;
		downpkt.lp_subband	= Rx2Channel->subband;
		downpkt.lp_spfact	= 
				CodeSpreadingFactor(Rx2Channel->dataraterx2);
	}
	downpkt.lp_correct	= CodeCorrectingCode(CR_LORA_4_5);

	downpkt.lp_size		= pktbeacon->lp_size;
	downpkt.lp_payload	= (u_char *)malloc(downpkt.lp_size);
	if	(!downpkt.lp_payload)
	{
RTL_TRDBG(0,"ERROR alloc payload %d\n",downpkt.lp_size);
		return;
	}
	memcpy	(downpkt.lp_payload,data,downpkt.lp_size);
	TmoaLrrPacket(&downpkt);

RTL_TRDBG(1,"MAC SEND beacon sz=%d tmoa=%fms\n",downpkt.lp_size,downpkt.lp_tmoa/1000.0);

	LgwSendPacket(&downpkt,0,0,0);
}

static	void	SendRadioClassCMultiCast(t_lrr_pkt *pktmulti,u_char *data,int sz)
{
	t_lrr_pkt		downpkt;
	t_lrr_multicast_dn	*multi	= &(pktmulti->lp_u.lp_multicast_dn);

	struct	timeval	tv;
	char	when[128];
	int	ret;

	LgwClassCRequestedCnt++;
	LgwClassCLastDeliveryCause	= LP_CB_NA;

	ret	= LgwStarted();
	if	(LgwThreadStopped || !LgwThreadStarted || !ret)
	{
		RTL_TRDBG(1,"Radio thread KO start=%d cmdstart=%d cmdstop=%d\n",
			ret,LgwThreadStarted,LgwThreadStopped);
		LgwClassCLastDeliveryCause	= LP_CB_RADIO_STOP;
		return;
	}

	if	(DownRadioStop)
	{
		RTL_TRDBG(1,"Radio stopped in downlink direction\n");
		LgwClassCLastDeliveryCause	= LP_CB_RADIO_STOP_DN;
		return;
	}

	memset	(&downpkt,0,sizeof(t_lrr_pkt));
	memcpy	(&downpkt,pktmulti,LP_PRE_HEADER_PKT_SIZE);

	// as we use ON_GPS mode a beacon can only be sent on PPS
	multi->mc_gns	= 0;

	memset	(&tv,0,sizeof(tv));
	tv.tv_sec		= multi->mc_gss;
	tv.tv_usec		= multi->mc_gns / 1000;
	rtl_gettimeofday_to_iso8601date(&tv,NULL,when);

	if	(LgwClassCUtcTime.tv_sec == multi->mc_gss)
	{
RTL_TRDBG(1,"MAC SEND classcmc already programmed at utc='%s' (%u,%09u) lk=%p\n",
		when,multi->mc_gss,multi->mc_gns,pktmulti->lp_lk);
		LgwClassCRequestedDupCnt++;
		return;
	}

RTL_TRDBG(1,"MAC SEND classcmc to send at utc='%s' (%u,%09u) lk=%p\n",
		when,multi->mc_gss,multi->mc_gns,pktmulti->lp_lk);


	LgwClassCUtcTime.tv_sec	= multi->mc_gss;
	LgwClassCUtcTime.tv_nsec= multi->mc_gns;

#ifndef TEKTELIC	// No need for tektelic, use directly utc time
	if	(!UseGpsTime)
	{
	RTL_TRDBG(1,"Classcmc packets need use of GPS time (usegpstime=1)\n");
		return;
	}
	if	(!Gps_ref_valid || GpsFd < 0)
	{
	RTL_TRDBG(1,"Classcmc packets need correct GPS time synchro(%d) fd=%d\n",
			Gps_ref_valid,GpsFd);
		return;
	}
#endif

	downpkt.lp_classc	= 0;
	downpkt.lp_classcmc	= 1;
	downpkt.lp_type		= LP_TYPE_LRR_PKT_RADIO;
	downpkt.lp_flag		= LP_RADIO_PKT_DOWN;
	downpkt.lp_gss		= multi->mc_gss;
	downpkt.lp_gns		= multi->mc_gns;
	downpkt.lp_tms		= rtl_tmmsmono();
	downpkt.lp_trip		= 0;
	downpkt.lp_delay	= 0;
	downpkt.lp_lgwdelay	= 1;
	downpkt.lp_lk		= pktmulti->lp_lk;

	downpkt.lp_deveui	= multi->mc_deveui;
	downpkt.lp_fcntdn	= multi->mc_fcntdn;

	downpkt.lp_chain	= multi->mc_chain;
	downpkt.lp_channel	= multi->mc_channel;
	downpkt.lp_spfact	= multi->mc_spfact;
	if	(Rx2Channel && downpkt.lp_channel == UNK_CHANNEL)
	{
		downpkt.lp_channel	= Rx2Channel->channel;
		downpkt.lp_subband	= Rx2Channel->subband;
		downpkt.lp_spfact	= 
				CodeSpreadingFactor(Rx2Channel->dataraterx2);
	}
	downpkt.lp_correct	= CodeCorrectingCode(CR_LORA_4_5);

	downpkt.lp_size		= pktmulti->lp_size;
	downpkt.lp_payload	= (u_char *)malloc(downpkt.lp_size);
	if	(!downpkt.lp_payload)
	{
RTL_TRDBG(0,"ERROR alloc payload %d\n",downpkt.lp_size);
		return;
	}
	memcpy	(downpkt.lp_payload,data,downpkt.lp_size);
	TmoaLrrPacket(&downpkt);

RTL_TRDBG(1,"MAC SEND classcmc sz=%d tmoa=%fms\n",downpkt.lp_size,downpkt.lp_tmoa/1000.0);

	LgwSendPacket(&downpkt,0,0,0);
}


static	void	RecvInfraPacket(t_lrr_pkt *downpkt)
{
	char	version[256];
	char	cmd[1024];
	char	traceupgrade[1024];
	int	masterid;

	switch	(downpkt->lp_type)
	{
	case	LP_TYPE_LRR_INF_UPGRADE:
	{
		t_lrr_upgrade	*upgrade;
		char		*md5;

		upgrade	= &(downpkt->lp_u.lp_upgrade);
		sprintf	(version,"%u.%u.%u",
		upgrade->up_versMaj,upgrade->up_versMin,upgrade->up_versRel);
		RTL_TRDBG(1,"upgade requested version='%s' md5='%s'\n",
						version, upgrade->up_md5);
		md5	= (char *)upgrade->up_md5;

		sprintf	(traceupgrade,"%s/var/log/lrr/UPGRADE.log",RootAct);
		if	(strlen(md5) && strcmp(md5,"*"))
		{
		sprintf	(cmd,"sh -c './upgrade.sh -V %s -M %s > %s 2>&1 &'",
			version,upgrade->up_md5,traceupgrade);
		}
		else
		{
		sprintf	(cmd,"sh -c './upgrade.sh -V %s > %s 2>&1 &'",
			version,traceupgrade);
		}
		DoSystemCmdBackGround(cmd);
		RTL_TRDBG(1,"%s\n",cmd);
	}
	break;

	case	LP_TYPE_LRR_INF_UPGRADE_V1:	// TODO a finir
	{
		t_lrr_upgrade_v1	*upgrade;
		char			*md5;

		upgrade	= &(downpkt->lp_u.lp_upgrade_v1);
		sprintf	(version,"%u.%u.%u",
		upgrade->up_versMaj,upgrade->up_versMin,upgrade->up_versRel);
		RTL_TRDBG(1,"upgade requested version='%s' md5='%s'\n",
						version, upgrade->up_md5);
		md5	= (char *)upgrade->up_md5;

		sprintf	(traceupgrade,"%s/var/log/lrr/UPGRADE.log",RootAct);
		if	(strlen(md5) && strcmp(md5,"*"))
		{
		sprintf	(cmd,"sh -c './upgrade.sh -V %s -M %s > %s 2>&1 &'",
			version,upgrade->up_md5,traceupgrade);
		}
		else
		{
		sprintf	(cmd,"sh -c './upgrade.sh -V %s > %s 2>&1 &'",
			version,traceupgrade);
		}
		DoSystemCmdBackGround(cmd);
		RTL_TRDBG(1,"%s\n",cmd);
	}
	break;

	case	LP_TYPE_LRR_INF_UPGRADE_CMD:
	{
		t_lrr_upgrade_cmd	*upgrade;

		upgrade	= &(downpkt->lp_u.lp_upgrade_cmd);
		RTL_TRDBG(1,"upgade requested with cmd ...\n");

		sprintf	(traceupgrade,"%s/var/log/lrr/UPGRADE.log",RootAct);
		sprintf	(cmd,"sh -c './upgrade.sh %s > %s 2>&1 &'",
			upgrade->up_cmd,traceupgrade);
		DoSystemCmdBackGround(cmd);
		RTL_TRDBG(1,"%s\n",cmd);
	}
	break;

	case	LP_TYPE_LRR_INF_RESTART_CMD:
	{
		RTL_TRDBG(1,"restartrequested with cmd ...\n");
		exit(0);
	}
	break;

	case	LP_TYPE_LRR_INF_SHELL_CMD:
	{
		DoShellCommand(downpkt);
	}
	break;

	//	start / stop radio without saving configuration flags
	case	LP_TYPE_LRR_INF_RADIO_STOP_CMD:
	{
		t_imsg	*msg;

		msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_EXIT,NULL,0);
		if	(!msg)
			break;
		rtl_imsgAdd(LgwQ,msg);
		LgwThreadStopped	= 1;
	}
	break;

	case	LP_TYPE_LRR_INF_RADIO_START_CMD:
	{
		LgwThreadStopped	= 0;
		DownRadioStop		= 0;
	}
	break;

	case	LP_TYPE_LRR_INF_DNRADIO_STOP_CMD:
	{
		DownRadioStop	= 1;
	}
	break;
	case	LP_TYPE_LRR_INF_DNRADIO_START_CMD:
	{
		DownRadioStop	= 0;
	}
	break;
	case	LP_TYPE_LRR_PKT_DTC_SYNCHRO:
	{
#ifdef	LP_TP31
		SendDtcSyncToLrc(downpkt->lp_lk);
#endif
	}
	break;

	case	LP_TYPE_LRR_INF_TWA_LRRID:
	{
		RTL_TRDBG(1,"get lrrid=%08x from twa lrc=%d\n",
					downpkt->lp_twalrrid,downpkt->lp_lrxid);
		if	(LrrIDGetFromTwa == 0)
		{ // The response from the first lrc has already been received,
		  // this is the response from the second lrc
		  // just check if the second lrrid received is the same than 
		  // the first one
			if (downpkt->lp_twalrrid != LrrID)
			{
RTL_TRDBG(0,"ERROR: received 2 differents LrrID from TWA (%08x!=%08x), big problems expected !\n",
				LrrID, downpkt->lp_twalrrid);
				break;
			}
	
		}
		if	(downpkt->lp_twalrruidns == 1)
		{ // The LRC understand the command but say it does not to
		  // support this request at all => use old mode
			LrrIDGetFromTwa	= 0;
			LrrIDFromTwa	= 0;
			SendLrrID(downpkt->lp_lk);
			break;
		}
		if	(downpkt->lp_twalrrid == 0)
		{ // The LRC understand the command but has no response
		  // and ask to do not use old mode, retry later
			LrrIDFromTwa	= 0;
RTL_TRDBG(0,"ERROR: received LrrID==0 from TWA ! => retry later\n");
			ServiceStop(0);
			break;
		}
		LrrID		= downpkt->lp_twalrrid;
		LrrIDFromTwa	= LrrID;
		LrrIDGetFromTwa = 0;	// LrrID received => disable the feature
		SaveConfigFileState();
		SendLrrID(downpkt->lp_lk);
	}
	break;

	// NFR997
	case	LP_TYPE_LRR_INF_PARTITIONID:
	{
		masterid = (u_int)CfgInt(HtVarLrr, "lrr", -1, "masterid", 0);
		RTL_TRDBG(1,"partitionid=%d for lrcid=%d (masterid=%d)\n",
					downpkt->lp_partitionid, downpkt->lp_lrxid, masterid);
		if	(LrrIDGetFromBS == 0)
		{
			RTL_TRDBG(0,"WARNING: received partitionid from lrc but NFR997 not activated\n");
			break;
		}

		// check if partitionid correspond to master id
		if (masterid == downpkt->lp_partitionid)
		{
			int	i;
			// search index of lrc
			RTL_TRDBG(3, "search lrc index ...\n");
			for	(i = 0 ; i < NbLrc ; i++)
			{
				if	(&TbLapLrc[i] == downpkt->lp_lk)
				{
					MasterLrc = i;
					RTL_TRDBG(1, "found master lrc: lrc %d (lrcid=%d)\n",
						MasterLrc, downpkt->lp_lrxid);
				}
			}
		}
		RTL_TRDBG(0, "master lrc is lrc %d\n", MasterLrc);
	}
	break;

	default :
	break;
	}
}

static	void	TcpKeepAliveHigh(int lrc,int fd)
{
	int	tcpKeepAlive	= 1;	// yes or not

	int	tcpKeepIdle	= 5;	// 5s
	int	tcpKeepIntvl	= 5;	// 5s
	int	tcpKeepCnt	= 20;	// 20 retries


	if	(fd < 0)
		return;


	tcpKeepAlive	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepalive",tcpKeepAlive);
	tcpKeepIdle	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepidle",tcpKeepIdle);
	tcpKeepIntvl	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepintvl",tcpKeepIntvl);
	tcpKeepCnt	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepcnt",tcpKeepCnt);

	if	(tcpKeepAlive <= 0)
		return;

RTL_TRDBG(1,"LAP LRC TCP KEEPALIVE HIGH lrc=%d fd=%d alive=%d idle=%d intvl=%d cnt=%d\n",
		lrc,fd,tcpKeepAlive,tcpKeepIdle,tcpKeepIntvl,tcpKeepCnt);

	setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,
				(char *)&tcpKeepAlive,sizeof(tcpKeepAlive));
	if	( tcpKeepIdle > 0 )
	{
		setsockopt(fd,IPPROTO_TCP,TCP_KEEPIDLE,
			(char *)&tcpKeepIdle,sizeof(tcpKeepIdle));
	}
	if	( tcpKeepIntvl > 0 )
	{
		setsockopt(fd,IPPROTO_TCP,TCP_KEEPINTVL,
			(char *)&tcpKeepIntvl,sizeof(tcpKeepIntvl));
	}
	if	( tcpKeepCnt > 0 )
	{
		setsockopt(fd,IPPROTO_TCP,TCP_KEEPCNT,
			(char *)&tcpKeepCnt,sizeof(tcpKeepCnt));
	}
}

static	void	TcpKeepAliveLow(int lrc,int fd)
{
	int	tcpKeepAlive	= 1;	// yes or not

	int	tcpKeepIdle	= 5;	// 5s
	int	tcpKeepIntvl	= 30;	// 30s
	int	tcpKeepCnt	= 3;	// 3 retries

	if	(fd < 0)
		return;



	tcpKeepAlive	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepalivelow",tcpKeepAlive);
	tcpKeepIdle	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepidlelow",tcpKeepIdle);
	tcpKeepIntvl	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepintvllow",tcpKeepIntvl);
	tcpKeepCnt	= (u_int)CfgInt(HtVarLrr,"tcp",-1,
					"tcpkeepcntlow",tcpKeepCnt);

	if	(tcpKeepAlive <= 0)
		return;

RTL_TRDBG(1,"LAP LRC TCP KEEPALIVE LOW lrc=%d fd=%d alive=%d idle=%d intvl=%d cnt=%d\n",
		lrc,fd,tcpKeepAlive,tcpKeepIdle,tcpKeepIntvl,tcpKeepCnt);

	setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,
				(char *)&tcpKeepAlive,sizeof(tcpKeepAlive));
	if	( tcpKeepIdle > 0 )
	{
		setsockopt(fd,IPPROTO_TCP,TCP_KEEPIDLE,
			(char *)&tcpKeepIdle,sizeof(tcpKeepIdle));
	}
	if	( tcpKeepIntvl > 0 )
	{
		setsockopt(fd,IPPROTO_TCP,TCP_KEEPINTVL,
			(char *)&tcpKeepIntvl,sizeof(tcpKeepIntvl));
	}
	if	( tcpKeepCnt > 0 )
	{
		setsockopt(fd,IPPROTO_TCP,TCP_KEEPCNT,
			(char *)&tcpKeepCnt,sizeof(tcpKeepCnt));
	}
}

void	TcpKeepAliveNo(int lrc,int fd)
{
	int	tcpKeepAlive	= 0;	// yes or not

RTL_TRDBG(1,"LAP LRC TCP NOKEEPALIVE lrc=%d fd=%d alive=%d\n",
		lrc,fd,tcpKeepAlive);

	if	(fd < 0)
		return;


	setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,
				(char *)&tcpKeepAlive,sizeof(tcpKeepAlive));
}

void	SendCapabToLrc(t_xlap_link *lktarget)
{
	t_xlap_link	*lk;
	t_lrr_pkt	uppkt;
	int		i;

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_lrrid	= LrrID;
	uppkt.lp_type	= LP_TYPE_LRR_INF_CAPAB;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_capab);
	uppkt.lp_u.lp_capab.li_nbantenna	= LgwAntenna;
	uppkt.lp_u.lp_capab.li_versMaj		= VersionMaj;
	uppkt.lp_u.lp_capab.li_versMin		= VersionMin;
	uppkt.lp_u.lp_capab.li_versRel		= VersionRel;
	uppkt.lp_u.lp_capab.li_versFix		= VersionFix;
	strncpy((char *)uppkt.lp_u.lp_capab.li_ismBand,IsmBand,9);
	strncpy((char *)uppkt.lp_u.lp_capab.li_system,System,32);
	strncpy((char *)uppkt.lp_u.lp_capab.li_ismBandAlter,IsmBandAlter,32);
	if	(RfRegionId && *RfRegionId)
		strncpy((char *)uppkt.lp_u.lp_capab.li_rfRegion,RfRegionId,32);
	uppkt.lp_u.lp_capab.li_ismVar		= RfRegionIdVers;
#ifdef	LP_TP31
	uppkt.lp_u.lp_capab.li_szPktRadioStruct	= sizeof(uppkt.lp_u.lp_radio);
	uppkt.lp_u.lp_capab.li_nbBoard		= LgwBoard;
	uppkt.lp_u.lp_capab.li_nbChan		= 8;
	uppkt.lp_u.lp_capab.li_nbSector		= LgwAntenna;	// TODO
	uppkt.lp_u.lp_capab.li_fpga		= 0;		// TODO
	uppkt.lp_u.lp_capab.li_nbChanUp		= 0;		// TODO
	uppkt.lp_u.lp_capab.li_nbChanDn		= 0;		// TODO
	uppkt.lp_u.lp_capab.li_iecExtFtr	= 1;
	uppkt.lp_u.lp_capab.li_dtcdnFtr		= 1;
	uppkt.lp_u.lp_capab.li_dtcupFtr		= 1;
	uppkt.lp_u.lp_capab.li_sentIndicFtr	= 1;
	uppkt.lp_u.lp_capab.li_pktStoreFtr	= StorePktCount;
	uppkt.lp_u.lp_capab.li_geoLocFtr	= 0;
	uppkt.lp_u.lp_capab.li_lbtFtr		= LgwLbtEnable;
#ifdef	WITH_GPS
	uppkt.lp_u.lp_capab.li_classBFtr	= 1;
	uppkt.lp_u.lp_capab.li_classCMcFtr	= 1;
#endif
	uppkt.lp_u.lp_capab.li_freqdnFtr	= 1;
#endif

#ifndef	WITH_LBT
	uppkt.lp_u.lp_capab.li_lbtFtr		= 0;
#endif


	if	(lktarget)
	{
		LapPutOutQueue(lktarget,(u_char *)&uppkt,uppkt.lp_szh);
		return;
	}

	for	(i = 0 ; i < NbLrc ; i++)
	{
		lk	= &TbLapLrc[i];
		if	(lk->lk_state == SSP_STARTED)
		{
			LapPutOutQueue(lk,(u_char *)&uppkt,uppkt.lp_szh);
		}
	}
}

#ifdef	LP_TP31
static	void	SendDtcSyncToLrc(t_xlap_link *lktarget)
{
	t_xlap_link	*lk;
	t_lrr_pkt	uppkt;
	int		i;

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_flag	= LP_RADIO_PKT_DTC;
	uppkt.lp_lrrid	= LrrID;
	uppkt.lp_type	= LP_TYPE_LRR_PKT_DTC_SYNCHRO;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_pkt_radio);

	inline	void dtcsync(void *pf,int ant,int c,int s,
                        float up,float dn,float upsub,float dnsub)
	{
		t_lrr_pkt_radio_dtc_ud	*dtc;

RTL_TRDBG(1,"DTC synchro a=%d c=%03d s=%03d uch=%f dch=%f usub=%f dsub=%f\n",
			ant,c,s,up,dn,upsub,dnsub);

		uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_pkt_radio);
		uppkt.lp_size	= 0;

		uppkt.lp_chain	= ant<<4;
		uppkt.lp_channel= c;
		uppkt.lp_subband= s;

		dtc	= &uppkt.lp_u.lp_radio.lr_u2.lr_dtc_ud;
		dtc->lr_dtcchanneldn	= dn;
		dtc->lr_dtcsubbanddn	= dnsub;
		dtc->lr_dtcchannelup	= up;
		dtc->lr_dtcsubbandup	= upsub;

		if	(lktarget)
		{
			LapPutOutQueue(lktarget,(u_char *)&uppkt,uppkt.lp_szh);
			return;
		}

		for	(i = 0 ; i < NbLrc ; i++)
		{
			lk	= &TbLapLrc[i];
			if	(lk->lk_state == SSP_STARTED)
			{
				LapPutOutQueue(lk,(u_char *)&uppkt,uppkt.lp_szh);
			}
		}
	}

	DcWalkChanCounters(NULL,dtcsync);

	// send an invalid DTC synchro packet to signal end of synchro
	
	uppkt.lp_flag	= 0;
	uppkt.lp_type	= LP_TYPE_LRR_PKT_DTC_SYNCHRO;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_pkt_radio);
	dtcsync(NULL,0,0,0,0.0,0.0,0.0,0.0);
}
#endif

static	void	LapEventProceed(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
	t_lrc_link	*lrc;

	lrc	= lk->lk_userptr;
	RTL_TRDBG(6,"LAP CB (%s,%s) st='%s' receive evttype=%d evtnum='%s'\n",
		lk->lk_addr,lk->lk_port,
		LapStateTxt(lk->lk_state),evttype,LapEventTxt(evtnum));

	LapState	= lk->lk_state;

	if	(LrrID == 0)	// sniffer
		return;

	switch	(evtnum)
	{
	case	EVT_LK_CREATED :	// tcp server only
	case	EVT_TCP_CONNECTED :
		RTL_TRDBG(1,"LAP LRC CNX\n");
		TcpKeepAliveHigh(-1,lk->lk_fd);
		DoHtmlFile();
#ifdef WIRMANA
		LedBackhaul(1);
#endif
	break;
	case	EVT_LK_DELETED :
		RTL_TRDBG(1,"LAP LRC DISC (%d)\n",lrc->lrc_stat.lrc_nbdisc);
		if	(lk->lk_state != SSP_INIT)
			lrc->lrc_stat.lrc_nbdisc++;
		DoHtmlFile();
		memset	(&lrc->lrc_stat_p,0,sizeof(lrc->lrc_stat_p));
#ifdef WIRMANA
		LedBackhaul(1);
#endif
	break;
	case	EVT_LK_STOPPED :
		RTL_TRDBG(1,"LAP LRC STOPPED\n");
		DoHtmlFile();
#ifdef WIRMANA
		LedBackhaul(1);
#endif
	break;
	case	EVT_LK_STARTED :
	{
#ifdef WIRMANA
		LedBackhaul(2);
#endif
		RTL_TRDBG(1,"LAP LRC STARTED\n");

		// if NFR684 activated
		RTL_TRDBG(1,"Get LrrID: from bootserver = %d, from twa = %d, lrrid=%08x\n", 
				LrrIDGetFromBS, LrrIDGetFromTwa, LrrIDFromTwa);
		if (!LrrIDGetFromBS && LrrIDGetFromTwa)
			SendLrrUID(lk);
		else
			SendLrrID(lk);
	}
	break;
	case	EVT_FRAMEI :
	{
		t_lrr_pkt	downpkt;
		int		szh	= LP_PRE_HEADER_PKT_SIZE;
		u_char		*frame	= data;

		if	(sz <= 0)	// ack forced by peer with frameI sz=0
			break;

		memset	(&downpkt,0,sizeof(t_lrr_pkt));
		memcpy	(&downpkt,data,szh);
		if	(downpkt.lp_vers != LP_LRX_VERSION)
		{
			RTL_TRDBG(0,"bad version %d/%d\n",downpkt.lp_vers,
						LP_LRX_VERSION);
#if	0		// TODO
			break;
#endif
		}
		szh	= downpkt.lp_szh;
		if	(szh > sizeof(t_lrr_pkt))
			szh	= sizeof(t_lrr_pkt);
		memcpy	(&downpkt,data,szh);
		downpkt.lp_lk		= lk;
		downpkt.lp_size		= sz - szh;
		downpkt.lp_payload	= frame + szh;

RTL_TRDBG(2,"LAP RECV sz=%d lrrid=%08x typ=%d/%d(%s) tms=%u rqtdelay=%u\n",
	sz,downpkt.lp_lrrid,downpkt.lp_type,
	downpkt.lp_flag,LrrPktFlagsTxt(downpkt.lp_flag),
	downpkt.lp_tms,downpkt.lp_delay);

		if	((downpkt.lp_flag&LP_INFRA_PKT_INFRA))
		{
			RecvInfraPacket(&downpkt);
			break;
		}
		switch	(downpkt.lp_type)
		{
		case	LP_TYPE_LRR_PKT_RADIO:
			SendRadioPacket(&downpkt,frame + szh,sz - szh);
		break;
		case	LP_TYPE_LRR_INF_BEACON_DN:
			SendRadioBeacon(&downpkt,frame + szh,sz - szh);
		break;
		case	LP_TYPE_LRR_PKT_MULTICAST_DN:
			SendRadioClassCMultiCast(&downpkt,frame + szh,sz - szh);
		break;
		default :
		break;
		}
	}
	default :
	break;
	}
}

static	void	LapEventProceedTest(t_xlap_link *lk,int evttype,int evtnum,void *data,int sz)
{
	t_lrc_link	*lrc;
	time_t		rtt;
	int		nbu;

	lrc	= lk->lk_userptr;
	RTL_TRDBG(6,"LAP CB (%s,%s) st='%s' receive evttype=%d evtnum='%s'\n",
		lk->lk_addr,lk->lk_port,
		LapStateTxt(lk->lk_state),evttype,LapEventTxt(evtnum));

	if	(!lrc)
		return;

	if	(lrc->lrc_testinit == 0)
	{
		lrc->lrc_testlast	= Currtime.tv_sec;
		lrc->lrc_testinit	= 1;
	}

	LapState	= lk->lk_state;

	switch	(evtnum)
	{
	case	EVT_LK_CREATED :	// tcp server only
	case	EVT_TCP_CONNECTED :
		RTL_TRDBG(0,"LAP LRC CNX\n");
		TcpKeepAliveHigh(-1,lk->lk_fd);
		AvDvUiClear(&lrc->lrc_avdvtrip); // reset stats on new cnx
	break;
	case	EVT_LK_DELETED :
		RTL_TRDBG(0,"LAP LRC DISC (%d)\n",lrc->lrc_stat.lrc_nbdisc);
		lrc->lrc_testinit	= 0;
	break;
	case	EVT_LK_STOPPED :
		RTL_TRDBG(1,"LAP LRC STOPPED\n");
	break;
	case	EVT_LK_STARTED :
		RTL_TRDBG(1,"LAP LRC STARTED\n");
	break;
	case	EVT_FRAMEI :
		RTL_TRDBG(1,"LAP LRC FRAMEI\n");
	break;
	case	EVT_TESTFRcon :
		rtt	= ABS(rtl_tmmsmono() - lk->lk_tFrmSndAtMs);
		AvDvUiAdd(&lrc->lrc_avdvtrip,rtt,Currtime.tv_sec);
		if	(LapTest > 1)
		{
			printf("lrc=%s rtt=%d ucnt=%d\n",
				lk->lk_addr,(int)rtt,lk->lk_stat.st_nbsendu);
			fflush(stdout);
		}
		if	(ABS(Currtime.tv_sec - lrc->lrc_testlast) > 180)
		{
			nbu	= AvDvUiCompute(&lrc->lrc_avdvtrip,300,Currtime.tv_sec);
			printf("lrc=%s %d,%d,%d,%d,%d\n",lk->lk_addr,nbu,
			lrc->lrc_avdvtrip.ad_aver,
			lrc->lrc_avdvtrip.ad_sdev,
			lrc->lrc_avdvtrip.ad_aver+lrc->lrc_avdvtrip.ad_sdev,
			lrc->lrc_avdvtrip.ad_vmax);
			fflush(stdout);
			lrc->lrc_testlast	= Currtime.tv_sec;
		}
	break;
	default :
	break;
	}

}

static	void	DoClockMs()
{
	static	unsigned int errth;

	if	(!LgwThreadStopped && pthread_kill(LgwThread,0) != 0)
	{
		errth++;
		if	(errth >= 10)
		{
			errth	= 0;
			RTL_TRDBG(0,"lgw thread does not exist => restart\n");
			ReStartLgwThread();
			return;
		}
	}
	else
		errth	= 0;
}

static	int	SendToLrcRobin(t_lrr_pkt *uppkt,u_char *buff,int sz)
{
	static	unsigned	int	rr;
	t_xlap_link		*lk;
	int	ok	= 0;
	int	i;
	int	lrc;
	int	ret;
	float	sv_rssi;
	float	sv_snr;

	if	(!uppkt)
	{
		uppkt	= (t_lrr_pkt *)buff;
	}

	sv_rssi = uppkt->lp_rssi;
	sv_snr  = uppkt->lp_snr;

	RTL_TRDBG(6,"Robin on LRC available rr=%d\n",rr);
	for	(i = 0 ; i < NbLrc ; i++)
	{
		lrc	= rr % NbLrc;
		rr++;
		lk	= &TbLapLrc[lrc];
		uppkt->lp_lrrid	= TbLrrID[lrc];
		RTL_TRDBG(3,"try to send packet to LRC=%d lrrid=%08x\n",
					lrc,uppkt->lp_lrrid);

		if	(SimulN && QosAlea)
		{	// simulate several lrr and change qos
			uppkt->lp_rssi	= sv_rssi + (-5-(rand()%10));
			uppkt->lp_snr	= sv_snr + (-5-(rand()%10));
		}

		if	(lk->lk_state == SSP_STARTED 
					&& (ret=LapPutOutQueue(lk,buff,sz)) > 0)
		{
			ok++;
RTL_TRDBG(1,"packet sent to LRC=%d lrrid=%08x by robin rssi=%f snr=%f\n",
			lrc,uppkt->lp_lrrid,uppkt->lp_rssi,uppkt->lp_snr);
			return	ok;
		}
		else
			RTL_TRDBG(3,"LRC=%d not available\n",lrc);
	}

	return	ok;
}

static	int	SendToLrcOrder(t_lrr_pkt *uppkt,u_char *buff,int sz)
{
	t_xlap_link		*lk;
	int	ok	= 0;
	int	i;
	int	lrc;
	int	ret;

	if	(!uppkt)
	{
		uppkt	= (t_lrr_pkt *)buff;
	}

	RTL_TRDBG(6,"Order on LRC available nb=%d\n",NbLrc);
	for	(i = 0 ; i < NbLrc ; i++)
	{
		lrc	= i;
		if (LrrIDGetFromBS && MasterLrc >= 0)	// NFR997
		{
			// send to lrc master first
			if (i == 0)
				lrc = MasterLrc;
			else if (i == MasterLrc)	// send also to lrc 0, the master took its place
				lrc = 0;
		}

		lk	= &TbLapLrc[lrc];
		RTL_TRDBG(3,"try to send packet to LRC=%d lrrid=%08x\n",
					lrc,uppkt->lp_lrrid);

		if	(lk->lk_state == SSP_STARTED 
					&& (ret=LapPutOutQueue(lk,buff,sz)) > 0)
		{
			ok++;
			switch(uppkt->lp_type)
			{
			case	LP_TYPE_LRR_PKT_SENT_INDIC:
RTL_TRDBG(1,"indic sent to LRC=%d lrrid=%08x by order\n",
			lrc,uppkt->lp_lrrid);
			break;
			default :
RTL_TRDBG(1,"packet sent to LRC=%d lrrid=%08x by order rssi=%f snr=%f\n",
			lrc,uppkt->lp_lrrid,uppkt->lp_rssi,uppkt->lp_snr);
			break;
			}
			break;
		}
		else
			RTL_TRDBG(3,"LRC=%d not available\n",lrc);

		if (LrrIDGetFromBS && MasterLrc >= 0)	// NFR997
		{
			// restore lrc value for following keepalive treatment
			if (i == MasterLrc)
				lrc = i;
		}
	}

	// current LRC changes : restore keepalive on new LRC
	// reset keepalive on all others
	if	(ok && CurrLrcOrder != lrc)
	{
RTL_TRDBG(1,"LRC changes %d => %d\n",CurrLrcOrder,lrc);
		CurrLrcOrder	= lrc;
		for	(i = 0 ; i < NbLrc ; i++)
		{
			lk	= &TbLapLrc[i];
			if	(i == lrc)
			{
				TcpKeepAliveHigh(i,lk->lk_fd);
			}
			else
			{
				TcpKeepAliveLow(i,lk->lk_fd);
			}
		}
	}

	return	ok;
}

static	int	SendToLrc(t_lrr_pkt *uppkt,u_char *buff,int sz)
{
	int	ok	= 0;
	int	i;
	t_xlap_link	*lk;

	if	(!uppkt)
	{
		uppkt	= (t_lrr_pkt *)buff;
	}

	switch	(Redundancy)
	{
	case	1:	// round robin
		ok	=	SendToLrcRobin(uppkt,buff,sz);
	break;

	
	case	0:	// by order
	default	:
		ok	=	SendToLrcOrder(uppkt,buff,sz);
	break;
	}

	if	(ok == 0)
	{
		RTL_TRDBG(1,"no LRC available\n");
		for	(i = 0 ; i < NbLrc ; i++)
		{
			lk	= &TbLapLrc[i];
			if	(lk && lk->lk_state == SSP_STOPPED)
			{
				LapEventRequest(lk,EVT_TESTFRact,NULL,0);
			}
		}
		switch	(uppkt->lp_type)
		{
		case	LP_TYPE_LRR_PKT_RADIO :
			LrcNbPktDrop++;
		break;
		default :
		break;
		}
	}
	return	ok;
}

static	int	SendStatToAllLrc(u_char *buff,int sz,int delay)
{
	int	ret	= 0;
	int	lrc;

	if	(delay > 0)
	{
		t_imsg	*msg;

		msg	= rtl_imsgAlloc(IM_DEF,IM_LGW_DELAY_ALLLRC,NULL,0);
		if	(!msg)
		{
			return	-1;
		}
		if	( rtl_imsgDupData(msg,buff,sz) != msg)
		{
			rtl_imsgFree(msg);
			return	-1;
		}
		rtl_imsgAddDelayed(MainQ,msg,delay);
		RTL_TRDBG(3,"SendStatToAllLrc() delayed=%d\n",delay);
		return	0;
	}

	// stats are sent to all LRC
	for	(lrc = 0 ; lrc < NbLrc ; lrc++)
	{
		t_xlap_link		*lk;

		lk	= &TbLapLrc[lrc];
		if	(SimulN)
		{
			t_lrr_pkt	*uppkt;

			uppkt	= (t_lrr_pkt *)buff;
			uppkt->lp_lrrid	= TbLrrID[lrc];
		}
		if	(lk->lk_state == SSP_STARTED)
		{
			LapPutOutQueue(lk,(u_char *)buff,sz);
			ret++;
		}
	}

	return	ret;
}

static	void	DoHtmlFile()
{
	FILE	*f;
	char	file[512];
	char	tmp[512];

#ifdef	WIRMAV2
	sprintf	(file,"%s","/home/www/index.html");
#else
	sprintf	(file,"%s/var/log/lrr",RootAct);
	rtl_mkdirp(file);
	sprintf	(file,"%s/var/log/lrr/stat.html",RootAct);
#endif
	f	= fopen(file,"w");
	if	(!f)
		return;

	rtl_getCurrentIso8601date(tmp);

	fprintf(f,"<br>Update=%s</br>\n",tmp);
	fprintf(f,"<br>Uptime=%s</br>\n",UptimeProcStr);
	fprintf(f,"<br>Version=%s</br>\n",lrr_whatStr);
	fprintf(f,"<br>LrxVersion=%d</br>\n",LP_LRX_VERSION);
	fprintf(f,"<br>LrrId=%08x (%u) lat=%f lon=%f alt=%d</br>\n",
				LrrID,LrrID,LrrLat,LrrLon,LrrAlt);
	fprintf(f,"<br>LapState=%s</br>\n",LapStateTxt(LapState));

	fprintf(f,"<br>LgwNbPacketSend=%u</br>\n",LgwNbPacketSend);
	fprintf(f,"<br>LgwNbPacketWait=%u</br>\n",LgwNbPacketWait);
	fprintf(f,"<br>LgwNbPacketRecv=%u</br>\n",LgwNbPacketRecv);

	fprintf(f,"<br>LgwNbStartOk=%u</br>\n",LgwNbStartOk);
	fprintf(f,"<br>LgwNbStartFailure=%u</br>\n",LgwNbStartFailure);
	fprintf(f,"<br>LgwNbConfigFailure=%u</br>\n",LgwNbConfigFailure);
	fprintf(f,"<br>LgwNbLinkDown=%u</br>\n",LgwNbLinkDown);


	fprintf(f,"<br>LgwNbBusySend=%u</br>\n",LgwNbBusySend);
	fprintf(f,"<br>LgwNbSyncError=%u</br>\n",LgwNbSyncError);
	fprintf(f,"<br>LgwNbCrcError=%u</br>\n",LgwNbCrcError);
	fprintf(f,"<br>LgwNbSizeError=%u</br>\n",LgwNbSizeError);
	fprintf(f,"<br>LgwNbChanUpError=%u</br>\n",LgwNbChanUpError);
	fprintf(f,"<br>LgwNbChanDownError=%u</br>\n",LgwNbChanDownError);
	fprintf(f,"<br>LgwNbDelayError=%u</br>\n",LgwNbDelayError);
	fprintf(f,"<br>LgwNbDelayReport=%u</br>\n",LgwNbDelayReport);
	fprintf(f,"<br>MacNbFcsError=%u</br>\n",MacNbFcsError);
	fprintf(f,"<br>LrcNbPktTrip=%u</br>\n",LrcNbPktTrip);
	fprintf(f,"<br>LrcAvPktTrip=%u</br>\n",LrcAvPktTrip);
	fprintf(f,"<br>LrcDvPktTrip=%u</br>\n",LrcDvPktTrip);
	fprintf(f,"<br>LrcMxPktTrip=%u</br>\n",LrcMxPktTrip);
	fprintf(f,"<br>LrcNbPktDrop=%u</br>\n",LrcNbPktDrop);
	fprintf(f,"<br>LrcNbDisc=%u</br>\n",LrcNbDisc);


	fprintf(f,"<br>NbLrc=%u Adjust=%d Redundancy=%d</br>\n",
						NbLrc,AdjustDelay,Redundancy);
	fprintf(f,"<br>StatRefresh=%d RfCellRefresh=%d WanRefresh=%d</br>\n",
				StatRefresh,RfCellRefresh,WanRefresh);
	fprintf(f,"<br>InvertPol=%u NoCrc=%d NoHeader=%d SyncWord=%x</br>\n",
				LgwInvertPol,LgwNoCrc,LgwNoHeader,LgwSyncWord);
	fprintf(f,"<br>Preamble=%u PreambleAck=%d Power=%d AckData802=%u</br>\n",
			LgwPreamble,LgwPreambleAck,LgwPower,LgwAckData802Wait);
	fprintf(f,"<br>SimulN=%u QosAlea=%d</br>\n",
				SimulN,QosAlea);

	fclose(f);
}

static	int	LrcOkCount()
{
	int	lrc;
	int	nb	= 0;

	for	(lrc = 0 ; lrc < NbLrc ; lrc++)
	{
		t_xlap_link		*lk;

		lk	= &TbLapLrc[lrc];
		switch	(lk->lk_state)
		{
		case	SSP_STARTED:
		case	SSP_STOPPED:
			nb++;
		break;
		default	:
		break;
		}
	}
	return	nb;
}

static	int	LrcOkStarted()
{
	int	lrc;
	int	nb	= 0;

	for	(lrc = 0 ; lrc < NbLrc ; lrc++)
	{
		t_xlap_link		*lk;

		lk	= &TbLapLrc[lrc];
		switch	(lk->lk_state)
		{
		case	SSP_STARTED:
			nb++;
		break;
		default	:
		break;
		}
	}
	return	nb;
}

static	int	LrcOkStartedTestLoad(int out,int ack)
{
	int	lrc;
	int	nb	= 0;

	for	(lrc = 0 ; lrc < NbLrc ; lrc++)
	{
		t_xlap_link		*lk;

		lk	= &TbLapLrc[lrc];
		switch	(lk->lk_state)
		{
		case	SSP_STARTED:
			if	(out >= 0 && lk->lk_outcount > out)
				break;
			if	(ack >= 0 && lk->lk_ackcount > ack)
				break;
			nb++;
		break;
		default	:
		break;
		}
	}
	return	nb;
}

static	void	DoPowerRefresh(int delay,u_short lptype,u_char state,u_int raw,float volt)
{
	u_char		buff[1024];
	t_lrr_pkt	uppkt;
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int		szm	= 0;
	char		*data	= NULL;

	t_lrr_power	power;

	power.pw_gss	= (time_t)Currtime.tv_sec;
	power.pw_gns	= (u_int)Currtime.tv_nsec;
	power.pw_raw	= raw;
	power.pw_volt	= volt;
	power.pw_state	= state;

	delay	= 0;
	szm	= 0;
	data	= NULL;
	szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_power);
	memset	(&uppkt,0,sizeof(uppkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_szh	= szh;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_type	= lptype;
	uppkt.lp_lrrid	= LrrID;

	memcpy	(&uppkt.lp_u.lp_power,&power,sizeof(t_lrr_power));
	memcpy	(buff,&uppkt,szh);
	if	(szm > 0)
		memcpy	(buff+szh,data,szm);

	SendStatToAllLrc(buff,szh+szm,delay);
}

#ifdef WITH_GPS
static	void	DoGpsRefresh(int delay, u_short lptype, u_char state, float srate_wma)
{
	u_char		buff[1024];
	t_lrr_pkt	uppkt;
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int		szm	= 0;
	char		*data	= NULL;

	t_lrr_gps_st	gps_st;

	gps_st.gps_gssupdate	= (time_t)Currtime.tv_sec;
	gps_st.gps_gnsupdate	= (u_int)Currtime.tv_nsec;
	gps_st.gps_state	= state;
	gps_st.gps_srate_wma = srate_wma;

	delay	= 0;
	szm	= 0;
	data	= NULL;
	szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_gps_st);
	memset (&uppkt, 0, sizeof (uppkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_szh	= szh;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_type	= lptype;
	uppkt.lp_lrrid	= LrrID;

	memcpy (&uppkt.lp_u.lp_gps_st, &gps_st, sizeof (t_lrr_gps_st));
	memcpy (buff, &uppkt, szh);
	if (szm > 0)
		memcpy (buff+szh, data, szm);

	SendStatToAllLrc(buff, szh+szm, delay);
}
#endif

void	DoStatRefresh(int delay)
{
	static	u_int	StatNbUpdate;

	u_char		buff[1024];
	t_lrr_pkt	uppkt;
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int		szm	= 0;
	char		*data	= NULL;

	t_lrr_stat_v1	*stat_v1 = &uppkt.lp_u.lp_stat_v1;



RTL_TRDBG(9,"AvDvUiCompute p=%p d=%u t=%u idxlrc=%d\n",
		&CmnLrcAvdvTrip,StatRefresh,Currtime.tv_sec,CurrLrcOrder);

	LrcNbPktTrip	= 
		AvDvUiCompute(&CmnLrcAvdvTrip,StatRefresh,Currtime.tv_sec);
	LrcAvPktTrip	= CmnLrcAvdvTrip.ad_aver;
	LrcDvPktTrip	= CmnLrcAvdvTrip.ad_sdev;
	LrcMxPktTrip	= CmnLrcAvdvTrip.ad_vmax;

	LrcNbDisc	= 0;
	if	(CurrLrcOrder >= 0 && CurrLrcOrder < NB_LRC_PER_LRR)
	{
		t_lrc_link	*lrc;

		lrc		= &TbLrc[CurrLrcOrder];
		LrcNbDisc	= lrc->lrc_stat.lrc_nbdisc;
	}

	DoHtmlFile();

	szm	= 0;
	data	= NULL;
	szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_stat_v1);
	memset	(&uppkt,0,sizeof(uppkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_szh	= szh;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_type	= LP_TYPE_LRR_INF_STATS_V1;
	uppkt.lp_lrrid	= LrrID;

	stat_v1->ls_LrcNbDisc		= LrcNbDisc;
	stat_v1->ls_LrcNbPktDrop	= LrcNbPktDrop;
	stat_v1->ls_LrcNbPktTrip	= LrcNbPktTrip;
	stat_v1->ls_LrcAvPktTrip	= LrcAvPktTrip;
	stat_v1->ls_LrcDvPktTrip	= LrcDvPktTrip;
	stat_v1->ls_LrcMxPktTrip	= LrcMxPktTrip;

	stat_v1->ls_LgwNbPacketSend	= LgwNbPacketSend;
	stat_v1->ls_LgwNbPacketWait	= LgwNbPacketWait;
	stat_v1->ls_LgwNbPacketRecv	= LgwNbPacketRecv;

	stat_v1->ls_LgwNbStartOk	= LgwNbStartOk;
	stat_v1->ls_LgwNbStartFailure	= LgwNbStartFailure;
	stat_v1->ls_LgwNbConfigFailure	= LgwNbConfigFailure;
	stat_v1->ls_LgwNbLinkDown	= LgwNbLinkDown;

	stat_v1->ls_LgwNbBusySend	= LgwNbBusySend;
	stat_v1->ls_LgwNbSyncError	= LgwNbSyncError;
	stat_v1->ls_LgwNbCrcError	= LgwNbCrcError;
	stat_v1->ls_LgwNbSizeError	= LgwNbSizeError;
	stat_v1->ls_LgwNbChanUpError	= LgwNbChanUpError;
	stat_v1->ls_LgwNbChanDownError	= LgwNbChanDownError;
	stat_v1->ls_LgwNbDelayReport	= LgwNbDelayReport;
	stat_v1->ls_LgwNbDelayError	= LgwNbDelayError;
// added in LRC 1.0.14, LRR 1.0.35
	stat_v1->ls_gssuptime		= (time_t)UptimeProc.tv_sec;
	stat_v1->ls_gnsuptime		= (u_int)UptimeProc.tv_nsec;
	stat_v1->ls_nbupdate		= ++StatNbUpdate;
	stat_v1->ls_statrefresh		= StatRefresh;
	stat_v1->ls_versMaj		= VersionMaj;
	stat_v1->ls_versMin		= VersionMin;
	stat_v1->ls_versRel		= VersionRel;
	stat_v1->ls_versFix		= VersionFix;
	stat_v1->ls_LgwInvertPol	= LgwInvertPol;
	stat_v1->ls_LgwNoCrc		= LgwNoCrc;
	stat_v1->ls_LgwNoHeader		= LgwNoHeader;
	stat_v1->ls_LgwPreamble		= LgwPreamble;
	stat_v1->ls_LgwPreambleAck	= LgwPreambleAck;
	stat_v1->ls_LgwPower		= LgwPower;
	stat_v1->ls_LgwAckData802Wait	= LgwAckData802Wait;
	stat_v1->ls_LrrUseGpsPosition	= UseGpsPosition;
	stat_v1->ls_LrrUseGpsTime	= UseGpsTime;
	stat_v1->ls_LrrUseLgwTime	= UseLgwTime;
	stat_v1->ls_LgwSyncWord		= LgwSyncWord;
	stat_v1->ls_rfcellrefresh	= RfCellRefresh;
	stat_v1->ls_gssupdate		= (time_t)Currtime.tv_sec;
	stat_v1->ls_gnsupdate		= (u_int)Currtime.tv_nsec;
	stat_v1->ls_gssuptimesys	= (time_t)UptimeSyst.tv_sec;
	stat_v1->ls_gnsuptimesys	= (u_int)UptimeSyst.tv_nsec;
	stat_v1->ls_sickrestart		= SickRestart;
	stat_v1->ls_configrefresh	= ConfigRefresh;
	stat_v1->ls_wanrefresh		= WanRefresh;

// added in LRC 1.0.31, LRR 1.4.4
	CompAllMfsInfos('L');
	{
	int	i;
	u_int	val;

	for	(i = 0 ; i < NB_MFS_PER_LRR ; i++)
	{
		if	(TbMfs[i].fs_enable == 0)	continue;
		if	(TbMfs[i].fs_exists == 0)	continue;

		val	= TbMfs[i].fs_used;
		stat_v1->ls_mfsUsed[i][0]	= (u_char)(val / (256*256));
		stat_v1->ls_mfsUsed[i][1]	= (u_char)(val / 256);
		stat_v1->ls_mfsUsed[i][2]	= (u_char)val;

		val	= TbMfs[i].fs_avail;
		stat_v1->ls_mfsAvail[i][0]	= (u_char)(val / (256*256));
		stat_v1->ls_mfsAvail[i][1]	= (u_char)(val / 256);
		stat_v1->ls_mfsAvail[i][2]	= (u_char)val;
	}
	}

	AvDvUiCompute(&CpuAvdvUsed,StatRefresh,Currtime.tv_sec);
	stat_v1->ls_cpuAv	= (u_char)CpuAvdvUsed.ad_aver;
	stat_v1->ls_cpuDv	= (u_char)CpuAvdvUsed.ad_sdev;
	stat_v1->ls_cpuMx	= (u_char)CpuAvdvUsed.ad_vmax;
	stat_v1->ls_cpuMxTime	= CpuAvdvUsed.ad_tmax;

	CompAllCpuInfos('L');
	stat_v1->ls_cpuLoad1	= CpuLoad1;
	stat_v1->ls_cpuLoad5	= CpuLoad5;
	stat_v1->ls_cpuLoad15	= CpuLoad15;

	CompAllMemInfos('L');
	stat_v1->ls_MemTotal[0]		= (u_char)(MemTotal / (256*256));
	stat_v1->ls_MemTotal[1]		= (u_char)(MemTotal / 256);
	stat_v1->ls_MemTotal[2]		= (u_char)(MemTotal);
	stat_v1->ls_MemFree[0]		= (u_char)(MemFree / (256*256));
	stat_v1->ls_MemFree[1]		= (u_char)(MemFree / 256);
	stat_v1->ls_MemFree[2]		= (u_char)(MemFree);
	stat_v1->ls_MemBuffers[0]	= (u_char)(MemBuffers / (256*256));
	stat_v1->ls_MemBuffers[1]	= (u_char)(MemBuffers / 256);
	stat_v1->ls_MemBuffers[2]	= (u_char)(MemBuffers);
	stat_v1->ls_MemCached[0]	= (u_char)(MemCached / (256*256));
	stat_v1->ls_MemCached[1]	= (u_char)(MemCached / 256);
	stat_v1->ls_MemCached[2]	= (u_char)(MemCached);


	stat_v1->ls_gpsUpdateCnt 	= ABS(GpsUpdateCnt - GpsUpdateCntP);
	GpsUpdateCntP		 	= GpsUpdateCnt;
#ifdef	TEKTELIC
	if	(GpsPositionOk)
		stat_v1->ls_gpsUpdateCnt	= StatRefresh;
	else
		stat_v1->ls_gpsUpdateCnt	= 0;
#endif

	stat_v1->ls_LrrTimeSync		= NtpdStarted();
	stat_v1->ls_LrrReverseSsh	= CmdCountOpenSsh();

	stat_v1->ls_powerState		= PowerState;
	stat_v1->ls_powerDownCnt	= ABS(PowerDownCnt - PowerDownCntP);
	PowerDownCntP			= PowerDownCnt;

	stat_v1->ls_gpsState		= GpsStatus;
	stat_v1->ls_gpsDownCnt		= ABS(GpsDownCnt - GpsDownCntP);
	GpsDownCntP			= GpsDownCnt;

	stat_v1->ls_gpsUpCnt		= ABS(GpsUpCnt - GpsUpCntP);
	GpsUpCntP			= GpsUpCnt;

	stat_v1->ls_rfScan		= CmdCountRfScan();

	stat_v1->ls_traceLevel		= TraceLevel + 1;// 0 means ? for LRC
	stat_v1->ls_useRamDir		= LogUseRamDir;

	memcpy	(buff,&uppkt,szh);
	if	(szm > 0)
		memcpy	(buff+szh,data,szm);

	SendStatToAllLrc(buff,szh+szm,delay);
}

void	GpsGetInfos(u_char *mode,float *lat,float *lon,short *alt,u_int *cnt)
{
	if (UseGpsPosition == 1) {
#ifdef TEKTELIC
		GetGpsPositionTektelic();
#endif
		/* Real GPS data */
		if (GpsPositionOk && mode && lat && lon && alt) {
			*mode	= 2;	// gps
			*lat	= GpsLatt;
			*lon	= GpsLong;
			*alt	= GpsAlti;
		}
		if (cnt)
			*cnt	= GpsUpdateCnt;
	} else {
		/* Simulated GPS data */
		if (mode && lat && lon && alt) {
			*mode	= 1;
			*lat	= LrrLat;
			*lon	= LrrLon;
			*alt	= LrrAlt;
		}
		if (cnt)
			*cnt	= 0;
	}
}

#ifdef	SX1301AR_MAX_BOARD_NB
/*!
* \fn void DoLocKeyRefresh(int delay, char ** resp)
* \brief Refresh and send AES-128 keys for each board
* \param delay:
* \param resp: Adress of response string
* \return void
*
* This function reads the configuration hashtables to retrieve boards
* finetimestamping AES keys send the result to LRC(s)
* If resp is passed to NULL, IEC104/XML format is used.
* If not, the resp string is filled for using IEC104/Text format (Built-in shell cmd)
*
* resp string should be long enough to contains at least 160 bytes
*
*/
void DoLocKeyRefresh(int delay, char ** resp)
{
	u_char		buff[1024];
	char *		key = NULL;
	int		board, nbBoard;
	char		section[64];
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	char		resp_tmp[40];

	t_lrr_pkt		uppkt;
	t_lrr_config_lockey	loc_key;

	RTL_TRDBG(1,"DoLocKeyRefresh\n");
	/* NFR622: Send configured boards finetimestamps AES-128 keys to LRC */
	nbBoard = CfgInt(HtVarLgw, "gen", -1, "board", 1);
	for (board = 0; board < nbBoard && board < SX1301AR_MAX_BOARD_NB; board++) {
		sprintf(section, "board:%d", board);
		key = CfgStr(HtVarLrr, section ,-1, "aeskey", NULL);

		if (resp && *resp) {
			if (key && *key) {
			    sprintf(resp_tmp, "card%d=%s\n", board, key);
			} else {
			    sprintf(resp_tmp, "card%d=\n", board);
			}
			strcat(*resp, resp_tmp);
		}
		else {
			szh = LP_PRE_HEADER_PKT_SIZE + sizeof (t_lrr_config_lockey);
			memset(&loc_key, 0, sizeof (loc_key));
			memset(&uppkt, 0, sizeof (uppkt));
			uppkt.lp_vers	= LP_LRX_VERSION;
			uppkt.lp_szh	= szh;
			uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
			uppkt.lp_type	= LP_TYPE_LRR_INF_CONFIG_LOCKEY;
			uppkt.lp_lrrid	= LrrID;

			loc_key.cf_idxboard = board;
			if (key && *key) {
				memcpy(&loc_key.cf_lockey, key, 2 * sizeof (u_char) * SX1301AR_BOARD_AES_KEY_SIZE/8);
			}

			memcpy (&uppkt.lp_u.lp_config_lockey, &loc_key, sizeof (t_lrr_config_lockey));
			memcpy (buff, &uppkt, szh);
			SendStatToAllLrc(buff, szh, delay);
		}
	}
}
#endif





void DoCustomVersionRefresh(int delay)
{
	u_char	buff[1024];
	int	szh = LP_HEADER_PKT_SIZE_V0;

	t_lrr_pkt	uppkt;
	t_lrr_custom_ver	cversions;
	RTL_TRDBG(1, "DoCustomVersionRefresh\n");
	/* NFR590 */
	szh = LP_PRE_HEADER_PKT_SIZE + sizeof (t_lrr_custom_ver);
	memset(&cversions, 0, sizeof (cversions));
	memset(&uppkt, 0, sizeof (uppkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_szh	= szh;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_type	= LP_TYPE_LRR_INF_CUSTOM_VERSION;
	uppkt.lp_lrrid	= LrrID;

	/* Hardware version */
	if (strlen(CustomVersion_Hw) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_hw, CustomVersion_Hw);
	/* Os version */
	if (strlen(CustomVersion_Os) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_os, CustomVersion_Os);
	/* HAL version */
	if (strlen(CustomVersion_Hal) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_hal, CustomVersion_Hal);
	/* Custom build version */
	if (strlen(CustomVersion_Build) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_build, CustomVersion_Build);
	/* Configuration version */
	if (strlen(CustomVersion_Config) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_config, CustomVersion_Config);
	/* Custom1 version */
	if (strlen(CustomVersion_Custom1) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_custom1, CustomVersion_Custom1);
	/* Custom2 version */
	if (strlen(CustomVersion_Custom2) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_custom2, CustomVersion_Custom2);
	/* Custom3 version */
	if (strlen(CustomVersion_Custom3) <= MAX_CUSTOM_VERSION_LEN)
		strcpy((char *)cversions.cf_cver_custom3, CustomVersion_Custom3);

	memcpy (&uppkt.lp_u.lp_custom_ver, &cversions, sizeof (t_lrr_custom_ver));
	memcpy (buff, &uppkt, szh);
	SendStatToAllLrc(buff, szh, delay);

}

/*!
* \fn void DoAntsConfigRefresh(int delay, char ** resp)
* \brief Refresh and send antennas configuration, user-requested Tx EIRP and LUT-calibrated Tx EIRP
* \param delay:
* \param resp: Adress of response string
* \return void
*
* This function retrieve the antenna configuration and user-requested Tx EIRP then calculate the LUT-calibrated Tx EIRP.
* All these values are send to LRC(s).
*
* If resp argument is passed to NULL, IEC104/XML format is used.
* If not, the resp string is filled for using IEC104/Text format (Built-in shell cmd)
*
* resp string should be long enough to contains at least 160 bytes
*
*/
void DoAntsConfigRefresh(int delay, char ** resp)
{
	u_char		buff[1024];
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int 		i;
	char		resp_tmp[40];

	t_lrr_pkt		uppkt;
	t_lrr_config_ants	ants;

	RTL_TRDBG(1,"DoAntsConfigRefresh\n");
	/* NFR620 */
	for (i = 0; (i < LgwAntenna) && (i < NB_ANTENNA); i++)
	{
		if (resp && *resp) {
			sprintf(resp_tmp, "chain%d=%d-%d\n", i, (int)roundf(AntennaGain[i]), (int)roundf(CableLoss[i]));
			//sprintf(resp_tmp, "chain%d=%.1f-%.1f\n", i, AntennaGain[i], CableLoss[i]);
			strcat(*resp, resp_tmp);
		}
		else {
			szh = LP_PRE_HEADER_PKT_SIZE + sizeof (t_lrr_config_ants);
			memset(&ants, 0, sizeof (ants));
			memset(&uppkt, 0, sizeof (uppkt));
			uppkt.lp_vers	= LP_LRX_VERSION;
			uppkt.lp_szh	= szh;
			uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
			uppkt.lp_type	= LP_TYPE_LRR_INF_CONFIG_ANTS;
			uppkt.lp_lrrid	= LrrID;

			ants.cf_idxant = i;
			if (AntennaGain[i] || CableLoss[i])
				ants.cf_use_float = 1;

			ants.cf_antgain = (int8_t)roundf(AntennaGain[i]);
			ants.cf_cableloss = (int8_t)roundf(CableLoss[i]);
			ants.cf_antgain_f = AntennaGain[i];
			ants.cf_cableloss_f = CableLoss[i];

			ants.cf_RX1_tx = TbChannel[1].power;
			ants.cf_RX1_eirp = GetTxCalibratedEIRP(TbChannel[1].power, AntennaGain[i], CableLoss[i], 0, 0);
			if	(Rx2Channel)
			{
			ants.cf_RX2_tx = Rx2Channel->power;
			ants.cf_RX2_eirp = GetTxCalibratedEIRP(Rx2Channel->power, AntennaGain[i], CableLoss[i], 0, 0);
			}
			//RTL_TRDBG(1, "Antenna config idx=%d, antgain=%d, cableloss=%d, rx1_tx=%d, rx1_eirp=%d, rx2_tx=%d, rx2_eirp=%d\n", i, ants.cf_antgain, ants.cf_cableloss, ants.cf_RX1_tx, ants.cf_RX1_eirp, ants.cf_RX2_tx, ants.cf_RX2_eirp);
			RTL_TRDBG(1, "Antenna config idx=%d, use_float=%d, antgain=%.1f=%d, cableloss=%.1f=%d, rx1_tx=%d, rx1_eirp=%d, rx2_tx=%d, rx2_eirp=%d\n", i, ants.cf_use_float, ants.cf_antgain_f, ants.cf_antgain, ants.cf_cableloss_f, ants.cf_cableloss, ants.cf_RX1_tx, ants.cf_RX1_eirp, ants.cf_RX2_tx, ants.cf_RX2_eirp);

			memcpy (&uppkt.lp_u.lp_config_ants, &ants, sizeof (t_lrr_config_ants));
			memcpy (buff, &uppkt, szh);
			SendStatToAllLrc(buff, szh, delay);
		}
	}

}


void	DoConfigRefresh(int delay)
{
	static	u_int		ConfigNbUpdate;

	u_char		buff[1024];
	t_lrr_pkt	uppkt;
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int		szm	= 0;
	char		*data	= NULL;

	int			idxlrc;
	t_lrr_config_lrc	cfglrc;
	t_lrr_config		config;
	int			mfs;

	for	(idxlrc = 0 ; idxlrc < NbLrc ; idxlrc++)
	{
		int	port	= atoi(TbLapLrc[idxlrc].lk_port);
		char	*addr	= (char *)TbLapLrc[idxlrc].lk_addr;

		if	(port <= 0 || !addr || !*addr)
			continue;
		memset	(&cfglrc,0,sizeof(cfglrc));
		szm	= 0;
		data	= NULL;
		szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_config_lrc);
		memset	(&uppkt,0,sizeof(uppkt));
		uppkt.lp_vers	= LP_LRX_VERSION;
		uppkt.lp_szh	= szh;
		uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
		uppkt.lp_type	= LP_TYPE_LRR_INF_CONFIG_LRC;
		uppkt.lp_lrrid	= LrrID;

		cfglrc.cf_LrcCount	= NbLrc;
		cfglrc.cf_LrcIndex	= idxlrc;
		cfglrc.cf_LrcPort	= (u_short)port;
		strncpy((char *)cfglrc.cf_LrcUrl,addr,64);

		memcpy	(&uppkt.lp_u.lp_config_lrc,&cfglrc,
					sizeof(t_lrr_config_lrc));
		memcpy	(buff,&uppkt,szh);
		if	(szm > 0)
			memcpy	(buff+szh,data,szm);

		SendStatToAllLrc(buff,szh+szm,delay);
	}
#ifdef	SX1301AR_MAX_BOARD_NB
	DoLocKeyRefresh(delay, NULL);
#endif
	DoAntsConfigRefresh(delay, NULL);
	DoCustomVersionRefresh(delay);
	DoLrrUIDConfigRefresh(delay);

	memset	(&config,0,sizeof(config));
	szm	= 0;
	data	= NULL;
	szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_config);
	memset	(&uppkt,0,sizeof(uppkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_szh	= szh;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_type	= LP_TYPE_LRR_INF_CONFIG;
	uppkt.lp_lrrid	= LrrID;

	config.cf_nbupdate		= ++ConfigNbUpdate;
	config.cf_configrefresh		= ConfigRefresh;
	config.cf_gssupdate		= (time_t)Currtime.tv_sec;
	config.cf_gnsupdate		= (u_int)Currtime.tv_nsec;

	ReadRffInfos();

	config.cf_versMaj		= VersionMaj;
	config.cf_versMin		= VersionMin;
	config.cf_versRel		= VersionRel;
	config.cf_versFix		= VersionFix;
	config.cf_versMajRff		= VersionMajRff;
	config.cf_versMinRff		= VersionMinRff;
	config.cf_versRelRff		= VersionRelRff;
	config.cf_versFixRff		= VersionFixRff;
	config.cf_rffTime		= RffTime;
	config.cf_LrxVersion		= LP_LRX_VERSION;
	config.cf_LgwSyncWord		= LgwSyncWord;

#if	0
	config.cf_LrrLocMeth		= 0;	// unknown
	if	(UseGpsPosition == 0)
	{
		config.cf_LrrLocMeth		= 1;	// manual
		config.cf_LrrLocLat		= LrrLat;
		config.cf_LrrLocLon		= LrrLon;
		config.cf_LrrLocAltitude	= LrrAlt;
	}
	else
	{
		if	(GpsPositionOk)
		{
			config.cf_LrrLocMeth		= 2;	// gps
			config.cf_LrrLocLat		= GpsLatt;
			config.cf_LrrLocLon		= GpsLong;
			config.cf_LrrLocAltitude	= GpsAlti;
		}
		else
		{
		}
	}
#endif

	GpsGetInfos(&config.cf_LrrLocMeth,&config.cf_LrrLocLat,
		&config.cf_LrrLocLon,&config.cf_LrrLocAltitude,NULL);

	config.cf_LrrUseGpsPosition	= UseGpsPosition;
	config.cf_LrrUseGpsTime		= UseGpsTime;
	config.cf_LrrUseLgwTime		= UseLgwTime;
	if	(GpsFd != -1)
		config.cf_LrrGpsOk	= 1;

	config.cf_LrcDistribution	= Redundancy;

	memcpy	(config.cf_IpInt,ConfigIpInt.cf_IpInt,
					sizeof(config.cf_IpInt));
	memcpy	(config.cf_ItfType,ConfigIpInt.cf_ItfType,
					sizeof(config.cf_ItfType));

	for	(mfs = 0 ; mfs < NB_MFS_PER_LRR ; mfs++)
	{
		if	(TbMfs[mfs].fs_enable == 0)	continue;
		if	(TbMfs[mfs].fs_exists == 0)	continue;
		strncpy	((char *)config.cf_Mfs[mfs],TbMfs[mfs].fs_name,16);
		config.cf_MfsType[mfs]	= TbMfs[mfs].fs_type;
	}

	memcpy	(&uppkt.lp_u.lp_config,&config,sizeof(t_lrr_config));
	memcpy	(buff,&uppkt,szh);
	if	(szm > 0)
		memcpy	(buff+szh,data,szm);

	SendStatToAllLrc(buff,szh+szm,delay);
}

void	DoRfcellRefresh(int delay)
{
	static	u_int		RfCellNbUpdate;
	static	t_lrr_stat_v1	StatSave;

	u_char		buff[1024];
	t_lrr_pkt	uppkt;
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int		szm	= 0;
	char		*data	= NULL;

	t_lrr_rfcell	rfcell;

	memset	(&rfcell,0,sizeof(rfcell));
	szm	= 0;
	data	= NULL;
	szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_rfcell);
	memset	(&uppkt,0,sizeof(uppkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_szh	= szh;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_type	= LP_TYPE_LRR_INF_RFCELL;
	uppkt.lp_lrrid	= LrrID;

	rfcell.rf_nbupdate		= ++RfCellNbUpdate;
	rfcell.rf_rfcellrefresh		= RfCellRefresh;
	rfcell.rf_gssupdate		= (time_t)Currtime.tv_sec;
	rfcell.rf_gnsupdate		= (u_int)Currtime.tv_nsec;

	rfcell.rf_LgwNbPacketRecv	= 
		ABS(LgwNbPacketRecv 	- StatSave.ls_LgwNbPacketRecv);
	rfcell.rf_LgwNbCrcError		= 
		ABS(LgwNbCrcError 	- StatSave.ls_LgwNbCrcError);
	rfcell.rf_LgwNbSizeError	= 
		ABS(LgwNbSizeError 	- StatSave.ls_LgwNbSizeError);
	rfcell.rf_LgwNbDelayReport	= 
		ABS(LgwNbDelayReport 	- StatSave.ls_LgwNbDelayReport);
	rfcell.rf_LgwNbDelayError	= 
		ABS(LgwNbDelayError 	- StatSave.ls_LgwNbDelayError);

	rfcell.rf_LgwNbPacketSend	= 
		ABS(LgwNbPacketSend 	- StatSave.ls_LgwNbPacketSend);
	rfcell.rf_LgwNbPacketWait	= 
		ABS(LgwNbPacketWait 	- 0);
	rfcell.rf_LgwNbBusySend		= 
		ABS(LgwNbBusySend 	- StatSave.ls_LgwNbBusySend);

	rfcell.rf_LgwNbStartOk		= 
		ABS(LgwNbStartOk 	- StatSave.ls_LgwNbStartOk);
	rfcell.rf_LgwNbStartFailure	= 
		ABS(LgwNbStartFailure 	- StatSave.ls_LgwNbStartFailure);
	rfcell.rf_LgwNbConfigFailure	= 
		ABS(LgwNbConfigFailure 	- StatSave.ls_LgwNbConfigFailure);
	rfcell.rf_LgwNbLinkDown		= 
		ABS(LgwNbLinkDown 	- StatSave.ls_LgwNbLinkDown);

	rfcell.rf_LgwState		= 0;
	if	(!LgwThreadStopped && LgwStarted())
	{
		if	(DownRadioStop)
			rfcell.rf_LgwState		= 1;
		else
			rfcell.rf_LgwState		= 2;
	}

	rfcell.rf_LgwBeaconRequestedCnt	= LgwBeaconRequestedCnt;
	rfcell.rf_LgwBeaconSentCnt	= LgwBeaconSentCnt;
	rfcell.rf_LgwBeaconLastDeliveryCause	= LgwBeaconLastDeliveryCause;

	StatSave.ls_LgwNbPacketRecv	= LgwNbPacketRecv;
	StatSave.ls_LgwNbCrcError	= LgwNbCrcError;
	StatSave.ls_LgwNbSizeError	= LgwNbSizeError;
	StatSave.ls_LgwNbDelayReport	= LgwNbDelayReport;
	StatSave.ls_LgwNbDelayError	= LgwNbDelayError;

	StatSave.ls_LgwNbPacketSend	= LgwNbPacketSend;
	StatSave.ls_LgwNbBusySend	= LgwNbBusySend;

	StatSave.ls_LgwNbStartOk	= LgwNbStartOk;
	StatSave.ls_LgwNbStartFailure	= LgwNbStartFailure;
	StatSave.ls_LgwNbConfigFailure	= LgwNbConfigFailure;
	StatSave.ls_LgwNbLinkDown	= LgwNbLinkDown;


	memcpy	(&uppkt.lp_u.lp_rfcell,&rfcell,sizeof(t_lrr_rfcell));
	memcpy	(buff,&uppkt,szh);
	if	(szm > 0)
		memcpy	(buff+szh,data,szm);

	SendStatToAllLrc(buff,szh+szm,delay);
}

static	void	DoWanRefresh(int delay)
{
	static	u_int	WanNbUpdate;

	u_char		buff[1024];
	t_lrr_pkt	uppkt;
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int		szm	= 0;
	char		*data	= NULL;
	int		deltadrop	= 0;
	int		nblrc	= 0;

	t_lrr_wan	wan;
	int		i;

	
	for	(i = 0 ; i < NB_LRC_PER_LRR ; i++)
	{
		t_lrc_link	*lrc;
		t_xlap_link	*lk;
		t_lrr_wan_lrc	wanlrc;
		int		state;
		int		nbelem;

		memset	(&wanlrc,0,sizeof(wanlrc));

		lrc	= &TbLrc[i];
		lk	= lrc->lrc_lk;
		if	(!lk)		continue;
		nblrc++;

		nbelem = AvDvUiCompute(&lrc->lrc_avdvtrip,WanRefresh,Currtime.tv_sec);

		state	= (lk->lk_state == SSP_STARTED);
		lrc->lrc_stat.lrc_nbsendB	= lk->lk_stat.st_nbsendB;
		lrc->lrc_stat.lrc_nbsend	= lk->lk_stat.st_nbsendu
						+  lk->lk_stat.st_nbsends
						+  lk->lk_stat.st_nbsendi;

		lrc->lrc_stat.lrc_nbrecvB	= lk->lk_stat.st_nbrecvB;
		lrc->lrc_stat.lrc_nbrecv	= lk->lk_stat.st_nbrecvu
						+  lk->lk_stat.st_nbrecvs
						+  lk->lk_stat.st_nbrecvi;
		lrc->lrc_stat.lrc_nbdrop	= LrcNbPktDrop;

		wanlrc.wl_LrcCount		= NbLrc;
		wanlrc.wl_LrcIndex		= i;
		wanlrc.wl_LrcState		= state;
		wanlrc.wl_LrcNbPktTrip		= nbelem;
		wanlrc.wl_LrcAvPktTrip		= lrc->lrc_avdvtrip.ad_aver;
		wanlrc.wl_LrcDvPktTrip		= lrc->lrc_avdvtrip.ad_sdev;
		wanlrc.wl_LrcMxPktTrip		= lrc->lrc_avdvtrip.ad_vmax;
		wanlrc.wl_LrcMxPktTripTime	= lrc->lrc_avdvtrip.ad_tmax;

		wanlrc.wl_LrcNbIecUpByte	= ABS(lrc->lrc_stat.lrc_nbsendB
						- lrc->lrc_stat_p.lrc_nbsendB);
		wanlrc.wl_LrcNbIecUpPacket	= ABS(lrc->lrc_stat.lrc_nbsend
						- lrc->lrc_stat_p.lrc_nbsend);
		wanlrc.wl_LrcNbIecDownByte	= ABS(lrc->lrc_stat.lrc_nbrecvB
						- lrc->lrc_stat_p.lrc_nbrecvB);
		wanlrc.wl_LrcNbIecDownPacket	= ABS(lrc->lrc_stat.lrc_nbrecv
						- lrc->lrc_stat_p.lrc_nbrecv);
		wanlrc.wl_LrcNbDisc		= ABS(lrc->lrc_stat.lrc_nbdisc
						- lrc->lrc_stat_p.lrc_nbdisc);
		deltadrop			= ABS(lrc->lrc_stat.lrc_nbdrop
						- lrc->lrc_stat_p.lrc_nbdrop);

		memcpy	(&lrc->lrc_stat_p,&lrc->lrc_stat,sizeof(lrc->lrc_stat));

		szm	= 0;
		data	= NULL;
		szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_wan_lrc);
		memset	(&uppkt,0,sizeof(uppkt));
		uppkt.lp_vers	= LP_LRX_VERSION;
		uppkt.lp_szh	= szh;
		uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
		uppkt.lp_type	= LP_TYPE_LRR_INF_WAN_LRC;
		uppkt.lp_lrrid	= LrrID;

		memcpy	(&uppkt.lp_u.lp_wan_lrc,&wanlrc,sizeof(t_lrr_wan_lrc));
		memcpy	(buff,&uppkt,szh);
		if	(szm > 0)
			memcpy	(buff+szh,data,szm);

		SendStatToAllLrc(buff,szh+szm,delay);
	}

	memset	(&wan,0,sizeof(wan));
	szm	= 0;
	data	= NULL;
	szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_wan);
	memset	(&uppkt,0,sizeof(uppkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_szh	= szh;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_type	= LP_TYPE_LRR_INF_WAN;
	uppkt.lp_lrrid	= LrrID;

	wan.wl_nbupdate		= ++WanNbUpdate;
	wan.wl_wanrefresh	= WanRefresh;
	wan.wl_gssupdate	= (time_t)Currtime.tv_sec;
	wan.wl_gnsupdate	= (u_int)Currtime.tv_nsec;

	wan.wl_LrcCount		= nblrc;
	wan.wl_LrcNbPktDrop	= deltadrop;
	wan.wl_ItfCount 	= 0;
	for	(i = 0; i < NB_ITF_PER_LRR ; i++)
	{
		t_wan_itf	*itf;
		u_short		sent;
		u_short		lost;
		u_short		okay;

		itf	= &TbItf[i];
		if	(!itf)			continue;
		wan.wl_ItfCount++;
		wan.wl_ItfIndex[i]	= i;
		wan.wl_ItfType[i]	= itf->it_type;
		wan.wl_ItfState[i]	= StateItf(itf);

		wan.wl_ItfRssi[i]	= 0;
		wan.wl_ItfOper[i][0]	= '\0';
		if	(itf->it_type == 1)
		{	// TODO
		}

		if	(!itf->it_enable)	continue;
		if	(!itf->it_name)		continue;
//		if	(!itf->it_exists)	continue;

		wan.wl_ItfActivityTime[i]	= itf->it_deftime;
		wan.wl_ItfAddr[i]		= itf->it_ipv4;

		sent	= ABS(itf->it_sentprtt - itf->it_sentprtt_p);
		lost	= ABS(itf->it_lostprtt - itf->it_lostprtt_p);
		okay	= ABS(itf->it_okayprtt - itf->it_okayprtt_p);
		if	(sent > 0 && okay)
		{
			wan.wl_ItfNbPktTrip[i]		= (u_short)sent;
			wan.wl_ItfLsPktTrip[i]		= (u_short)lost;
			wan.wl_ItfAvPktTrip[i]		= 
					(u_short)itf->it_avdvprtt.ad_aver;
			wan.wl_ItfDvPktTrip[i]		= 
					(u_short)itf->it_avdvprtt.ad_sdev;
			wan.wl_ItfMxPktTrip[i]		= 
					(u_short)itf->it_avdvprtt.ad_vmax;
			wan.wl_ItfMxPktTripTime[i]	= 
					(u_int)itf->it_avdvprtt.ad_tmax;
		}
		itf->it_sentprtt_p	= itf->it_sentprtt;
		itf->it_lostprtt_p	= itf->it_lostprtt;
		itf->it_okayprtt_p	= itf->it_okayprtt;

		wan.wl_ItfRxTraffic[i]		= itf->it_lgt.it_rxbytes_d;
		wan.wl_ItfRxAvgRate[i]		= itf->it_lgt.it_rxbytes_br;
		wan.wl_ItfRxMaxRate[i]		= itf->it_lgt.it_rxbytes_mbr;
		wan.wl_ItfRxMaxRateTime[i]	= itf->it_lgt.it_rxbytes_mbrt;

		wan.wl_ItfTxTraffic[i]		= itf->it_lgt.it_txbytes_d;
		wan.wl_ItfTxAvgRate[i]		= itf->it_lgt.it_txbytes_br;
		wan.wl_ItfTxMaxRate[i]		= itf->it_lgt.it_txbytes_mbr;
		wan.wl_ItfTxMaxRateTime[i]	= itf->it_lgt.it_txbytes_mbrt;
	}


	//	FIX3323
	if	(StorePktCount > 0)
	{
		int	nb	= rtl_imsgCount(StoreQ);
		if	(nb > 0)
		{
			wan.wl_LtqCnt		= nb;
		}
		if	(MaxStorePktCount > 0)
		{
			wan.wl_LtqMxCnt		= MaxStorePktCount;
			wan.wl_LtqMxCntTime	= MaxStorePktTime;
		}
		MaxStorePktCount= 0;
		MaxStorePktTime	= 0;
	}

	memcpy	(&uppkt.lp_u.lp_wan,&wan,sizeof(t_lrr_wan));
	memcpy	(buff,&uppkt,szh);
	if	(szm > 0)
		memcpy	(buff+szh,data,szm);

	SendStatToAllLrc(buff,szh+szm,delay);
}

static char * AutoRebootFile         = "usr/data/lrr/autorebootcnt.txt";
static char * AutoRebootFileNoUplink = "usr/data/lrr/autorebootcnt_nouplink.txt";
static char * AutoRestartLrrProcess  = "usr/data/lrr/autorestartlrrprocess.txt";

static  void AutoRebootGetFile(int cause, char (* file)[])
{
	switch(cause) {
		case REBOOT_NO_UPLINK_RCV:
			sprintf(*file, "%s/%s", RootAct, AutoRebootFileNoUplink);
			break;
		case REBOOT_NO_LRC_COMM:
		default:
			sprintf(*file, "%s/%s", RootAct, AutoRebootFile);
			break;
	}
}

static	void	AutoRebootReset(int cause)
{
	char	file[1024];
	AutoRebootGetFile(cause, &file);
	unlink	(file);
}

static	unsigned int	AutoRebootInc(int cause)
{
	int	inc	= 0;
	char	file[1024];
	FILE	*f;

	AutoRebootGetFile(cause, &file);
	f	= fopen(file,"r");
	if	(f)
	{
		fscanf	(f,"%u",&inc);
		fclose	(f);
	}
	inc++;
	f	= fopen(file,"w");
	if	(f)
	{
		fprintf	(f,"%u",inc);
		fclose	(f);
	}
	return	inc;
}

static void AutoRestartLrrProcessReset(void)
{
	char file[1024];
	sprintf(file, "%s/%s", RootAct, AutoRestartLrrProcess);
	unlink(file);
}

static unsigned int AutoRestartLrrProcessInc(void)
{
	int inc = 0;
	char file[1024];
	FILE * f = NULL;
	sprintf(file, "%s/%s", RootAct, AutoRestartLrrProcess);
	f = fopen(file, "r");
	if (f) {
		fscanf(f, "%u", &inc);
		fclose(f);
	}
	inc++;
	f = fopen(file, "w");
	if (f) {
		fprintf(f, "%u", inc);
		fclose(f);
	}
	return inc;
}


static	void	NeedRevSsh(int nblrcok)
{
	static	int	revssh	= 0;
	static	time_t	tnocnx	= 0;
	time_t		now;
	int		tm	= AutoRevSshTimer;
	char		cmdStart[1024];
	char		cmdStop[1024];
	int		port;

	port	= 50000 + (LrrID % 10000);
	sprintf	(cmdStop,"./shelllrr.sh closessh -Z 0 -P %d 2>&1 > /dev/null",
					port);
	sprintf	(cmdStart,"./shelllrr.sh openssh -Z 0 -a -K -P %d 2>&1 > /dev/null",
					port);

	if	(nblrcok > 0)
	{
		tnocnx	= 0;
		if	(revssh)
		{
			RTL_TRDBG(0,"LRC connection ok => kill revssh\n");
			revssh	= 0;
			DoSystemCmdBackGround(cmdStop);
		}
		return;
	}
	if	(tnocnx == 0)
	{
		tnocnx	= rtl_timemono(NULL);
		return;
	}
	now	= rtl_timemono(NULL);
	if	(ABS(now-tnocnx) <= tm)
	{
		return;
	}
	if	(revssh)
	{
		revssh++;
		if	(revssh <= 10)
		{
			return;
		}
		RTL_TRDBG(0,"LRC connection ko => kill&start revssh\n");
		revssh	= 0;
		DoSystemCmdBackGround(cmdStop);
		sleep(3);
	}
	// no LRC connection during more than N sec
	RTL_TRDBG(0,"no LRC connection during more than %dsec => revssh\n",tm);
	revssh	= 1;

	DoSystemCmdBackGround(cmdStart);
}


static	void	NeedReboot(int nblrcok)
{
	static	time_t	tnocnx	= 0;
	time_t		now;
	int		tm	= AutoRebootTimer;
	unsigned int	autorebootcnt;
	char		cmd[1024];
	FILE		*f;

	if	(nblrcok > 0)
	{
		AutoRebootReset(REBOOT_NO_LRC_COMM);
		tnocnx	= 0;
		return;
	}
	if	(tnocnx == 0)
	{
		tnocnx	= rtl_timemono(NULL);
		return;
	}
	now	= rtl_timemono(NULL);
	if	(ABS(now-tnocnx) <= tm)
	{
		return;
	}
	// no LRC connection during more than N sec
	autorebootcnt	= AutoRebootInc(REBOOT_NO_LRC_COMM);
	RTL_TRDBG(0,"no LRC connection during more than %dsec => reboot\n",tm);
	RTL_TRDBG(0,"auto reboot counter %d\n",autorebootcnt);

#if defined(WIRMAV2) || defined(TEKTELIC)
	if	(autorebootcnt > 12)
	{
		AutoRebootReset(REBOOT_NO_LRC_COMM);
		sprintf(cmd,"%s/lrr/com/cmd_shells/%s/execrff.sh -L %x08x",
				RootAct,System,LrrID);
		RTL_TRDBG(0,"too much auto reboot => execrff.sh\n");
		RTL_TRDBG(0,"%s\n",cmd);
		system(cmd);
	}
#endif

	sprintf(cmd,"%s/usr/etc/lrr/autoreboot_last",RootAct);
	unlink(cmd);
	f	= fopen(cmd,"w");
	if	(f)
	{
		fprintf(f, "No LRC connection during more than %d s => reboot #%d\n", tm, autorebootcnt);
		fclose(f);
	}
	system("reboot");
}

static void NeedRebootNoUplink(int nbpktuplink)
{
	static int     nbpktuplink_last = 0;
	unsigned int   autorestartcnt = 0;
	unsigned int   autorebootcnt = 0;
	static time_t  tnocnx	= 0;
	time_t         now;
	int            tm = AutoRebootTimerNoUplink;
	int            cnt_max = AutoRestartLrrMaxNoUplink;
	char           cmd[1024];
	FILE         * f;

	if ((nbpktuplink > 0) && (nbpktuplink - nbpktuplink_last > 0))
	{
		AutoRebootReset(REBOOT_NO_UPLINK_RCV);
		AutoRestartLrrProcessReset();
		tnocnx = 0;
		nbpktuplink_last = nbpktuplink;
		return;
	}
	if (tnocnx == 0)
	{
		RTL_TRDBG(2, "NeedRebootNoUplink: no uplink received since the last function call. Start timer\n");
		tnocnx = rtl_timemono(NULL);
		return;
	}
	now	= rtl_timemono(NULL);
	RTL_TRDBG(2, "NeedRebootNoUplink: no uplink received since %u s\n", (unsigned int)(ABS(now-tnocnx)));
	if (ABS(now-tnocnx) < tm)
	{
		return;
	}

	autorestartcnt = AutoRestartLrrProcessInc();
	if (autorestartcnt <= cnt_max)
	{
		RTL_TRDBG(0, "NeedRebootNoUplink: no radio uplink packet received during %d s or more => restart (%d / %d)\n", tm, autorestartcnt, cnt_max);
		sprintf(cmd, "%s/usr/etc/lrr/autorestart_last", RootAct);
		unlink(cmd);
		f = fopen(cmd, "w");
		if (f)
		{
			fprintf(f, "No radio uplink packet received during %d s => restart (%d / %d)\n", tm, autorestartcnt, cnt_max);
			fclose(f);
		}
		ServiceStop(0);
		return;
	}
	AutoRestartLrrProcessReset();
	autorebootcnt = AutoRebootInc(REBOOT_NO_UPLINK_RCV);
	RTL_TRDBG(0, "NeedRebootNoUplink: no radio uplink packet received during %d s or more => reboot (#%d)\n", tm, autorebootcnt);
	sprintf(cmd, "%s/usr/etc/lrr/autoreboot_last", RootAct);
	unlink(cmd);
	f = fopen(cmd, "w");
	if (f)
	{
		fprintf(f, "No radio uplink packet received => reboot #%d\n", autorebootcnt);
		fclose(f);
	}
	system("reboot");
}

/* Number of values used to calculate the GPS status weighted moving average */
#define GPS_STATUS_WMA_NB 3
#ifdef WITH_GPS
static int CheckGpsStatus()
{
	static float 	srate[GPS_STATUS_WMA_NB] = {1.0, 1.0, 1.0};
	float 		srate_wma = 0.0;
	int 		i = 0;
	int 		ret = 0;
	u_short		lptype;
	u_int 		cnt = GpsUpdateCnt;
	static u_int 	cnt_prev = 0;

	if (    (!cnt) 				/* if GPS is not working at all */
	     || (cnt >= GpsStRefresh)		/* Avoid first 30 NMEA frames */
	     || (cnt == cnt_prev))		/* if GPS starts working but fall down before the first 30 NMEA frames */
	{	 
		for (i = GPS_STATUS_WMA_NB - 1; i > 0; i--)
			srate[i] = srate[i-1];
		srate[0] = (float)(cnt - cnt_prev) / (float)GpsStRefresh;
		RTL_TRDBG(4, "GPS Status cnt=%u/%d (srate=%.2f)\n", cnt - cnt_prev, GpsStRefresh, srate[0]);
	}

	/* Calcul de la moyenne mobile pondérée */
	/* Weighted moving average */
	for (i=0; i < GPS_STATUS_WMA_NB; i++)
		srate_wma += srate[i] * (GPS_STATUS_WMA_NB - i);
	srate_wma /= GPS_STATUS_WMA_NB * (GPS_STATUS_WMA_NB + 1) / 2;
	cnt_prev = cnt;
	switch (GpsStatus) {
		default:
			GpsStatus = 'U';
		case 'U':
			if (srate_wma < GpsStRateLow) {
				GpsStatus = 'D';
				ret = 1;
				GpsDownCnt++;
				lptype = LP_TYPE_LRR_INF_GPS_DOWN;
			}
			break;
		case 'D':
			if (srate_wma > GpsStRateHigh) {
				GpsStatus = 'U';
				ret = 1;
				GpsUpCnt++;
				lptype = LP_TYPE_LRR_INF_GPS_UP;
			}
			break;
	}
	RTL_TRDBG(4, "GPS Status %c (srate_wma=%.2f ,sr0=%.2f, sr1=%.2f, sr2=%.2f)\n", GpsStatus, \
		srate_wma, srate[0], srate[1], srate[2]);
	if (ret > 0) {
		DoGpsRefresh(0, lptype, GpsStatus, srate_wma);
	}
	return ret;
}
#endif /* WITH_GPS */

static	int	Power()
{
	FILE		*f		= NULL;
	int		ret;
	int		value;
	u_short		lptype;

	if	(PowerEnable ==0)
		return	0;

	f	= fopen(PowerDevice,"r");
	if	(!f)
	{
		RTL_TRDBG(0,"cannot open '%s' => disable\n",PowerDevice);
		PowerEnable	= 0;
		return	0;
	}

	ret	= fscanf(f,"%d",&value);
	fclose	(f);
	if	(ret != 1)
	{
		RTL_TRDBG(0,"cannot read '%s' => disable\n",PowerDevice);
		PowerEnable	= 0;
		return	0;
	}

//RTL_TRDBG(1,"power read power=%d state=%c\n",value,PowerState);
	ret	= 0;
	switch	(PowerState)
	{
	default:
		PowerState	= 'U';
	case	'U':
		if	(value < PowerDownLev)
		{
			PowerState	= 'D';
			ret		= 1;
			lptype		= LP_TYPE_LRR_INF_POWER_DOWN;
		}
	break;
	case	'D':
		if	(value > PowerUpLev)
		{
			PowerState	= 'U';
			ret		= 1;
			lptype		= LP_TYPE_LRR_INF_POWER_UP;
			PowerDownCnt++;
		}
	break;
	}
	if	(ret > 0)
	{
		float	volt	= 0.0;

#ifdef	WIRMAV2
		if	(value > 1023)
			value	= 1023;
		volt	= (float)value;
		volt	= (volt * 456.0 / 10230.0) + 0.4;
#endif
		DoPowerRefresh(0,lptype,PowerState,(u_int)value,volt);
	}

	return	ret;
}

static	float	ConvTemperature(int X)
{
float	t;
t = (float)(((209310.0 - (float)((285000.0 * (X&0x3FF)) / 1023.0))) / 1083.0);
return	t;
}

static	int	Temperature()
{
	FILE		*f		= NULL;
	int		ret;
	int		value;
	float		fvalue;

	if	(TempEnable ==0)
		return	0;

	f	= fopen(TempDevice,"r");
	if	(!f)
	{
		RTL_TRDBG(0,"cannot open '%s' => disable\n",TempDevice);
		TempEnable	= 0;
		return	0;
	}

	ret	= fscanf(f,"%d",&value);
	fclose	(f);
	if	(ret != 1)
	{
		RTL_TRDBG(0,"cannot read '%s' => disable\n",TempDevice);
		TempEnable	= 0;
		return	0;
	}
	fvalue		= ConvTemperature(value);
	CurrTemp	= roundf(fvalue);

RTL_TRDBG(3,"temperature read raw=%d temp=%f (%d)\n",value,fvalue,CurrTemp);
	return	ret;
}

#define	PERIODICITY(cnt,per) ( ((per) && ((cnt) % (per)) == 0) ? 1 : 0 )
#define	TPWADELAY	10000

static	void	DoClockSc()
{
	static	unsigned	int	nbclock		= 0;
	static	unsigned	int	refreshclock	= 0;
	static			int	startedonce	= 0;
				int	tpwadelay	= TPWADELAY;
				int	nblrcok;
				int	ret;
#if	0
	u_char		buff[1024];
	t_lrr_pkt	uppkt;
	int		szh	= LP_HEADER_PKT_SIZE_V0;
	int		szm;
	char		*data	= "bonjour";
#endif

	clock_gettime(CLOCK_REALTIME,&Currtime);

	if	(LapTest)
	{
		goto	laponly;
	}

	ret	= Power();
	if	(ret)
	{
		RTL_TRDBG(0,"enter power state %c\n",PowerState);
		// TODO send a message to LRCs
		goto	laponly;
	}

	if	(access(ExtLrrRestart,R_OK) == 0)
	{
		RTL_TRDBG(1,"lrr restart '%s'\n",ExtLrrRestart);
		unlink	(ExtLrrRestart);
		exit(0);
		return;
	}
	if	(access(ExtRadioRestart,R_OK) == 0)
	{
		RTL_TRDBG(1,"lrr radiorestart '%s'\n",ExtRadioRestart);
		unlink	(ExtRadioRestart);
		DownRadioStop		= 0;
		LgwThreadStopped	= 0;
		ReStartLgwThread();
		return;
	}

	CompAllItfInfos('S');
	CompAllCpuInfos('S');
	CompAllMemInfos('S');

	if	(startedonce == 0)
		startedonce	= LrcOkStarted();

	if	(startedonce)
	{
		ret	= Power();
		if	(ret)
		{
			RTL_TRDBG(0,"enter power state %c\n",PowerState);
			// TODO send a message to LRCs
			goto	laponly;
		}
	}

	tpwadelay = TPWADELAY;
	if	(startedonce && PERIODICITY(refreshclock,StatRefresh))
	{
		RTL_TRDBG(1,"DoStatRefresh\n");
		DoStatRefresh(tpwadelay);
		tpwadelay += TPWADELAY;
	}

	if	(startedonce && PERIODICITY(refreshclock,ConfigRefresh))
	{
		RTL_TRDBG(1,"DoConfigRefresh\n");
		DoConfigRefresh(tpwadelay);
		tpwadelay += TPWADELAY;
	}

	if	(startedonce && PERIODICITY(refreshclock,WanRefresh))
	{
		CompAllItfInfos('L');
		RTL_TRDBG(1,"DoWanRefresh\n");
		DoWanRefresh(tpwadelay);
		tpwadelay += TPWADELAY;
	}

	if	(startedonce && PERIODICITY(refreshclock,RfCellRefresh))
	{
		RTL_TRDBG(1,"DoRfcellRefresh\n");
		DoRfcellRefresh(tpwadelay);
		tpwadelay += TPWADELAY;
	}
	
#ifdef WITH_GPS
	if (startedonce && PERIODICITY(refreshclock, GpsStRefresh))
	{
		ret = CheckGpsStatus();
		if (ret) {
			RTL_TRDBG(1, "GPS Status changed to %c\n", GpsStatus);
		}
	}
#endif

	if	(startedonce)
	{
		refreshclock++;
	}

	nbclock++;
	if	((nbclock % 30) == 0)
	{
#ifdef TEKTELIC
		GetGpsPositionTektelic();
#endif
		nblrcok	= LrcOkCount();
		if	(AutoRebootTimer > 0)
		{
			NeedReboot(nblrcok);
		}
		if	(AutoRebootTimerNoUplink > 0)
		{
			NeedRebootNoUplink(LgwNbPacketRecv);
		}
		if	(AutoRevSshTimer > 0)
		{
			NeedRevSsh(nblrcok);
		}
	}
	if	((nbclock % 30) == 0)
	{
		Temperature();
	}


/* Not needed since gps is managed by a separated thread */
/*
#ifdef	WITH_GPS
	if	((nbclock % 30) == 0)
	{
		InitGps();
	}
#endif
*/
	laponly :
	if	(NbLrc <= 1)
	{
		LapDoClockSc();
	}
	else
	{	// period of TCP reconnect depends of number of LRC connected
		int	lrc;
		lrc	= LrcOkCount();
		LapDoClockScPeriod(3 + (lrc*10));
	}

}

static	void	DoClockHh()
{
	static	unsigned int nbclock = 0;
	char	exe[1024];
	char	cmd[1024];
	char	trace[1024];
	int	dayopen;


	RTL_TRDBG(1,"DoClockHh()\n");

	// backup traces
	dayopen	= rtl_tracedayopen();
#if	defined(WIRMAAR) || defined(WIRMANA)
	if	(dayopen != -1 && LogUseRamDir)
#else
	if	(dayopen != -1)
#endif
	{
		sprintf	(exe,"%s/lrr/com/shells/%s/",RootAct,System);
		strcat	(exe,"backuptraces.sh");
		if	(access(exe,X_OK) == 0)
		{
			strcpy	(trace,"/dev/null");
			sprintf	(cmd,"%s %d 2>&1 > %s &",exe,dayopen,trace);
			DoSystemCmdBackGround(cmd);
			RTL_TRDBG(1,"backuptraces started day=%d\n",dayopen);
		}
		else
		{
			RTL_TRDBG(1,"backuptraces does not exist\n");
		}
	}
	nbclock++;
}

int	ClearStoreQ()
{
	int		nb	= 0;
	t_imsg		*msg;
	t_lrr_pkt	*uppkt;

	while	((msg = rtl_imsgGet(StoreQ,IMSG_MSG)))
	{
		uppkt	= (t_lrr_pkt *)msg->im_dataptr;
		if	(uppkt->lp_payload)
			free(uppkt->lp_payload);
		uppkt->lp_payload	= NULL;
		rtl_imsgFree(msg);
		nb++;
	}
	RTL_TRDBG(1,"clear stored messages nb=%d\n",nb);
	return	nb;
}

static	void	RunStoreQMs()	// function called 10 times per sec
{
	static	u_int	count;
	int		nb;
	t_imsg		*msg;
	t_lrr_pkt	*uppkt;

	count++;

	if	(StorePktCount <= 0)
		return;
	if	(StoreQ == NULL)
		return;
	// NFR997: Master lrc not yet identified
	if	(LrrIDGetFromBS && MasterLrc < 0)
		return;

//	count % (ceil(10.0 / (float)ReStorePerSec))

	switch	(ReStorePerSec)
	{
	case	10:	// each time no control
	break;
	case	5:
		if	( (count % 2) != 0)
			return;
	break;
	case	3:
		if	( (count % 3) != 0)
			return;
	break;
	case	2:
		if	( (count % 5) != 0)
			return;
	break;
	case	1:
		if	( (count % 10) != 0)
			return;
	break;
	default:
		ReStorePerSec	= 1;
	break;
	}

	nb	= rtl_imsgCount(StoreQ);
	if	(nb <= 0)
		return;

	//	FIX3323
	if	(nb > MaxStorePktCount)
	{
		MaxStorePktCount= nb;
		MaxStorePktTime	= (time_t)Currtime.tv_sec;
	}

	if	(LrcOkStartedTestLoad(ReStoreCtrlOutQ,ReStoreCtrlAckQ) <= 0)
		return;

	msg	= rtl_imsgGet(StoreQ,IMSG_MSG);
	if	(!msg)
		return;

	uppkt	= (t_lrr_pkt *)msg->im_dataptr;
	RecvRadioPacket(uppkt,1);
	if	(uppkt->lp_payload)
		free(uppkt->lp_payload);
	uppkt->lp_payload	= NULL;
	rtl_imsgFree(msg);
	nb	= rtl_imsgCount(StoreQ);
	RTL_TRDBG(1,"unstore message nb=%d\n",nb);
}

static	int	RecvRadioPacketStore(t_imsg *msg,t_lrr_pkt *uppkt)
{
	static	u_int	localdrop;	// TODO
	int	nb;
	float	memused	= 0.0;

	if	(StorePktCount <= 0)
		return	0;
	if	(StoreQ == NULL)
		return	0;

	nb	= rtl_imsgCount(StoreQ);
	if	(!msg || !uppkt)
		return	nb;

	if	(MemTotal)
	memused	= ((float)(MemUsed-MemBuffers-MemCached)/(float)MemTotal)*100.0;

	uppkt->lp_type	= LP_TYPE_LRR_PKT_RADIO_LATE;
	uppkt->lp_flag	= uppkt->lp_flag | LP_RADIO_PKT_LATE;
	if	( (nb >= StorePktCount) || (memused && memused > StoreMemUsed))
	{
		t_imsg		*drop;
		t_lrr_pkt	*dppkt;

		drop	= rtl_imsgGet(StoreQ,IMSG_MSG);
		if	(drop)
		{
			dppkt	= (t_lrr_pkt *)drop->im_dataptr;
			if	(dppkt->lp_payload)
				free(dppkt->lp_payload);
			dppkt->lp_payload	= NULL;
			rtl_imsgFree(drop);
			localdrop++;
		}
	}
	rtl_imsgAdd(StoreQ,msg);
	nb	= rtl_imsgCount(StoreQ);

	RTL_TRDBG(1,"store message nb=%d max=%d drop=%u memused=%f\n",
				nb,StorePktCount,localdrop,memused);

	return	nb;
}

static	int	RecvRadioPacket(t_lrr_pkt *uppkt,int late)
{
	u_char	buff[1024];
	int	szh	= LP_HEADER_PKT_SIZE_V0;
	int	szm	= uppkt->lp_size;
	u_char	*data	= uppkt->lp_payload;
	u16	crcccitt= 0;
	u16	crcnode = 0;

	if	(late)
		goto	lateonly;

	if	(MacWithFcsUp && szm >= 2)
	{
		crcnode	= data[szm-2] + (data[szm-1] << 8);
		crcccitt= crc_ccitt(0,data,szm-2);
	}

	RTL_TRDBG(2,"MAC RECV sz=%d crcccitt=0x%04x crcnode=0x%04x\n",
			szm,crcccitt,crcnode);

	if	(crcnode != crcccitt)
	{
		MacNbFcsError++;
		return	1;		// drop
	}
	if	(MacWithFcsUp)
		szm	= szm - 2;	// suppress FCS

RTL_TRDBG(2,"MAC RECV\n");
	if	(TraceLevel > 3)
	{
		rtl_binToStr(data,szm,(char *)buff,sizeof(buff)-10);
		RTL_TRDBG(4,"MAC RECV\n<<<'%s'\n",buff);
	}

lateonly:
	szh		= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_pkt_radio);
	uppkt->lp_vers	= LP_LRX_VERSION;
	uppkt->lp_flag	|= LP_RADIO_PKT_UP;	// keeps flags comming from lgw
	uppkt->lp_lrrid	= LrrID;
	uppkt->lp_type	= LP_TYPE_LRR_PKT_RADIO;
	uppkt->lp_szh	= szh;
	if	(late)
	{
		uppkt->lp_type	= LP_TYPE_LRR_PKT_RADIO_LATE;
		uppkt->lp_flag	= uppkt->lp_flag | LP_RADIO_PKT_LATE;
	}

	memcpy	(buff,uppkt,szh);
	memcpy	(buff+szh,data,szm);

	return	SendToLrc(NULL,buff,szh+szm);
}

static	void	DoInternalEvent(t_imsg *imsg,int *freemsg)
{
	static	int	startFailure;

	*freemsg	= 1;
	RTL_TRDBG(9,"receive event cl=%d ty=%d\n",imsg->im_class,imsg->im_type);
	switch(imsg->im_class)
	{
	case	IM_DEF :
		switch(imsg->im_type)
		{
		case	IM_SERVICE_STATUS_RQST :
			ServiceStatusResponse();
		break;
		case	IM_LGW_RECV_DATA :	// uplink packet from radio
		{
			t_lrr_pkt	*uppkt;
			int		ret;

			uppkt	= (t_lrr_pkt *)imsg->im_dataptr;
			// NFR997: store messages until MasterLrc is identified
			if (LrrIDGetFromBS && MasterLrc == -1)
			{
				RTL_TRDBG(3,"Master lrc not yet identified, message will be stored\n");
				ret = 0;
			}
			else
				ret	= RecvRadioPacket(uppkt,0);
			if	(ret == 0)
			{
				*freemsg	= 0;
				RecvRadioPacketStore(imsg,uppkt);
				break;
			}
			if	(uppkt->lp_payload)
				free(uppkt->lp_payload);
			uppkt->lp_payload	= NULL;
		}
		break;
		case	IM_LGW_SENT_INDIC :
		{
			t_lrr_pkt	*uppkt;

			uppkt	= (t_lrr_pkt *)imsg->im_dataptr;
			SendIndicToLrc(uppkt);
		}
		break;
		case	IM_LGW_POST_DATA :	// postponed downlink packet
		{
			t_lrr_pkt	*dnpkt;

			dnpkt	= (t_lrr_pkt *)imsg->im_dataptr;
			SendRadioPacket(dnpkt,NULL,0);
		}
		break;
		case	IM_LGW_DELAY_ALLLRC  :	// delayed stats to all lrc
		{
			u_char	*buff;
			int	sz;

			buff	= (u_char *)imsg->im_dataptr;
			sz	= imsg->im_datasz;
			SendStatToAllLrc(buff,sz,0);
			RTL_TRDBG(3,"SendStatToAllLrc() retrieved\n");
		}
		break;
		case	IM_CMD_RECV_DATA :	// uplink data from commands
		{
			t_lrr_pkt	*resppkt;
			int		sz;
			t_xlap_link	*lk;
			u_char		buff[1024];

			resppkt	= (t_lrr_pkt *)imsg->im_dataptr;
			sz	= imsg->im_datasz;
			lk	= (t_xlap_link *)resppkt->lp_lk;
			RTL_TRDBG(3,"Retrieve command data lk=%p sz=%d\n",
							lk,sz);

			memcpy	(buff,resppkt,resppkt->lp_szh);
			LapPutOutQueue(lk,(u_char *)buff,resppkt->lp_szh);
		}
		break;
		case	IM_LGW_CONFIG_FAILURE :
			if	(OnConfigErrorExit)
			{
				ServiceStop(0);
				exit(1);
			}
			ReStartLgwThread();
		break;
		case	IM_LGW_START_FAILURE :
		{
			char	cmd[1024];

			if	(OnStartErrorExit)
			{
				ServiceStop(0);
				exit(1);
			}
			startFailure++;
			if	(startFailure < 10)
			{	// just restart the radio thread
				ReStartLgwThread();
				break;
			}
			// try to power off on the radio board
			// and restart the radio thread
			startFailure	= 0;
			sprintf	(cmd,"%s/lrr/com/cmd_shells/%s/",
								RootAct,System);
			strcat	(cmd,"radiopower.sh");
			if	(access(cmd,X_OK) != 0)
			{
			RTL_TRDBG(0,"RADIO power on/off not provided on '%s'\n",
									System);
				ReStartLgwThread();
				break;
			}
			strcat	(cmd," 2>&1 > /dev/null &");
			DoSystemCmdBackGround(cmd);
			RTL_TRDBG(0,"RADIO power on/off\n");
			ReStartLgwThread();
		}
		break;
		case	IM_LGW_STARTED :
			startFailure	= 0;
			if	(CfgRadioDnStop)
			{
				DownRadioStop	= 1;
				RTL_TRDBG(0,"RADIO downlink stopped by config\n");
			}
		break;
		case	IM_LGW_LINK_DOWN :
			ReStartLgwThread();
		break;
		}
	break;
	}
}

static	void	DoInternalTimer(t_imsg *imsg)
{
	u_int	szmalloc;
#ifndef	MACOSX
	struct	mallinfo info;
	info	= mallinfo();
	szmalloc= info.uordblks;
#else
	szmalloc= 0;
#endif

	RTL_TRDBG(3,"receive timer cl=%d ty=%d vsize=%ld alloc=%u\n",
		imsg->im_class,imsg->im_type,rtl_vsize(getpid()),szmalloc);

	switch(imsg->im_class)
	{
	case	IM_DEF :
		switch(imsg->im_type)
		{
		case	IM_TIMER_GEN :
		rtl_imsgAdd(MainQ,
		rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,IM_TIMER_GEN_V,NULL,0));
		break;
		case	IM_TIMER_LRRUID_RESP :	// NFR684
			if (LrrIDGetFromTwa == 1)
			{
				RTL_TRDBG(2,"Failed to get lrrid from twa => lrrid '%08x' used\n", LrrID);
				LrrIDGetFromTwa = 0;
			}
			if (LrrIDGetFromTwa == 2)
			{
				RTL_TRDBG(2,"Failed to get lrrid from twa => retry later\n");
				ServiceStop(0);
				exit(0);
			}
		break;
		}
	break;
	}
}

static	void	MainLoop()
{
	time_t	lasttimems	= 0;
	time_t	lasttimesc	= 0;
	time_t	lasttimehh	= 0;
	time_t	now		= 0;
	int	ret;
	int	freemsg;

	t_imsg	*msg;


	unlink	(ExtLrrRestart);
	unlink	(ExtRadioRestart);

	rtl_imsgAdd(MainQ,
	rtl_timerAlloc(IM_DEF,IM_TIMER_GEN,IM_TIMER_GEN_V,NULL,0));

	while	(1)
	{
		// internal events
		while ((msg= rtl_imsgGet(MainQ,IMSG_MSG)) != NULL)
		{
			freemsg	= 1;
			DoInternalEvent(msg,&freemsg);
			if	(freemsg)
				rtl_imsgFree(msg);
		}

		// external events
		ret	= rtl_poll(MainTbPoll,MSTIC);
		if	(ret < 0)
		{
		}

		// clocks
		now	= rtl_tmmsmono();
		if	(ABS(now-lasttimems) >= 100)
		{
			RunStoreQMs();
			DoClockMs();
			lasttimems	= now;
		}
		if	(ABS(now-lasttimesc) >= 1000)
		{
			if	(MainWantStop && --MainStopDelay <= 0)
			{
				RTL_TRDBG(0,"auto stopped\n");
				break;
			}
			if	(LapTest && --LapTestDuration <= 0)
			{
				RTL_TRDBG(0,"lap test auto stopped\n");
				break;
			}
			DoClockSc();
			lasttimesc	= now;
		}
		if	(ABS(now-lasttimehh) >= 3600*1000)
		{
			DoClockHh();
			lasttimehh	= now;
		}

		// internal timer
		while ((msg= rtl_imsgGet(MainQ,IMSG_TIMER)) != NULL)
		{
			DoInternalTimer(msg);
			rtl_imsgFree(msg);
		}
	}
}

static	void	SigHandler(int sig)
{
	switch(sig)
	{
	case	SIGTERM:
		ServiceStop(sig);
	break;
	case	SIGUSR1:
		ServiceStatus(sig);
	break;
	case	SIGUSR2:
		ReStartLgwThread();
	break;
	}
}

#ifdef	WITH_GPS
void	GpsPositionUpdated(LGW_COORD_T *loc_from_gps)
{
	t_xlap_link	*lk;
	t_lrr_pkt	uppkt;
	int		i;

	RTL_TRDBG(1,"GPS update position (%f,%f) -> (%f,%f)\n",
			LrrLat,LrrLon,
			loc_from_gps->lat,loc_from_gps->lon);

	memset	(&uppkt,0,sizeof(t_lrr_pkt));
	uppkt.lp_vers	= LP_LRX_VERSION;
	uppkt.lp_flag	= LP_INFRA_PKT_INFRA;
	uppkt.lp_lrrid	= LrrID;
	uppkt.lp_type	= LP_TYPE_LRR_INF_GPSCO;
	uppkt.lp_szh	= LP_PRE_HEADER_PKT_SIZE+sizeof(t_lrr_gpsco);

	uppkt.lp_u.lp_gpsco.li_gps	= 2;
	uppkt.lp_u.lp_gpsco.li_latt	= loc_from_gps->lat;
	uppkt.lp_u.lp_gpsco.li_long	= loc_from_gps->lon;
	uppkt.lp_u.lp_gpsco.li_alti	= loc_from_gps->alt;

	for	(i = 0 ; i < NbLrc ; i++)
	{
		
		lk	= &TbLapLrc[i];
		if	(lk->lk_state == SSP_STARTED)
		{
			LapPutOutQueue(lk,(u_char *)&uppkt,uppkt.lp_szh);
		}
	}
}
#endif

#define	u_int_sizeof(X)	(unsigned int)sizeof((X))
void	StructSize()
{
t_lrr_pkt	pkt;

printf("LP_HEADER_PKT_SIZE_V0=%d old ...\n",LP_HEADER_PKT_SIZE_V0);

printf("LP_IEC104_SIZE_MAX=%d\n",LP_IEC104_SIZE_MAX);
printf("LP_PRE_HEADER_PKT_SIZE=%d\n",LP_PRE_HEADER_PKT_SIZE);
printf("LP_MACLORA_SIZE_MAX=%d (IEC104-PREHEADER-sizeof(lp_u.lp_radio)-10)\n",
				LP_MACLORA_SIZE_MAX);
printf("LP_DATA_CMD_LEN=%d (IEC104-PREHEADER-10)\n",
				LP_DATA_CMD_LEN);

printf("sizeof(t_lrr_pkt.lp_u)=%d\n",u_int_sizeof(pkt.lp_u));
printf("sizeof(t_lrr_pkt.lp_u.lp_radio)=%d\n",u_int_sizeof(pkt.lp_u.lp_radio));
#ifdef	LP_TP31
printf("sizeof(t_lrr_pkt.lp_u.lp_radio_p1)=%d\n",u_int_sizeof(pkt.lp_u.lp_radio_p1));
printf("sizeof(t_lrr_pkt.lp_u.lp_sent_indic)=%d\n",u_int_sizeof(pkt.lp_u.lp_sent_indic));
#endif
printf("sizeof(t_lrr_pkt.lp_u.lp_stat_v1)=%d\n",u_int_sizeof(pkt.lp_u.lp_stat_v1));
printf("sizeof(t_lrr_pkt.lp_u.lp_config)=%d\n",u_int_sizeof(pkt.lp_u.lp_config));
printf("sizeof(t_lrr_pkt.lp_u.lp_config_lrc)=%d\n",u_int_sizeof(pkt.lp_u.lp_config_lrc));
printf("sizeof(t_lrr_pkt.lp_u.lp_wan)=%d\n",u_int_sizeof(pkt.lp_u.lp_wan));
printf("sizeof(t_lrr_pkt.lp_u.lp_wan_lrc)=%d\n",u_int_sizeof(pkt.lp_u.lp_wan_lrc));
printf("sizeof(t_lrr_pkt.lp_u.lp_rfcell)=%d\n",u_int_sizeof(pkt.lp_u.lp_rfcell));

printf("sizeof(t_lrr_pkt.lp_u.lp_shell_cmd)=%d\n",u_int_sizeof(pkt.lp_u.lp_shell_cmd));
printf("sizeof(t_lrr_pkt.lp_u.lp_cmd_resp)=%d\n",u_int_sizeof(pkt.lp_u.lp_cmd_resp));

printf("max network part size of t_lrr_pkt=%d\n",(u_int)offsetof(t_lrr_pkt,lp_payload));
printf("sizeof(t_lrr_pkt)=%d (memory only)\n",u_int_sizeof(pkt));

printf("static data size of t_imsg=%d\n",IMSG_DATA_SZ);
}

int	NtpdStartedGeneric(char *file)
{
	int	ret	= 0;
	FILE	*f;

	if	(!file || !*file)
		return	0;
	f	= fopen(file,"r");
	if	(f)
	{
		pid_t	pid;

		pid	= 0;
		if	(fscanf(f,"%d",&pid) == 1 && pid > 0)
		{
			if	(kill(pid,0) == 0)
				ret	= 1;
		}
		fclose	(f);
	}

	return	ret;
}

int NtpdStartedCiscoCorsica(char *file)
{

 int ret = 0;
 char * buffer = NULL;
 long length;

 FILE *f;

 if (!file || !*file){
  return 0;
 }
 f = fopen(file,"r");
 if (f){
  fseek (f, 0, SEEK_END);
  length = ftell (f);
  fseek (f, 0, SEEK_SET);
  buffer = (char *)malloc(length * sizeof (char));
  if (buffer) {
   fread(buffer, 1, length, f);
  }
  fclose(f);
 }

 if (buffer) {
  if (strchr(buffer, '*')){
   ret = 1;
  } else {
   ret = 0;
  }
 }

 if (buffer) {
  free(buffer);
 }
 return ret;
}

#if	defined(WIRMAV2)

char	*UsbProtect	= "/etc/usbkey.txt";
char	*UsbProtectBkp	= "/etc/usbkey.txt.bkp";

static	void	UsbProtectBackup()
{
	char	cmd[512];

	sprintf	(cmd,"cp %s %s >/dev/null 2>&1 ",UsbProtect,UsbProtectBkp);
	system	(cmd);
}

static	void	UsbProtectRestore()
{
	char	cmd[512];

	sprintf	(cmd,"cp %s %s >/dev/null 2>&1",UsbProtectBkp,UsbProtect);
	system	(cmd);
}

static	void	UsbProtectCreate()
{
	FILE	*f;

	f	= fopen(UsbProtect,"w");
	if	(f)
	{
		// TODO use secret here
		fprintf	(f,"usb_auto_protect\n");
		fclose	(f);
	}
}

static	void	UsbProtectInit(int init)
{
	// try to restore file protect if exists
	UsbProtectRestore();
	if	(access(UsbProtect,R_OK) == 0)
	{	// file protect already exists, just backup it
		UsbProtectBackup();
		return;
	}
	// no file to protect automatic update with USB => create one
	UsbProtectCreate();
	UsbProtectBackup();
}

int	UsbProtectOff()
{
	UsbProtectBackup();
	unlink	(UsbProtect);
	return	0;
}

int	UsbProtectOn()
{
	UsbProtectInit(0);
	return	0;
}
#endif

#ifdef	WIRMAV2
#define	SYSTEM_DEFINED
void	InitSystem()
{

	System	= "wirmav2";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/mnt/fsuser-1/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
	UsbProtectInit(1);
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// WIRMAV2
}
#endif

#ifdef	WIRMAMS
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "wirmams";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/user/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// WIRMAMS
}
#endif

#ifdef	WIRMAAR
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "wirmaar";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/user/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// WIRMAAR
}
#endif

#ifdef	WIRMANA
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "wirmana";
	RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		RootAct	= "/user/actility";
		setenv	("ROOTACT",strdup(RootAct),1);
	}
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// WIRMANA
}
#endif

#if defined(NATRBPI_USB)
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "natrbpi_usb_v1.0";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// NATRBPI
}
#endif

#if defined(RBPI_V1_0)
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "rbpi_v1.0";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// NATRBPI
}
#endif

#ifdef	IR910
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "ir910";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntp/ntpd.pid");	// IR910
}
#endif

#ifdef	CISCOMS
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "ciscoms";
}

int	NtpdStarted()
{
	return	NtpdStartedCiscoCorsica("/var/run/ntp_status");
}
#endif

#if defined	(MTAC_V1_0)
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtac_v1.0";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntp.pid");			// MTAC CONDUIT SPI V1.0
}
#endif

#if defined	(MTAC_V1_5)
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtac_v1.5";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntp.pid");			// MTAC CONDUIT SPI V1.5
}
#endif


#if defined	(MTAC_REFRESH_V1_5)
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtac_refresh_v1.5";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntp.pid");			// MTAC CONDUIT REFRESH V1.5 (GPS)
}
#endif


#if defined (MTAC_USB)
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtac_usb_v1.0";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntp.pid");			// MTAC CONDUIT USB V1.0
}
#endif


#ifdef	MTCAP
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "mtcap";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntp.pid");			// MTCAP
}
#endif

#ifdef  FCMLB
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System  = "fcmlb";
}

int     NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// FCMLB
}
#endif

#ifdef	FCPICO
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System  = "fcpico";
}
#endif

#ifdef  FCLAMP
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System  = "fclamp";
}
#endif

#if defined(FCPICO) || defined(FCLAMP)
#define	SYSTEM_DEFINED
int     NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// FCPICO & FCLAMP
}
#endif

#ifdef  FCLOC
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System  = "fcloc";
}

int     NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// FCLOC
}
#endif

#ifdef	TEKTELIC
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System  = "tektelic";
}

int     NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntp.pid");	// TEKTELIC
}

#endif

#ifdef	RFILR
#define	SYSTEM_DEFINED
void	InitSystem()
{
	System	= "rfilr";
}

int	NtpdStarted()
{
	return	NtpdStartedGeneric(NULL);			// MTAC
}
#endif

#ifdef  OIELEC
#define	SYSTEM_DEFINED
void    InitSystem()
{
        System  = "oielec";
}

int     NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");	// OIELEC
}
#endif

#ifdef  GEMTEK
#define	SYSTEM_DEFINED
void    InitSystem()
{
       System  = "gemtek";
}

int     NtpdStarted()
{
        return  NtpdStartedGeneric("/var/run/ntpd.pid");        // GEMTEK
}
#endif

#ifndef	SYSTEM_DEFINED
void    InitSystem()
{
#warning "you are compiling the LRR for linux 32/64bits generic target system"
#warning "this implies the use of a Semtech Picocell connected with ttyACMx"
        System  = "linux";
}

int     NtpdStarted()
{
	return	NtpdStartedGeneric("/var/run/ntpd.pid");
}
#endif

void	DoUptime()
{
	FILE	*f;
	float	sec;
	struct	timeval	tv;

	// absolute process uptime ~ now
	clock_gettime(CLOCK_REALTIME,&UptimeProc);
	rtl_getCurrentIso8601date(UptimeProcStr);


	// relative system uptime in sec
	f	= fopen("/proc/uptime","r");
	if	(!f)
		return;

	fscanf	(f,"%f",&sec);

	// as process is newer than system :) ...
	UptimeSyst.tv_sec	= UptimeProc.tv_sec - (time_t)sec;
	UptimeSyst.tv_nsec	= 0;

	tv.tv_sec		= UptimeSyst.tv_sec;
	tv.tv_usec		= 0;
	rtl_gettimeofday_to_iso8601date(&tv,NULL,UptimeSystStr);


	fclose	(f);

}

static	void	StopIfaceDaemon()
{
	char	cmd[1024];

	if	(IfaceDaemon == 0)
	{
		RTL_TRDBG(0,"IfaceDaemon disabled\n");
		return;
	}
	sprintf	(cmd,"killall %s 2>&1 > /dev/null &","ifacefailover.sh");
	DoSystemCmdBackGround(cmd);
	sprintf	(cmd,"killall %s 2>&1 > /dev/null &","ifacerescuespv.sh");
	DoSystemCmdBackGround(cmd);
	RTL_TRDBG(0,"IfaceDaemon stopped\n");
}

static	void	ConfigIfaceDaemon(FILE *f)
{
	char	*section	= "ifacefailover.";
	int	lg;
	int	routesfound	= 0;
	int	rescuespvfound	= 0;

	inline	void	CBHtWalk(char *var,void *value)
	{
//		fprintf(out,"'%s' '%s'\n",var,(char *)value);
		if	(strncmp(var,section,lg) == 0)
		{
			char	*pt;
			char	maj[256];
			char	*val;

			val	= (char *)value;
			strcpy	(maj,var+lg);
			pt	= maj;
			while	(*pt)
			{
				*pt	= toupper(*pt);
				pt++;
			}
			if	(*val != '"')
			fprintf	(f,"%s=\"%s\"\n",maj,(char *)val);
			else
			fprintf	(f,"%s=%s\n",maj,(char *)val);
			if	(strcmp(maj,"ROUTES") == 0 && strlen(val))
			{
				routesfound	= 1;
			}
			if	(strcmp(maj,"RESCUESPV") == 0 && strlen(val))
			{
				rescuespvfound	= 1;
			}
		}
	}

	lg	= strlen(section);
	rtl_htblDump(HtVarLrr,CBHtWalk);

	// routes not declared use lrc1 .. lrcN
	if	(routesfound == 0 && NbLrc > 0)
	{
		int	i;

		fprintf	(f,"# no routes declared use lrc list\n");
		fprintf	(f,"ROUTES=\"");
		for	(i = 0 ; i < NbLrc ; i++)
		{
			fprintf	(f,"%s",TbLapLrc[i].lk_addr);
			if	(i != NbLrc - 1)
				fprintf	(f," ");
		}
		fprintf	(f,"\"\n");
	}
	if	(rescuespvfound == 0)
	{
		fprintf	(f,"# key rescuespv not declared => forced\n");
		fprintf	(f,"RESCUESPV=\"1\"\n");
	}
}

static	void	StartIfaceDaemon()
{
	char	exe[1024];
	char	cmd[1024];
	char	trace[1024];
	char	config[1024];
	FILE	*f;

	if	(IfaceDaemon == 0)
	{
		RTL_TRDBG(0,"IfaceDaemon disabled\n");
		return;
	}

	sprintf	(exe,"%s/lrr/com/shells/%s/",RootAct,System);
	strcat	(exe,"ifacefailover.sh");
	if	(access(exe,X_OK) != 0)
	{
		RTL_TRDBG(0,"IfaceDaemon not provided on '%s'\n",System);
		return;
	}
	sprintf	(config,"%s/usr/data/ifacefailover.cfg",RootAct);
	f	= fopen(config,"w");
	if	(f)
	{
		ConfigIfaceDaemon(f);
		fclose(f);
	}

	sprintf	(trace,"%s/var/log/lrr/IFACEDAEMON.log",RootAct);
	sprintf	(cmd,"%s 2>&1 > %s &",exe,trace);
	DoSystemCmdBackGround(cmd);
	RTL_TRDBG(0,"IfaceDaemon started\n");
}

static	void	ServiceExit()
{
	StopIfaceDaemon();
#ifdef WIRMANA
	LedBackhaul(0);
#endif
	unlink	(DoFilePid());
	RTL_TRDBG(0,"service stopped\n");
}

static	void	DoSigAction(int nointr)
{
	struct sigaction sigact;

	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigact.sa_handler = SigHandler;
	sigaction(SIGTERM,&sigact,NULL);
	sigaction(SIGUSR1,&sigact,NULL);
	sigaction(SIGUSR2,&sigact,NULL);

	sigact.sa_handler = SIG_IGN;
	sigaction(SIGINT,&sigact,NULL);
	sigaction(SIGQUIT,&sigact,NULL);
	sigaction(SIGHUP,&sigact,NULL);
	sigaction(SIGPIPE,&sigact,NULL);

	sigact.sa_handler = SIG_DFL;
	sigaction(SIGCHLD,&sigact,NULL);
	if	(nointr)
	sigaction(SIGINT,&sigact,NULL);
}

static	void	DoLap()
{
	int	i;
#ifdef	LP_TP31
	LapEnableLongMode();
#endif
	LapList		= LapInit(0,MainTbPoll);
	for	(i = 0 ; i < NbLrc ; i++)
	{
		TbLapLrc[i].lk_cbEvent	= (void *)LapEventProceed;
		TbLapLrc[i].lk_userptr	= &TbLrc[i];
		LapAddLink(&TbLapLrc[i]);

		TbLrc[i].lrc_lk		= &TbLapLrc[i];

		RTL_TRDBG(1,"idxlrc=%d lk=%p lrc=%p avdv=%p\n",i,
			&TbLapLrc[i],&TbLrc[i],&TbLrc[i].lrc_avdvtrip);
	}

	LapBindAll();
	LapConnectAll();
}

static	void	DoLapTest()
{
	int	i;
#ifdef	LP_TP31
	LapEnableLongMode();
#endif
	LapList		= LapInit(0,MainTbPoll);
	for	(i = 0 ; i < NbLrc ; i++)
	{
		TbLapLrc[i].lk_cbEvent	= (void *)LapEventProceedTest;
		TbLapLrc[i].lk_userptr	= &TbLrc[i];
		TbLapLrc[i].lk_t3	= 3;
		LapAddLink(&TbLapLrc[i]);

		TbLrc[i].lrc_lk		= &TbLapLrc[i];

		RTL_TRDBG(1,"idxlrc=%d lk=%p lrc=%p avdv=%p\n",i,
			&TbLapLrc[i],&TbLrc[i],&TbLrc[i].lrc_avdvtrip);
	}

	LapBindAll();
	LapConnectAll();
}

void	DecryptHtbl(void *htbl)
{
	inline	int	CBHtWalk(void *htbl,char *pvar,void *pvalue,void *p)
	{
		char	*value	= (char *)pvalue;
		char	var[512];
		char	varext[512];
		char	plaintext[1024];
		char	*pt;
		int	keynum;
		int	ret;
		int	lg;
		u_char	*keyvalhex;

		if	(!value)
			return	0;
		strcpy	(var,pvar);
		sprintf	(varext,"%s_crypted_k",var);
		pt	= rtl_htblGet(htbl,varext);
		if	(!pt || !*pt)
			return	0;
		// this variable is declared as crypted 
		lg	= strlen(value);
		if	(lg == 0 || *value != '[' || value[lg-1] != ']')
		{
			// but the crypted value is "" or not between [] 
			// => considered as not crypted
RTL_TRDBG(1,"'%s' declared as _crypted_k=%s but empty or not [...]\n",
				var,pt);
			rtl_htblRemove(htbl,varext);
			return	0;
		}
		value[lg-1]	= '\0';
		value++;
		keynum		= atoi(pt);
#ifdef NOSSL
		keyvalhex	= NULL;
#else
		keyvalhex	= BuildHex(keynum);
#endif
		if	(!keyvalhex)
			return	0;

RTL_TRDBG(1,"decrypt '%s' key=%d '%s'\n",var,keynum,value);
		plaintext[0]	= '\0';
#ifdef NOSSL
		ret = -1;
#else
		ret	= lrr_keyDec((u_char *)value,-1,
				keyvalhex,NULL,(u_char *)plaintext,1000,0);
#endif
		if	(ret >= 0)	// "" is a possible decrypted value
		{
			plaintext[ret]	= '\0';
#if	0
RTL_TRDBG(1,"decrypt'%s' ret=%d '%s'\n",var,ret,plaintext);
#endif
			rtl_htblRemove(htbl,varext);
			rtl_htblRemove(htbl,var);
			rtl_htblInsert(htbl,var,(void *)strdup(plaintext));
		}
		else
		{
RTL_TRDBG(1,"decrypt '%s' ret=%d ERROR\n",var,ret);
		}

		free(keyvalhex);
		return	0;
	}

	rtl_htblWalk(htbl,CBHtWalk,NULL);
}

void GetConfigFromBootSrv()
{
	char	buf[256];
	int	ret = 1;
	char	*addr, *port, *user, *pwd;

	LrrIDGetFromBS = 1;

	addr = CfgStr(HtVarLrr, "bootserver", -1, "addr", "0.0.0.0");
	port = CfgStr(HtVarLrr, "bootserver", -1, "port", "2404");
	user = CfgStr(HtVarLrr, "bootserver", -1, "user", "actility");
	pwd = CfgStr(HtVarLrr, "bootserver", -1, "pass", "");

	// execute shell to get config from bootserver
	RTL_TRDBG(0,"Get config from bootserver '%s:%s'...\n", addr, port);
	while (ret)
	{
		sprintf(buf, "$ROOTACT/lrr/com/cmd_shells/treatbootsrv.sh -A %s -P %s -U %s -W %s",
			addr, port, user, pwd);
		ret = system(buf);
		if (ret)
		{
			RTL_TRDBG(0,"Getting config from bootserver failed, retry in 30 seconds\n");
			sleep(30);
		}
	}
}

int	main(int argc,char *argv[])
{
	int	i;
	char	*pt = lrr_whatStr+18;	// TODO
	char	* lrr_base_version = NULL;
	char	deftrdir[256];

	MainThreadId	= pthread_self();

	InitSystem();

	if	(!System)
		System	= "generic";
	if	(!RootAct)
		RootAct	= getenv("ROOTACT");
	if	(!RootAct)
	{
		printf	("$ROOTACT not set\n");
		setenv	("ROOTACT",strdup("/home/actility"),1);
		RootAct	= getenv("ROOTACT");
	}

	clock_gettime(CLOCK_REALTIME,&Currtime);

	sscanf(pt,"%d.%d.%d_%d",&VersionMaj,&VersionMin,&VersionRel,&VersionFix);

	lrr_base_version = calloc(128, sizeof (char));
	sprintf(lrr_base_version,"%d.%d.%d",VersionMaj, VersionMin, VersionRel);
	setenv("LRR_VERSION", strdup(lrr_base_version), 1);
	free(lrr_base_version);
	lrr_base_version = calloc(128, sizeof (char));
	sprintf(lrr_base_version,"%d",VersionFix);
	setenv("LRR_FIXLEVEL", strdup(lrr_base_version), 1);
	free(lrr_base_version);

#ifdef _HALV_COMPAT
	setenv("HAL_VERSION", strdup(hal_version), 1);
#endif

	LP_VALID_SIZE_PKT();
	if	(argc == 2 && strcmp(argv[1],"--size") == 0)
	{
		StructSize();
		exit(0);
	}
	if	(argc == 2 && (strcmp(argv[1],"--version") == 0
			|| strcmp(argv[1],"--versions") == 0))
	{
		printf	("%s\n",rtl_version());
		printf	("%s\n",lrr_whatStr);
		printf	("%s\n",LgwVersionInfo());
#ifdef _HALV_COMPAT
		printf	("HAL %s\n",hal_version);
#endif
#ifdef	WITH_GPS
		printf	("gps=yes\n");
#endif
#ifdef	WITH_USB
		printf	("usb=yes\n");
#endif
#ifdef	WITH_SPI
		printf	("spi=yes\n");
#endif
#ifdef	WITH_TTY
		printf	("tty=yes\n");
#endif
#ifdef	WITH_SX1301_X1
		printf	("x1=yes\n");
#endif
#ifdef	WITH_SX1301_X8
		printf	("x8=yes\n");
#endif
		exit(0);
	}

	if	(argc == 2 && strcmp(argv[1],"--stpicoid") == 0)
	{
#ifdef	WITH_TTY
		uint8_t uid[8];  //unique id
		if	(lgw_connect(TtyDevice) == LGW_REG_SUCCESS)
		{
                	lgw_mcu_get_unique_id(&uid[0]);
			printf("%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\n",
				uid[0], uid[1], uid[2], uid[3], 
				uid[4], uid[5], uid[6], uid[7]);
			exit(0);
		}
		fprintf(stderr,"--stpicoid chipid error on tty link %s\n",
			TtyDevice);
		exit(1);
#endif
		fprintf(stderr,"--stpicoid not supported\n");
		exit(1);
	}
	if	(argc == 2 && strcmp(argv[1],"--avdv") == 0)
	{
		t_avdv_ui	avdvTrip;
		time_t		now;
		int		nb,n;
		int		t0,t1;

		time	(&now);
		for	(i = 0 ; i < AVDV_NBELEM ; i++)
		{
			AvDvUiAdd(&avdvTrip,360+(rand()%50),now+i);
		}
		t0	= rtl_tmmsmono();
		for	(n = 0 ; n < 100 ; n++)
			nb = AvDvUiCompute(&avdvTrip,now+AVDV_NBELEM,now+i);
		t1	= rtl_tmmsmono();
		printf	("nb=%d tms=%f av=%u dv=%u mx=%u\n",nb,
			(float)(t1-t0)/(float)nb,
			avdvTrip.ad_aver,avdvTrip.ad_sdev,avdvTrip.ad_vmax);
		exit(0);
	}

	if	(argc == 2 && strcmp(argv[1],"--cpu") == 0)
	{
		u_int	nb	= 0;
		while	(1)
		{
			double	used;
			used	= CompAllCpuInfos('S');
			printf	("cpuused=%f\n",used);
			if	(nb && nb%30==0)
			{
				CompAllCpuInfos('L');
				AvDvUiCompute(&CpuAvdvUsed,30,Currtime.tv_sec);
				printf("cpu av=%u dv=%u vm=%u\n",
					CpuAvdvUsed.ad_aver,
					CpuAvdvUsed.ad_sdev,
					CpuAvdvUsed.ad_vmax);
				printf("l1=%f l5=%f l15=%f\n",
					(float)CpuLoad1/100.0,
					(float)CpuLoad5/100.0,
					(float)CpuLoad15/100.0);
			}
			Currtime.tv_sec++;
			nb++;
			sleep(1);
		}
		exit(0);
	}
	if	(argc == 2 && strcmp(argv[1],"--mem") == 0)
	{
		while	(1)
		{
			CompAllMemInfos('S');
			printf	("MemTotal :%u ",MemTotal);
			printf	("MemFree :%u ",MemFree);
			printf	("Buffers :%u ",MemBuffers);
			printf	("Cached :%u ",MemCached);
			printf	("Used :%u\n",MemUsed);
			sleep(5);
		}
		exit(0);
	}

	DoUptime();


	sprintf	(ConfigDefault,"%s/%s",RootAct,"lrr/config");
	sprintf	(ConfigCustom,"%s/%s",RootAct,"usr/etc/lrr");
	rtl_mkdirp(ConfigCustom);
	sprintf	(DirCommands,"%s/%s",RootAct,"usr/data/lrr/cmd_shells");
	rtl_mkdirp(DirCommands);

	printf	("$ROOTACT %s\n",RootAct);
	printf	("ConfigDefault '%s'\n",ConfigDefault);
	printf	("ConfigCustom '%s'\n",ConfigCustom);

	rtl_init();

	// Activate traces before reading tracelevel in config to see startup messages
	rtl_tracemutex();
	rtl_tracelevel(1);
	rtl_tracesizemax(TraceSize);
	snprintf(deftrdir,sizeof(deftrdir)-1,"%s/%s",RootAct,"var/log/lrr/TRACE.log");
	deftrdir[sizeof(deftrdir)-1] = '\0';
        rtl_tracerotate(deftrdir);
	RTL_TRDBG(0,"Traces activated for startup\n");

	HtVarLrr	= rtl_htblCreateSpec(25,NULL,
						HTBL_KEY_STRING|HTBL_FREE_DATA);

	HtVarLgw	= rtl_htblCreateSpec(25,NULL,
						HTBL_KEY_STRING|HTBL_FREE_DATA);

	HtVarSys	= rtl_htblCreateSpec(25,NULL,
						HTBL_KEY_STRING|HTBL_FREE_DATA);

	if	(!HtVarLrr || !HtVarLgw || !HtVarSys)
	{
		RTL_TRDBG(0,"cannot alloc internal resources (htables)\n");
		exit(1);
	}


	if	(argc == 2 && strcmp(argv[1],"--config") == 0)
	{
		TraceDebug	= 0;
		TraceLevel	= 3;
		rtl_tracelevel(TraceLevel);
		LoadConfigDefine(0,1);
		LoadConfigBootSrv(1);
		LoadConfigLrr(0,1);
		LoadConfigLgw(0);
		LoadConfigChannel(0);
		LgwConfigure(0,0);
		ChannelConfigure(0,0);
		exit(0);
	}

	if	(argc == 2 && strcmp(argv[1],"--ini") == 0)
	{
		int		i;
		int		nbv	= 0;
		t_ini_var	tbvar[1024*4];

		inline	int	compar(const void *arg1, const void *arg2)
		{
			t_ini_var	*v1	= (t_ini_var *)arg1;
			t_ini_var	*v2	= (t_ini_var *)arg2;

			return	strcmp(v1->in_name,v2->in_name);
		}

		inline	void	CBHtDumpCli(char *var,void *value)
		{
//			printf("'%s' '%s'\n",var,(char *)value);
			if	(nbv < sizeof(tbvar)/sizeof(t_ini_var))
			{
				tbvar[nbv].in_name	= var;
				tbvar[nbv].in_val	= value;
				nbv++;
			}
		}
		TraceDebug	= 0;
		TraceLevel	= 0;
		rtl_tracelevel(TraceLevel);
		LoadConfigDefine(0,1);
		LoadConfigBootSrv(1);
		LoadConfigLrr(0,1);
		LoadConfigLgw(0);
		LoadConfigChannel(0);

		nbv	= 0;
		rtl_htblDump(HtVarSys,CBHtDumpCli);
		qsort(tbvar,nbv,sizeof(t_ini_var),compar);
		for	(i = 0 ; i < nbv ; i++)
			printf("'%s' '%s'\n",tbvar[i].in_name,
							tbvar[i].in_val);
		nbv	= 0;
		rtl_htblDump(HtVarLrr,CBHtDumpCli);
		qsort(tbvar,nbv,sizeof(t_ini_var),compar);
		for	(i = 0 ; i < nbv ; i++)
			printf("'%s' '%s'\n",tbvar[i].in_name,
							tbvar[i].in_val);
		nbv	= 0;
		rtl_htblDump(HtVarLgw,CBHtDumpCli);
		qsort(tbvar,nbv,sizeof(t_ini_var),compar);
		for	(i = 0 ; i < nbv ; i++)
			printf("'%s' '%s'\n",tbvar[i].in_name,
							tbvar[i].in_val);


		exit(0);
	}

	if	(argc == 2 && strcmp(argv[1],"--rtt") == 0)
	{
		TraceDebug	= 0;
		TraceLevel	= 3;
		rtl_tracelevel(TraceLevel);
		LoadConfigDefine(0,1);
		LoadConfigLrr(0,1);
		StartItfThread();
		while	(1)
		{
			clock_gettime(CLOCK_REALTIME,&Currtime);
			sleep(1);
		}
		exit(0);
	}

	if	(argc == 2 && strcmp(argv[1],"--itf") == 0)
	{
		int	nb	= 0;

		TraceDebug	= 0;
		TraceLevel	= 3;
		rtl_tracelevel(TraceLevel);
		LoadConfigDefine(0,1);
		LoadConfigLrr(0,1);
		while	(1)
		{
			CompAllItfInfos('S');
			if	(nb && nb%30==0)
			{
				CompAllItfInfos('L');
			}
			sleep(1);
			nb++;
		}
		exit(0);
	}

	if	(argc == 2 && strcmp(argv[1],"--mfs") == 0)
	{
		int	nb	= 0;

		TraceDebug	= 0;
		TraceLevel	= 3;
		rtl_tracelevel(TraceLevel);
		LoadConfigDefine(0,1);
		LoadConfigLrr(0,1);
		while	(1)
		{
//			CompAllMfsInfos('S');
			if	(nb && nb%30==0)
			{
				CompAllMfsInfos('L');
			}
			sleep(1);
			nb++;
		}
		exit(0);
	}
	if	(argc == 2 
		&& (!strcmp(argv[1],"--xlap") || !strcmp(argv[1],"--lap")))
	{
		LapTest		= 1;
		TraceDebug	= 0;
		TraceLevel	= 0;
		if	(!strcmp(argv[1],"--xlap"))
		{
			LapTest		= 2;
			TraceLevel	= 1;
		}
		rtl_tracelevel(TraceLevel);
		LoadConfigDefine(0,0);
		LoadConfigLrr(0,0);
		DoSigAction(1);
		MainTbPoll	= rtl_pollInit();
		MainQ		= rtl_imsgInitIq();
		LgwQ		= rtl_imsgInitIq();
#ifdef REF_DESIGN_V2
		for (i=0; i<SX1301AR_MAX_BOARD_NB; i++)
		{
			LgwSendQ[i]	= rtl_imsgInitIq();
			if	(!LgwSendQ[i])
			{
				RTL_TRDBG(0,"cannot alloc internal resources (queues)\n");
				exit(1);
			}
		}
		if	(!MainTbPoll || !MainQ || !LgwQ)
		{
			RTL_TRDBG(0,"cannot alloc internal resources (queues)\n");
			exit(1);
		}
#else
		LgwSendQ	= rtl_imsgInitIq();
		if	(!MainTbPoll || !MainQ || !LgwQ || !LgwSendQ)
		{
			RTL_TRDBG(0,"cannot alloc internal resources (queues)\n");
			exit(1);
		}
#endif /* defined(REF_DESIGN_V2) */
		LgwThreadStopped= 1;
		GpsThreadStopped = 1;
		DoLapTest();
		MainLoop();
		exit(0);
	}

	if	(access(DoFilePid(),R_OK) == 0)
	{
		SickRestart	= 1;
	}

	atexit	(ServiceExit);
	ServiceWritePid();

	DoSigAction(0);

#ifdef	WIRMANA
	LedConfigure();
#endif

	// if bootserver.ini found, activate NFR997
	if (LoadConfigBootSrv(0))
	{
		LoadConfigDefine(0,0);
		GetConfigFromBootSrv();
	}

	MainTbPoll	= rtl_pollInit();
	MainQ		= rtl_imsgInitIq();
	LgwQ		= rtl_imsgInitIq();
	StoreQ		= rtl_imsgInitIq();
#ifdef REF_DESIGN_V2
	for (i=0; i<SX1301AR_MAX_BOARD_NB; i++)
	{
		LgwSendQ[i]	= rtl_imsgInitIq();
		if	(!LgwSendQ[i])
		{
			RTL_TRDBG(0,"cannot alloc internal resources (queues)\n");
			exit(1);
		}
	}
	if	(!MainTbPoll || !MainQ || !LgwQ || !StoreQ)
	{
		RTL_TRDBG(0,"cannot alloc internal resources (queues)\n");
		exit(1);
	}
#else
	LgwSendQ	= rtl_imsgInitIq();
	if	(!MainTbPoll || !MainQ || !LgwQ || !LgwSendQ || !StoreQ)
	{
		RTL_TRDBG(0,"cannot alloc internal resources (queues)\n");
		exit(1);
	}
#endif /* defined(REF_DESIGN_V2) */

	LoadConfigDefine(0,0);
	LoadConfigLrr(0,0);
	rtl_tracemutex();
	rtl_tracelevel(TraceLevel);
        if      (strcmp(TraceFile,"stderr") != 0)
        {
		rtl_tracesizemax(TraceSize);
                rtl_tracerotate(TraceFile);
        }
#if	0
	DecryptHtbl(HtVarLrr);
#endif

	RTL_TRDBG(0,"start lrr.x/main th=%lx pid=%d sickrestart=%d\n",
			(long)pthread_self(),getpid(),SickRestart);
	RTL_TRDBG(0,"%s\n",lrr_whatStr);
	RTL_TRDBG(0,"%s\n",LgwVersionInfo());
#ifdef _HALV_COMPAT
	RTL_TRDBG(0,"HAL %s\n",hal_version);
#endif
	RTL_TRDBG(0,"lrrid=%08x lrridext=%04x lrridpref=%02x\n",
		LrrID,LrrIDExt,LrrIDPref);

	if (TraceStdout && *TraceStdout)
	{
		if (!freopen(TraceStdout, "w", stdout))
		{
			RTL_TRDBG(0,"stdout can't be redirected to '%s' !\n", TraceStdout);
		}
		else
		{
			RTL_TRDBG(0,"stdout redirected to '%s'\n", TraceStdout);
		}
	}

	LoadConfigLgw(0);
	LoadConfigChannel(0);
	// very special case , because channels.ini needs info from lgw.ini
	LgwGenConfigure(0,1);
	ChannelConfigure(0,1);

	StartIfaceDaemon();
#ifdef	WITH_GPS
	StartGpsThread();
#else
	UseGps		= 0;
	UseGpsTime	= 0;
	UseGpsPosition	= 0;
#endif
	StartLgwThread();
	StartCmdThread();
	StartItfThread();

#if	0
	LapSizeFrame(2,256);
#endif

	DoLap();
	RTL_TRDBG(0,"TRACE LEVEL IS SET TO %d\n", TraceLevel);
	MainLoop();
	exit(0);
}
