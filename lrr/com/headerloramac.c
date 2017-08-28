#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>


#include "headerloramac.h"

struct sk_buff {
//	unsigned int	size;		// constant
//	unsigned char	*head;		// constant
//	unsigned char	*end;		// constant

	unsigned int	len;
	unsigned char	*data;
	unsigned char	*tail;
//	unsigned char	ref;
};

static	int LoRaMAC_decodeHeader_skb(struct sk_buff *skb, LoRaMAC_t *lora) {
	if	(skb->len < 11)
		return -1;

	memset (lora, 0, sizeof(LoRaMAC_t));
	lora->payload_encrypted = true;
	lora->MHDR.MType	= *skb->data >> 5;
	lora->MHDR.RFU		= (*skb->data >> 2) & 0x7;		// 3-bits
	lora->MHDR.Major	= *skb->data & 0x3;
	skb->data	++;
	skb->len	--;

	memcpy (lora->DevAddr, skb->data, 4);
	skb->data	+= 4;
	skb->len	-= 4;

	lora->FCtrl.ADR		= (*skb->data >> 7) & 0x1;
	lora->FCtrl.ADRACKReq	= (*skb->data >> 6) & 0x1;
	lora->FCtrl.ACK		= (*skb->data >> 5) & 0x1;
	lora->FCtrl.FPending	= (*skb->data >> 4) & 0x1;
	lora->FCtrl.FOptsLen	= *skb->data & 0xf;
	skb->data	++;
	skb->len	--;

	lora->FCnt	= *skb->data + (*(skb->data+1) << 8);
	skb->data	+= 2;
	skb->len	-= 2;

	if	(lora->FCtrl.FOptsLen != 0) {
		if	(lora->FCtrl.FOptsLen > skb->len-4)
			return -1;
		memcpy (lora->FOpts, skb->data, lora->FCtrl.FOptsLen);
		skb->data	+= lora->FCtrl.FOptsLen;
		skb->len	-= lora->FCtrl.FOptsLen;
	}

	//	last 4 bytes are for MIC
	memcpy (lora->MIC, skb->tail-4, 4);
	skb->tail -= 4;
	skb->len -= 4;

	if	(skb->len > 0) {
		lora->FPort	= *skb->data;
		skb->data	++;
		skb->len	--;
		if	(skb->len > 0) {
			lora->payload	= skb->data;
			lora->payload_len = skb->len;
		}
	}
	else
		lora->flags	|= LORA_FPort_Absent;

	return 0;
}

int LoRaMAC_decodeHeader(uint8_t *data, uint32_t len, LoRaMAC_t *lora)
{
	struct sk_buff	skb;

	memset	(&skb,0,sizeof(skb));

	skb.data	= data;
	skb.len		= len;
	skb.tail	= data + len;

	return	LoRaMAC_decodeHeader_skb(&skb,lora);
}
