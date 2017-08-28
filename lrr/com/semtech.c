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

/*! @file semtech.c
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "rtlbase.h"

#include "semtech.h"


// defined in main.c
extern char *IsmBand;

#ifdef	IR910
// Cisco 433 Mhz Card:

#define RSSI_BOARD_OFFSET_IR910_433	176.5
const tx_pow_t tx_pow_table_ir910_433[TX_POW_LUT_SIZE] = {\
	{	0,	3,	8,	-7},\
	{	0,	3,	10,	-3},\
	{	0,	3,	12,	0},\
	{	1,	3,	8,	4},\
	{	1,	3,	10,	7},\
	{	1,	3,	11,	8},\
	{	1,	3,	12,	9},\
	{	1,	3,	15,	10},\
	{	2,	3,	8,	11},\
	{	2,	3,	9,	14},\
	{	2,	3,	10,	15},\
	{	2,	3,	11,	17},\
	{	3,	3,	8,	20},\
	{	3,	3,	9,	22},\
	{	3,	3,	10,	23},\
	{	3,	3,	11,	23.5},\
};

// Cisco 868 MHz Card:
#define RSSI_BOARD_OFFSET_IR910_868	166.0
const tx_pow_t tx_pow_table_ir910_868[TX_POW_LUT_SIZE] = {\
	{	0,	3,	9,	-7},\
	{	0,	3,	12,	-3},\
	{	1,	3,	8,	0},\
	{	1,	3,	11,	5},\
	{	1,	3,	12,	7},\
	{	1,	3,	13,	8},\
	{	1,	3,	15,	9},\
	{	2,	3,	9,	10},\
	{	2,	3,	10,	11},\
	{	2,	3,	11,	14},\
	{	2,	3,	12,	15},\
	{	2,	3,	14,	17},\
	{	3,	3,	10,	20},\
	{	3,	3,	11,	22},\
	{	3,	3,	12,	23},\
	{	3,	3,	13,	23.5},\
}; 

// Cisco 915 MHz Card:
#define RSSI_BOARD_OFFSET_IR910_915	166.0
const tx_pow_t tx_pow_table_ir910_915[TX_POW_LUT_SIZE] = {\
	{	0,	3,	8,	-7},\
	{	0,	3,	11,	-3},\
	{	0,	3,	15,	0},\
	{	1,	3,	8,	2},\
	{	1,	3,	9,	4},\
	{	1,	3,	10,	6},\
	{	1,	3,	12,	8},\
	{	1,	3,	13,	9},\
	{	1,	3,	15,	10},\
	{	2,	3,	9,	12},\
	{	2,	3,	10,	13},\
	{	2,	3,	11,	15},\
	{	2,	3,	12,	16},\
	{	2,	3,	15,	18},\
	{	3,	3,	9,	20},\
	{	3,	3,	15,	24},\
}; 
#endif

#ifdef	IR910
extern tx_pow_t tx_pow_table[];
extern float fRSSI_BOARD_OFFSET;

int traceBoardCalibration()
{
	int	i;
        RTL_TRDBG(1,"pow_table:\n");
        for (i=0; i<TX_POW_LUT_SIZE; i++)
                RTL_TRDBG(1,"[%d,%d,%d,%d]\n", tx_pow_table[i].pa_gain,
			tx_pow_table[i].dac_gain, tx_pow_table[i].mix_gain, tx_pow_table[i].rf_power);
}

int setBoardCalibration()
{
	char	*band;

	band = IsmBand;

	if (!band || !*band)
		return -1;

	if (!strcmp(band, "eu433"))
	{
		RTL_TRDBG(1,"setBoardCalibration(%s)\n", band);
		memcpy(tx_pow_table, tx_pow_table_ir910_433, sizeof(tx_pow_table_ir910_433));
		fRSSI_BOARD_OFFSET = RSSI_BOARD_OFFSET_IR910_433;
	}
	else if (!strcmp(band, "eu868") || !strcmp(band, "eu8682015"))
	{
		RTL_TRDBG(1,"setBoardCalibration(%s)\n", band);
		memcpy(tx_pow_table, tx_pow_table_ir910_868, sizeof(tx_pow_table_ir910_868));
		fRSSI_BOARD_OFFSET = RSSI_BOARD_OFFSET_IR910_868;
	}
	else if (!strcmp(band, "us915"))
	{
		RTL_TRDBG(1,"setBoardCalibration(%s)\n", band);
		memcpy(tx_pow_table, tx_pow_table_ir910_915, sizeof(tx_pow_table_ir910_915));
		fRSSI_BOARD_OFFSET = RSSI_BOARD_OFFSET_IR910_915;
	}
	else
	{
		RTL_TRDBG(0,"setBoardCalibration(%s) error, use default values of eu868 !\n",band);
		memcpy(tx_pow_table, tx_pow_table_ir910_868, sizeof(tx_pow_table_ir910_868));
		fRSSI_BOARD_OFFSET = RSSI_BOARD_OFFSET_IR910_868;
		return -1;
	}

	traceBoardCalibration();

	return 0;
}
#else
int setBoardCalibration()
{
	RTL_TRDBG(0,"setBoardCalibration() not supported\n");
	return	0;
}
#endif

