#!/bin/bash

# Copyright (C) Actility, SA. All Rights Reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License version
# 2 only, as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License version 2 for more details (a copy is
# included at /legal/license.txt).
#
# You should have received a copy of the GNU General Public License
# version 2 along with this work; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
# 02110-1301 USA
#
# Please contact Actility, SA.,  4, rue Ampere 22300 LANNION FRANCE
# or visit www.actility.com if you need additional
# information or have any questions.

RESCUE="ppp0"
LOGFILE=${ROOTACT}/var/log/lrr/ifacerescuespv.log
RESCUESPV=1
RESCUECHECKFREQ=30
RESCUEINTERVPING=2
RESCUETMTPINGRESP=10

loadConfig()
{
	if [ -f $ROOTACT/usr/data/ifacefailover.cfg ] ; then
		. $ROOTACT/usr/data/ifacefailover.cfg
	fi
}

Log()
{
	d=`date "+%F %T"`
	echo $d $1 $2 $3 $4 $5 $6 $7 $8 $9 $10 >> ${LOGFILE}
	sz=`ls -s ${LOGFILE} | awk {'print $1'}`
	# sz is in kBytes
	if [ $sz -gt "1024" ]; then
		mv ${LOGFILE} ${LOGFILE}.backup
	fi
}

rescueSpv()
{
	if [ "$RESCUESPV" = "0" ]; then
		return 0
	fi

	/etc/init.d/gprs status > /dev/null 2>&1
	if [ $? != "0" ]; then
		Log "Start gprs service because status KO"
		/etc/init.d/gprs restart
		return 0
	fi

	if [ "${RESCUEROUTES}" != "" ]; then
		LSTROUTES=${RESCUEROUTES}
	else
		LSTROUTES=${ROUTES}
	fi

	if [ "${RESCUESUCCESSCOUNT}" != "" ]; then
		SUCCESS=${RESCUESUCCESSCOUNT}
	else
		SUCCESS=${SUCCESSCOUNT}
	fi

	cnt=0
	for i in ${LSTROUTES}
	do
		ping -w ${RESCUETMTPINGRESP} -q -c1 -I ${RESCUE} ${i} >/dev/null
		if [ $? = "0" ]; then
			cnt=$(expr $cnt + 1)
			Log "${RESCUE} ok with ${i}"
		else
			Log "${RESCUE} failed with ${i}"
		fi
		if [ $cnt -ge $SUCCESS ]; then
			return $cnt
		fi
		sleep ${RESCUEINTERVPING}
	done	
	if [ "$cnt" == "0" ]; then
		Log "Start gprs service because ping KO"
		/etc/init.d/gprs restart
		return 0
	fi
	return $cnt
}

checks()
{
	loadConfig
	rescueSpv
	sleep ${RESCUECHECKFREQ}
}

killExisting()
{
	if [ -f /var/run/ifacerescuespv.pid ]; then
		p=$(cat /var/run/ifacerescuespv.pid)
		name=$(cat /proc/${p}/cmdline 2>/dev/null | grep ifacerescuespv >/dev/null)
		if [ $? == "0" ]; then
			Log "Kill existing pid $p"
			kill -9 $p
		fi
	fi
	echo $$ >/var/run/ifacerescuespv.pid
}

killExisting

Log "#############################################"
Log "### Start $$"                                 
Log "#############################################"                

while true
do
	checks
done
