
#ifdef	WITH_SX1301_X1


#include "loragw_hal.h"
#ifdef	WITH_GPS
#include "loragw_gps.h"
#endif
#include "loragw_aux.h"
#include "loragw_reg.h"
#if defined(FCMLB) || defined(FCPICO)
#include "loragw_lbt.h"
#endif

typedef	struct lgw_pkt_rx_s	T_lgw_pkt_rx_t;
typedef	struct lgw_pkt_tx_s	T_lgw_pkt_tx_t;

typedef	struct lgw_conf_rxrf_s	T_lgw_conf_rxrf_t;
typedef	struct lgw_conf_rxif_s	T_lgw_conf_rxif_t;
typedef	struct lgw_conf_lbt_s	T_lgw_conf_lbt_t;

#ifndef	WITH_GPS	// due to absence of loragw_gps.h with pico lora lib
struct tref {
    time_t          systime;    /*!> system time when solution was calculated */
    uint32_t        count_us;   /*!> reference concentrator internal timestamp */
    struct timespec utc;        /*!> reference UTC time (from GPS) */
    double          xtal_err;   /*!> raw clock error (eg. <1 'slow' XTAL) */
};
#endif

#ifdef	STM32FWVERSION	// pico lora lib => no gps
#undef	WITH_LBT
#endif

#ifndef	TX_START_DELAY
#define		SX13XX_START_DELAY		1500
#else
#define		SX13XX_START_DELAY		0
#endif

#ifdef	USELIBLGW3
#undef		SX13XX_START_DELAY
#define		SX13XX_START_DELAY		0
#endif



#endif

#ifdef  WITH_SX1301_X8

#include "sx1301ar_hal.h"
#include "sx1301ar_err.h"
#include "sx1301ar_aux.h"
#include "sx1301ar_gps.h"
#ifndef TEKTELIC
#include "sx1301ar_reg.h"
#endif

#ifndef	TX_START_DELAY
#define		SX13XX_START_DELAY		1500
#else
#define		SX13XX_START_DELAY		0
#endif

#define	DR_LORA_SF7	MR_SF7
#define	DR_LORA_SF8	MR_SF8
#define	DR_LORA_SF9	MR_SF9
#define	DR_LORA_SF10	MR_SF10
#define	DR_LORA_SF11	MR_SF11
#define	DR_LORA_SF12	MR_SF12
#define	DR_LORA_MULTI	MR_SF7_12	// (MR_SF7 | ... | MR_SF12)

#define	BW_500KHZ	BW_500K
#define	BW_250KHZ	BW_250K
#define	BW_125KHZ	BW_125K
#define	BW_62K5HZ	BW_62K5
#define	BW_31K2HZ	BW_31K2
#define	BW_15K6HZ	BW_15K6
#define	BW_7K8HZ	BW_7K8

#define	CR_LORA_4_5	CR_4_5
#define	CR_LORA_4_6	CR_4_6
#define	CR_LORA_4_7	CR_4_7
#define	CR_LORA_4_8	CR_4_8

#define	LGW_HAL_SUCCESS	0
#define	LGW_HAL_ERROR	-1

#define	STAT_CRC_BAD	STAT_CRC_ERR

typedef	sx1301ar_rx_pkt_t	T_lgw_pkt_rx_t;
typedef	sx1301ar_tx_pkt_t	T_lgw_pkt_tx_t;

#endif

