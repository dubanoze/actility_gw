
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

#define	STRERRNO	strerror(errno)
#define	MSTIC		1
#define ABS(x)		((x) > 0 ? (x) : -(x))


#define	LP_LRX_VERSION	1

#define	SERVICE_NAME		"lora-relay"
#define	SERVICE_STATUS_FILE	"/tmp/lora-relay.status"

#define	LRR_DEFAULT_T3	DEFAULT_T3

#define	NB_ANTENNA	3

#define	NB_CHANNEL	255	//	0..254
#define	UNK_CHANNEL	255
#define	MAXUP_CHANNEL	126	// >= 127 => downklink channels

#define	LGW_RECV_PKT	4

#define	IM_DEF			1000
#define	IM_TIMER_GEN		1001
#define					IM_TIMER_GEN_V		10000	// ms

#define	IM_TIMER_BEACON		1002
#define					IM_TIMER_BEACON_V	60000	// ms

#define	IM_TIMER_LRRUID_RESP	1003
#define					IM_TIMER_LRRUID_RESP_V	5000	// ms

#define	IM_SERVICE_STATUS_RQST	1100

#define	IM_LGW_CONFIG_FAILURE	1200	// LGW -> SPV
#define	IM_LGW_START_FAILURE	1201	// LGW -> SPV
#define	IM_LGW_STARTED		1202	// LGW -> SPV
#define	IM_LGW_LINK_UP		1203	// LGW -> SPV
#define	IM_LGW_LINK_DOWN	1204	// LGW -> SPV
#define	IM_LGW_SEND_DATA	1205	// SPV -> LGW
#define	IM_LGW_RECV_DATA	1206	// LGW -> SPV
#define	IM_LGW_GPS_TIME		1207	// SPV -> LGW
#define	IM_LGW_POST_DATA	1208	// SPV -> SPV then IM_LGW_SEND_DATA
#define	IM_LGW_DELAY_ALLLRC	1209	// SPV -> SPV
#define	IM_LGW_EXIT		1210	// SPV -> LGW
#define	IM_LGW_SENT_INDIC	1211	// LGW -> SPV

#define	IM_CMD_RECV_DATA	1300	// CMD -> SPV

#define	IM_DC_LIST		1400	// Duty cycle

#define	MAX_BOARDS		4	// Maximum of boards

/* Semtech Ref Design version, based on gw platform */
#if defined(WIRMAAR) || defined(CISCOMS) || defined(FCLOC) || defined(TEKTELIC)
#define REF_DESIGN_V2 1
#undef  REF_DESIGN_V1
#else /* == WIRMAV2, WIRMANA, FCPICO, FCLAMP, GEMTEK ... */
#define REF_DESIGN_V1 1
#undef  REF_DESIGN_V2
#endif

#if defined(CISCOMS)
#define HAL_VERSION_5 1
#undef  HAL_VERSION_4
#undef  HAL_VERSION_3
#else
#define HAL_VERSION_4 1
#undef  HAL_VERSION_5
#undef  HAL_VERSION_3
#endif

#ifdef REF_DESIGN_V2
#define LGW_GPS_SUCCESS  0
#define LGW_GPS_ERROR   -1
#define	LGW_COORD_T	sx1301ar_coord_t
#define	LGW_GPSMSG_T	sx1301ar_nmea_msg_t
#ifdef HAL_VERSION_5
#define LGW_UBXMSG_T	sx1301ar_ubx_msg_t
#endif /* HAL_VERSION_5 */
#else
#define	LGW_COORD_T	struct coord_s
#define	LGW_GPSMSG_T	enum gps_msg
#endif /* REF_DESIGN_V2 */

#define GPS_REF_MAX_AGE 30

/* LBT Feature availability
 *    - For GW_V2, LBT feature comes since HAL >= 4.0.0 (SX1301AR_LBT_CHANNEL_NB_MAX)
 */
#if defined(LGW_LBT_ISSUE) || defined(SX1301AR_LBT_CHANNEL_NB_MAX)
#define WITH_LBT 1
#endif

#if (defined(WIRMANA) || defined(FCPICO) || defined(FCLAMP) || defined(MTAC) || defined (MTAC_USB) || defined (MTCAP) || defined(NATRBPI)) && defined(WITH_LBT)
#undef	WITH_LBT
#endif

#ifdef	STM32FWVERSION	// pico lora lib => no lbt no gps
#undef	WITH_LBT
#undef	WITH_GPS
#endif

#ifdef	WITH_TTY	// pico lora lib => no lbt no gps
#undef	WITH_LBT
#undef	WITH_GPS
#endif

#ifdef REF_DESIGN_V1
#ifdef	WIRMAMS
#define	LGW_BOARD_SETCONF(p1,...)	lgw_board_setconf(p1,__VA_ARGS__)
#define	LGW_GET_TRIGCNT(p1,...)		lgw_get_trigcnt(p1,__VA_ARGS__)
#define	LGW_RECEIVE(p1,...)		lgw_receive(p1,__VA_ARGS__)
#define	LGW_REG_CHECK(p1,...)		lgw_reg_check(p1,__VA_ARGS__)
#define	LGW_REG_RB(p1,...)		lgw_reg_rb(p1,__VA_ARGS__)
#define	LGW_REG_W(p1,...)		lgw_reg_w(p1,__VA_ARGS__)
#define	LGW_RXIF_SETCONF(p1,...)	lgw_rxif_setconf(p1,__VA_ARGS__)
#define	LGW_RXRF_SETCONF(p1,...)	lgw_rxrf_setconf(p1,__VA_ARGS__)
#define	LGW_START(p1)			lgw_start(p1)
#define	LGW_STATUS(p1,...)		lgw_status(p1,__VA_ARGS__)
#define	LGW_STOP(p1)			lgw_stop(p1)
#define	LGW_TXGAIN_SETCONF(p1,...)	lgw_txgain_setconf(p1,__VA_ARGS__)
#else
#define	LGW_BOARD_SETCONF(p1,...)	lgw_board_setconf(__VA_ARGS__)
#define	LGW_GET_TRIGCNT(p1,...)		lgw_get_trigcnt(__VA_ARGS__)
#define	LGW_RECEIVE(p1,...)		lgw_receive(__VA_ARGS__)
#define	LGW_REG_CHECK(p1,...)		lgw_reg_check(__VA_ARGS__)
#define	LGW_REG_RB(p1,...)		lgw_reg_rb(__VA_ARGS__)
#define	LGW_REG_W(p1,...)		lgw_reg_w(__VA_ARGS__)
#define	LGW_RXIF_SETCONF(p1,...)	lgw_rxif_setconf(__VA_ARGS__)
#define	LGW_RXRF_SETCONF(p1,...)	lgw_rxrf_setconf(__VA_ARGS__)
#define	LGW_START(p1)			lgw_start()
#define	LGW_STATUS(p1,...)		lgw_status(__VA_ARGS__)
#define	LGW_STOP(p1)			lgw_stop()
#define	LGW_TXGAIN_SETCONF(p1,...)	lgw_txgain_setconf(__VA_ARGS__)
#endif /* WIRMAMS */
#endif /* REF_DESIGN_V1 */

