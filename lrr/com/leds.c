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

/*! @file leds.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "rtlbase.h"

#ifdef WIRMANA

#define LED_TX_LORA		"/sys/class/leds/led3:red:lora"
#define LED_RX_LORA		"/sys/class/leds/led3:green:lora"
#define LED_NOK_BACKHAUL	"/sys/class/leds/led2:red:backhaul"
#define LED_BACKHAUL		"/sys/class/leds/led2:green:backhaul"
#define BUFFER_MAX_SIZE		128

int LedWrite(char *path, char *str)
{
	int ret = 0;
	int fd = -1;

	fd = open(path, O_WRONLY);
	if (fd >= 0)
	{
		if (write(fd, str, strlen(str)) < 0)
		{
			RTL_TRDBG(0, "Unable to write in '%s'\n", path);
			ret = -1;
		}
		close(fd);
	}
	else
	{
		RTL_TRDBG(0, "Unable to open '%s' (%s)\n", path, strerror(errno));
		ret = -1;
	}
	return ret;
}

int LedConfigure()
{
	LedWrite(LED_TX_LORA"/trigger", "oneshot");
	LedWrite(LED_RX_LORA"/trigger", "oneshot");

	return 0;
}

int LedShotTxLora()
{
	LedWrite(LED_TX_LORA"/shot", "1");
	return 0;
}

int LedShotRxLora()
{
	LedWrite(LED_RX_LORA"/shot", "1");
	return 0;
}

int LedBackhaul(int val)
{
	static int	previous = -1;

	if (val == previous)
		return 0;

	switch (val)
	{
		case 0:
			LedWrite(LED_BACKHAUL"/trigger", "none");
			LedWrite(LED_BACKHAUL"/brightness", "0");
			LedWrite(LED_NOK_BACKHAUL"/trigger", "none");
			LedWrite(LED_NOK_BACKHAUL"/brightness", "0");
			previous = 0;
			break;
		case 1:
			LedWrite(LED_NOK_BACKHAUL"/trigger", "timer");
			LedWrite(LED_BACKHAUL"/brightness", "0");
			previous = 1;
			break;
		case 2:
			LedWrite(LED_BACKHAUL"/brightness", "1");
			LedWrite(LED_NOK_BACKHAUL"/trigger", "none");
			LedWrite(LED_NOK_BACKHAUL"/brightness", "0");
			previous = 2;
			break;
	}
	return 0;
}

#endif	// WIRMANA
