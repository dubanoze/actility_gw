
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


void	FindIpInterfaces(t_lrr_config *config);
t_wan_itf	*FindItfByName(char *name);
void	CompAllItfInfos(char shortlongtime);
char	*FindItfDefautRoute();
uint8_t	StateItf(t_wan_itf *itf);
uint32_t	GetMacAddr32(char *name,u_short *ext);
uint32_t	GetIpv4Addr(char *name);
uint32_t	FindEthMac32(u_short *ext);

void	AvDvUiClear(t_avdv_ui *ad);
void	AvDvUiAdd(t_avdv_ui *ad,uint32_t val,time_t when);
int	AvDvUiCompute(t_avdv_ui *ad,time_t tmax,time_t when);

char	*CfgStr(void *ht,char *sec,int index,char *var,char *def);
int	CfgInt(void *ht,char *sec,int index,char *var,int def);
int CfgCBIniLoad(void *user,const char *section,const char *name,const char *value);

int	LgwGenConfigure(int hot,int config);
void	LgwDumpGenConf(FILE *f);
int	LgwTempPowerGain();

int	LgwStart();
int	LgwStarted();
void	LgwStop();
void	*LgwRun(void *param);
int	LgwSendPacket(t_lrr_pkt *downpkt,int seqnum,int m802,int ack);
int	LgwPacketDelayMsFromUtc(t_lrr_pkt *downpkt,struct timespec *utc);
int	LgwPacketDelayMsFromUtc2(t_lrr_pkt *downpkt,struct timespec *utc);
int	LgwDiffMsUtc(struct timespec *utc1,struct timespec *utc0);
void	LgwEstimUtc(struct timespec *utc);
void	LgwInitPingSlot(t_lrr_pkt *pkt);
void	LgwResetPingSlot(t_lrr_pkt *pkt);
int	LgwNextPingSlot(t_lrr_pkt *pkt,struct timespec *eutc,int maxdelay,int *retdelay);

int	BinSearchFirstChannel(uint32_t freq);
int	BinSearchFirstChannelDn(uint32_t freq);
t_channel	*FindChannelUp(uint32_t freq);
t_channel	*FindChannelDn(uint32_t freq);

void	ChangeChannelFreq(t_lrr_pkt *downpkt,T_lgw_pkt_tx_t *txpkt);
int	CmpChannel(const void *m1, const void *m2);
int	ChannelConfigure(int hot,int config);
void	TmoaLrrPacket(t_lrr_pkt *downpkt);
float	TmoaLrrPacketUp(T_lgw_pkt_rx_t *pkt);
void	DoPcap(char *data,int sz);
void	AutoRx2Settings(t_lrr_pkt *downpkt);
void	AdjustRx2Settings(t_lrr_pkt *downpkt);


void	LgwForceDefine(void *htbl);
char	*LgwVersionInfo();
int	LgwConfigure(int hot,int config);
int	LgwLINKUP();
int	LgwDoSendPacket(time_t now);
int	LgwDoRecvPacket(time_t tms);
void	LgwGpsTimeUpdated(struct timespec *utc_from_gps, struct timespec * ubxtime_from_gps);
int	LgwTxFree(uint8_t board,uint8_t *txstatus);


void	DoSystemCmdBackGround(char *cmd);
void	DoSystemCmd(t_lrr_pkt *downpkt,char *cmd);
void	DoShellCommand(t_lrr_pkt *downpkt);
void	StartCmdThread();
unsigned int CmdCountOpenSsh();
unsigned int CmdCountRfScan();

int8_t 	GetTxCalibratedEIRP(int8_t tx_requested, float antenna_gain, float cable_loss, uint8_t board, uint8_t rfc);

void	DoLocKeyRefresh(int delay, char ** resp);
void	DoConfigRefresh(int delay);
void	DoAntsConfigRefresh(int delay, char ** resp);
void	DoCustomVersionRefresh(int delay);
void	DoStatRefresh(int delay);
void	DoRfcellRefresh(int delay);


int	OkDevAddr(char *dev);


unsigned short
crc_contiki(unsigned short acc,const unsigned char *data, int len);


void ReStartLgwThread();
void	StartItfThread();
int	UsbProtectOff();
int	UsbProtectOn();


void	lrr_keyInit();
int lrr_keyEnc(unsigned char *plaintext, int plaintext_len, unsigned char *key,
unsigned char *iv, unsigned char *ciphertext,int maxlen,int aschex);
int lrr_keyDec(unsigned char *ciphertext,int ciphertext_len,unsigned char *key,
  unsigned char *iv, unsigned char *plaintext,int maxlen,int aschex);
unsigned char        *BuildHex(int version);


void	SaveConfigFileState();
void	GpsGetInfos(u_char *mode,float *lat,float *lon,short *alt,u_int *cnt);
#ifdef WITH_GPS
void	GpsPositionUpdated(LGW_COORD_T *loc_from_gps);
int	GpsStart();
int	GpsStarted();
void	GpsStop();
void	* GpsRun(void * param);
#endif /* WITH_GPS */

void DcCheckLists();
int DcTreatDownlink(t_lrr_pkt *pkt);
int DcTreatUplink(t_lrr_pkt *pkt);
void DcSaveCounters(FILE *f);
void DcWalkCounters(void *f,void (*fct)(void *pf,int type,int ant,int idx,
                        float up,float dn));
void DcWalkChanCounters(void *f,void (*fct)(void *pf,int ant,int c,int s,
                        float up,float dn,float upsub,float dnsub));

uint8_t	CodeSpreadingFactor(int sf);
uint8_t	CodeCorrectingCode(int coderate);

void	SetIndic(t_lrr_pkt *pkt,int delivered,int c1,int c2,int c3);
void	SendIndicToLrc(t_lrr_pkt *pkt);
void	SendCapabToLrc(t_xlap_link *lktarget);

void	DecryptHtbl(void *htbl);


#ifdef WIRMANA
int LedConfigure();
int LedShotTxLora();
int LedShotRxLora();
int LedBackhaul(int val);
#endif
