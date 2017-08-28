
#ifndef LORAMAC_H
#define LORAMAC_H

#include <stdint.h>
#include <stdbool.h>




typedef struct {
	uint8_t		MType;		// 3-bits
	uint8_t		RFU;		// 3-bits
	uint8_t		Major;		// 2-bits
} LoRaMAC_MHDR_t;

typedef struct {
	uint8_t		ADR;		// 1-bit
	uint8_t		ADRACKReq;	// 1-bit
	uint8_t		ACK;		// 1-bit
	uint8_t		FPending;	// 1-bit
	uint8_t		FOptsLen;	// 4-bits
} LoRaMAC_FCtrl_t;

typedef struct {
	LoRaMAC_MHDR_t	MHDR;
	uint8_t		MIC[4];
	uint8_t		DevAddr[4];
	LoRaMAC_FCtrl_t		FCtrl;
	uint32_t	FCnt;		// 16-bits sent but 32 bits used for MIC calculation and cryptage
#define	LORA_MAX_OPTS	15
	uint8_t		FOpts[LORA_MAX_OPTS];
	uint8_t		FPort;
	bool		updown;
#define	LORA_UpLink	0
#define	LORA_DownLink	1
	bool		payload_encrypted;
	uint8_t		*payload;
	uint8_t		payload_len;
	uint8_t		flags;
#define LORA_FPort_Absent	0x1
	uint32_t	Delay;
#define LORAMAC_R2	2
#define LORAMAC_R3	3
	int specificationVersion;
} LoRaMAC_t;


int LoRaMAC_decodeHeader(uint8_t *data, uint32_t len, LoRaMAC_t *lora);

#endif
