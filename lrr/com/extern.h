
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

extern	u_int	VersionMaj;
extern	u_int	VersionMin;
extern	u_int	VersionRel;

extern	int	Sx13xxPolling;
extern	int	Sx13xxStartDelay;
#if defined(FCMLB) || defined(FCPICO) || defined(GEMTEK) || defined(FCLAMP)
extern	char	*SpiDevice;
#elif defined(REF_DESIGN_V2)
extern	char	*SpiDevice[SX1301AR_MAX_BOARD_NB];
#endif
extern	int	CfgRadioStop;
extern	int	CfgRadioDnStop;
extern	int	DownRadioStop;
extern	int	MainWantStop;
extern	int	MainStopDelay;

extern	int	AntennaGain[];
extern	int	CableLoss[];
extern	char	*RfRegionId;
extern	u_int	RfRegionIdVers;

extern	char	DirCommands[];

#ifdef REF_DESIGN_V2
extern	sx1301ar_tref_t Gps_time_ref[SX1301AR_MAX_BOARD_NB];
#else
extern	struct tref 		Time_reference_gps; 
#endif
extern	int			Gps_ref_valid; 
extern	struct timespec		LgwCurrUtcTime;
extern	struct timespec		LgwBeaconUtcTime;
extern	time_t			LgwTmmsUtcTime;

extern	struct	timespec	Currtime;

extern	t_wan_itf		TbItf[NB_ITF_PER_LRR];
extern	int			NbItf;

extern	t_lrc_link		TbLrc[NB_LRC_PER_LRR];
extern	u_int			NbLrc;

extern	char			UptimeSystStr[];
extern	char			UptimeProcStr[];


extern	char	*RootAct;
extern	char	*System;
extern	char	*IsmBand;
extern	char	*IsmBandAlter;
extern	int	IsmFreq;

extern	t_lrr_config	ConfigIpInt;

extern	int	LgwLinkUp;
extern	int	MacWithBeacon;
extern	int	ServiceStopped;
extern	int	LgwThreadStopped;
extern	int	GpsThreadStopped;
extern	char    *TraceFile;
extern	int     TraceSize;
extern	int     TraceLevel;
extern	int     TraceDebug;
extern	void	*HtVarLrr;
extern	void	*HtVarLgw;
extern	void	*HtVarSys;
extern	void	*MainQ;
extern	void	*LgwQ;
#ifdef REF_DESIGN_V2
extern	void	*LgwSendQ[];
#else
extern	void	*LgwSendQ;
#endif
extern	void	*MainTbPoll;

extern	int	PingRttPeriod;

extern	unsigned	int	UseLgwTime;
extern	int			AdjustDelay;
extern	unsigned	int	LrrID;

extern int      GpsFd;
extern char   * GpsDevice;
extern u_int    UseGps;
extern u_int    UseGpsPosition;
extern u_int    UseGpsTime;
extern u_int    GpsUpdateCnt;
extern float    GpsLatt;
extern float    GpsLong;
extern int      GpsAlti;
extern int      GpsPositionOk;
extern float    LrrLat;
extern float    LrrLon;
extern int      LrrAlt;

extern	unsigned	int	LrcAvPktTrip;
extern	unsigned	int	LrcMxPktTrip;
extern	unsigned 	int	LrxAvPktProcess;
extern	unsigned 	int	LrxMxPktProcess;

extern	unsigned	int	LgwNbPacketSend;
extern	unsigned	int	LgwNbPacketWait;
extern	unsigned	int	LgwNbPacketLoop;
extern	unsigned	int	LgwNbPacketRecv;

extern	unsigned	int	LgwNbStartOk;
extern	unsigned	int	LgwNbStartFailure;
extern	unsigned	int	LgwNbConfigFailure;
extern	unsigned	int	LgwNbLinkDown;
extern	unsigned	int	LgwNbBusySend;
extern	unsigned	int	LgwNbSyncError;
extern	unsigned	int	LgwNbCrcError;
extern	unsigned	int	LgwNbSizeError;
extern	unsigned	int	LgwNbChanUpError;
extern	unsigned	int	LgwNbChanDownError;
extern	unsigned	int	LgwNbDelayError;
extern	unsigned	int	LgwNbDelayReport;

extern	unsigned	int	MacNbFcsError;

extern			int	LgwInvertPol;
extern			int	LgwInvertPolBeacon;
extern			int	LgwNoCrc;
extern			int	LgwNoHeader;
extern			int	LgwPreamble;
extern			int	LgwPreambleAck;
extern			int	LgwPower;
extern			int	LgwPowerMax;
extern			int	LgwAckData802Wait;
extern			u_int	LgwSyncWord;
extern			char	*LgwSyncWordStr;
extern			int	LgwBoard;
extern			int	LgwChipsPerBoard;
extern			int	LgwAntenna;
extern			int	LgwForceRX2;
extern			int	LgwLbtEnable;
extern			int	LgwLbtRssi;
extern			int	LgwLbtRssiOffset;
extern			int	LgwLbtScantime;
extern			int	LgwLbtNbChannel;

extern			t_channel	TbChannel[NB_CHANNEL];
extern			t_channel	* Rx2Channel;
extern			int	NbChannel;
extern			int	MaxChannel;
extern			t_channel_entry	TbChannelEntry[NB_CHANNEL];
extern			int	NbChannelEntry;
extern			t_channel_entry	TbChannelEntryDn[NB_CHANNEL];
extern			int	NbChannelEntryDn;

#ifdef REF_DESIGN_V2
extern			int	LastTmoaRequested[];
extern			int	CurrTmoaRequested[];
#else
extern			int	LastTmoaBoard;
extern			int	LastTmoaRequested;
extern			int	CurrTmoaRequested;
#endif

extern			int	WanRefresh;
extern			int	TempEnable;
extern			int	TempPowerAdjust;
extern			int	TempExtAdjust;
extern			int	CurrTemp;

extern			int	MaxReportDnImmediat;
extern			int	LogUseRamDir;

extern			u_int	LgwBeaconRequestedCnt;
extern			u_int	LgwBeaconSentCnt;
extern			u_char	LgwBeaconLastDeliveryCause;
