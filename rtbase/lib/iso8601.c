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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>



/**
 ** @brief Formats an ISO time
 ** @param A time_t (Unix time)
 ** @param The output buffer
 ** @return void
 **/
void rtl_gettimeofday_to_iso8601date(struct timeval *tv, struct timezone *tz, char *buf) {
	struct tm sttm;
        struct tm *tm = localtime_r(&tv->tv_sec,&sttm);
        int h, m;
        h = tm->tm_gmtoff/3600;
        m = (tm->tm_gmtoff - h*3600) / 60;
        char sign = (h<0) ? '-':'+';
        sprintf (buf, "%04d-%02d-%02dT%02d:%02d:%02d.%ld%c%02d:%02d",
                tm->tm_year + 1900, tm->tm_mon + 1,
                tm->tm_mday, tm->tm_hour, tm->tm_min, 
		tm->tm_sec, tv->tv_usec / 1000,
                sign, abs(h), abs(m));
}

void rtl_getCurrentIso8601date(char *buf) {
        struct timeval tv;
        struct timezone tz;
        gettimeofday(&tv, &tz);
        rtl_gettimeofday_to_iso8601date(&tv, &tz, buf);
}

char *findSign(char *pt) {
	while (*pt) {
		if (*pt == '+')
			return pt;
		if (*pt == '-')
			return pt;
		pt++;
	}
	return NULL;
}

// 2016-08-26T08:55:47
// 2016-01-11T14:28:00+02:00
time_t rtl_iso8601_to_Unix(char *iso, int convertUTC) {
	struct tm tm;

	if	(!iso || strlen(iso) < 19)
		return 0;
	if	((iso[4] != '-') || (iso[7] != '-') || (iso[10] != 'T') || (iso[13] != ':') || (iso[16] != ':'))
		return 0;

	memset (&tm, 0, sizeof(tm));

	tm.tm_year = (iso[0]-'0')*1000 + (iso[1]-'0')*100 + (iso[2]-'0') * 10 + (iso[3]-'0') - 1900;
	tm.tm_mon = (iso[5]-'0')*10 + (iso[6]-'0') - 1;
	tm.tm_mday = (iso[8]-'0')*10 + (iso[9]-'0');
	tm.tm_isdst = -1;
	tm.tm_hour = (iso[11]-'0')*10 + (iso[12]-'0');
	tm.tm_min = (iso[14]-'0')*10 + (iso[15]-'0');

	// Seconds are decimal
	char tmp[7];
	memcpy (tmp, iso+17, 6);
	tmp[6] = 0;
	double seconds = atof(tmp);
	tm.tm_sec	= (int)seconds;

	/* determine number of seconds since the epoch, i.e. Jan 1st 1970 */
	time_t epoch_seconds = timegm(&tm);

	if	(convertUTC) {
		char *sign = findSign(iso+19);
		if	(sign) {
			int offset;
			offset	= ((sign[1]-'0')*10 + (sign[2]-'0')) * 3600;
			offset	+= ((sign[4]-'0')*10 + (sign[5]-'0')) * 60;
			if	(*sign == '+')
				epoch_seconds	-= offset;
			else
				epoch_seconds	+= offset;
		}
	}
	return epoch_seconds;
}

int rtl_iso8601ToTv(char *iso, struct timeval *tv, int convertUTC) {
	struct tm tm;

	if	(!iso || strlen(iso) < 17)
		return -1;

	memset (&tm, 0, sizeof(tm));

	tm.tm_year = (iso[0]-'0')*1000 + (iso[1]-'0')*100 + (iso[2]-'0') * 10 + (iso[3]-'0') - 1900;
	tm.tm_mon = (iso[5]-'0')*10 + (iso[6]-'0') - 1;
	tm.tm_mday = (iso[8]-'0')*10 + (iso[9]-'0');
	tm.tm_isdst = -1;
	tm.tm_hour = (iso[11]-'0')*10 + (iso[12]-'0');
	tm.tm_min = (iso[14]-'0')*10 + (iso[15]-'0');

	// Seconds are decimal
	char tmp[7];
	memcpy (tmp, iso+17, 6);
	tmp[6] = 0;
	double seconds = atof(tmp);
	tm.tm_sec	= (int)seconds;
	int usec = atoi(tmp+3);

	/* determine number of seconds since the epoch, i.e. Jan 1st 1970 */
	time_t epoch_seconds = timegm(&tm);

	if	(convertUTC) {
		char *sign = findSign(iso+19);
		if	(sign) {
			int offset;
			offset	= ((sign[1]-'0')*10 + (sign[2]-'0')) * 3600;
			offset	+= ((sign[4]-'0')*10 + (sign[5]-'0')) * 60;
			if	(*sign == '+')
				epoch_seconds	-= offset;
			else
				epoch_seconds	+= offset;
		}
	}
	tv->tv_sec	= epoch_seconds;
	tv->tv_usec = usec;
	return 0;
}

void rtl_gmtToIso8601(char *buf) {
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	struct tm *tm = gmtime(&tv.tv_sec);
	sprintf (buf, "%04d-%02d-%02dT%02d:%02d:%02d.%ldZ",
		tm->tm_year + 1900, tm->tm_mon + 1,
		tm->tm_mday, tm->tm_hour, tm->tm_min,
		tm->tm_sec, tv.tv_usec / 1000);
}

void rtl_nanoToIso8601(time_t sec, uint32_t nsec, char *buf) {
	struct tm *tm = gmtime(&sec);
	sprintf (buf, "%04d-%02d-%02dT%02d:%02d:%02d.%uZ",
		tm->tm_year + 1900, tm->tm_mon + 1,
		tm->tm_mday, tm->tm_hour, tm->tm_min,
		tm->tm_sec, nsec);
}
