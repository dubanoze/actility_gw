#ifndef _LIBGEN_H
#define _LIBGEN_H

#include <stdio.h>

typedef unsigned long int uint4;

typedef struct {
	uint4 i[2];
	uint4 buf[4];
	unsigned char in[64];
	unsigned char digest[16];
} keygen_t;

/*
 * Initalize the keygen structure used for key calculation
 */
void init(keygen_t *keygen);

/*
 * Generate a hash key based on the secret str parameter
 */
void generate(keygen_t *keygen, char *str, unsigned int len); 

/*
 * Extract the key in a buffer
 */
void retrieve(keygen_t *keygen, char *buffer);

#endif
