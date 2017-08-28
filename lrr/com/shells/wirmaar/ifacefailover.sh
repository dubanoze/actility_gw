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

PRINCIPAL="eth0"
RESCUE="wwan1"
ROUTES="lrc1.thingpark.com lrc2.thingpark.com"
SUCCESSCOUNT=1
CHECKFREQ=30
RETURNFREQ=120
LOGFILE=${ROOTACT}/var/log/lrr/ifacefailover.log
RESCUESPV=1
INTERVPING=1
INTERVADDRT=1
COUNTADDRT=5
TMTPINGRESP=5


CURRENT="ERROR"

loadConfig()
{
	if [ -f $ROOTACT/usr/data/ifacefailover.cfg ] ; then
		. $ROOTACT/usr/data/ifacefailover.cfg
	fi
}

Log()
{
	d=`date "+%F %T"`
	echo $d $1 $2 $3 $4 $5 $6 $7 $8 $9 ${10} >> ${LOGFILE}
	sz=`ls -s ${LOGFILE} | awk {'print $1'}`
	# sz is in kBytes
	if [ $sz -gt "1024" ]; then
		mv ${LOGFILE} ${LOGFILE}.backup
	fi
}

ipAddrFmt()
{
if expr "$1" : '[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$' >/dev/null
then
	return 0
else
	return 1
fi
}

checkGatewayFile()
{
	gw=$(route -n | grep "^0.0.0.0" | grep "$PRINCIPAL" | awk '{ print $2 }')
#	Log "Gateway = '$gw'"
	if [ -z "$gw" ]
	then
#		Log "Gateway empty for eth0, nothing done"
		return 0
	fi

	# check if gateway changed
	if [ -f "/tmp/gateway.${PRINCIPAL}" ]
	then
		oldgw=$(cat "/tmp/gateway.${PRINCIPAL}")
		if [ "$gw" = "$oldgw" ]
		then
#			Log "Gateway didn't change"
			return 0
		fi
	fi

	# create /tmp/gateway... file
	echo "$gw" > /tmp/gateway.${PRINCIPAL}
	Log "File /tmp/gateway.${PRINCIPAL} updated = '$(cat /tmp/gateway.${PRINCIPAL})'"
}

delRoutes()
{
	for i in ${ROUTES}
	do
		route del ${i} 2>/dev/null
	done
}

addOneRoute()
{
	countaddrt=0
	maxtry=$COUNTADDRT

	ipAddrFmt ${1}
	if [ $? == 0 ]
	then
# the target is an ip address no reason than route add fails => only 2 tries
		maxtry=2
	fi

	while [ $countaddrt -lt $maxtry ]
	do
		route del ${1} 2>/dev/null
		countaddrt=$(expr $countaddrt + 1)
		route add -host ${1} gw ${2} netmask 0.0.0.0 metric 0 dev ${3} 2>/dev/null
		if [ $? == "0" ]; then
			return 0
		fi
		Log "route add failure ${1}"
		sleep ${INTERVADDRT}
	done
	return 1
}

#
#	Search gateway used for PRINCIPAL interface and create
#	routes using these gateway and interface
#
addRoutes()
{
	added=0
	ifconfig ${PRINCIPAL} >/dev/null 2>&1
	if [ $? != "0" ]; then
		Log "### No ${PRINCIPAL} main iface"
		return 0
	fi
	if [ ! -f /tmp/gateway.${PRINCIPAL} ]
	then
		Log "file gateway.${PRINCIPAL} does not exist"
		return 0
	fi
	gw=`cat /tmp/gateway.${PRINCIPAL}`
	if [ "$gw" == "" ]; then
		Log "trying to add route no gateway for ${PRINCIPAL}"
		return 0
	fi
	for i in ${ROUTES}
	do
#		route add -host ${i} gw ${gw} netmask 0.0.0.0 metric 0 dev ${PRINCIPAL} 2>/dev/null
		addOneRoute ${i} ${gw} ${PRINCIPAL}
		if [ $? == "1" ]; then
			Log "can not add route ${i} on ${PRINCIPAL}"
		else
			added=$(expr $added + 1)
		fi
	done
	return $added
}

#check_v1()
#{
#	addRoutes
#	if [ $? == "1" ]; then
#		Log "addRoutes failed"
#		return 0
#	fi
#	
#	cnt=0
#	for i in ${URLS}
#	do
#		wget -qs ${i}
#		if [ $? == "0" ]; then
#			cnt=$(expr $cnt + 1)
#			Log "${PRINCIPAL} ok with ${i}"
#		else
#			Log "${PRINCIPAL} failed with ${i}"
#		fi
#	done	
#	delRoutes
#	return $cnt
#}

check_v2()
{
	addRoutes
	if [ $? == "0" ]; then
		Log "addRoutes full failed"
		return 0
	fi
	
	cnt=0
	for i in ${ROUTES}
	do
		ping -w ${TMTPINGRESP} -q -c1 -I ${PRINCIPAL} ${i} >/dev/null
		if [ $? == "0" ]; then
			cnt=$(expr $cnt + 1)
			Log "${PRINCIPAL} ok with ${i}"
		else
			Log "${PRINCIPAL} failed with ${i}"
		fi
		if [ $cnt -ge $SUCCESSCOUNT ]; then
			delRoutes
			return $cnt
		fi
		sleep ${INTERVPING}
	done	
	delRoutes
	return $cnt
}

use_principal_v2()
{
	ifconfig ${PRINCIPAL} >/dev/null 2>&1
	if [ $? != "0" ]; then
		Log "### No ${PRINCIPAL} main iface"
		return 1
	fi
	if [ ! -f /tmp/gateway.${PRINCIPAL} ]; then
		Log "### file gateway.${PRINCIPAL} does not exist"
		return 1
	fi
	#Log "Use Primary ${PRINCIPAL}"
	route del default         
	route add default gw `cat /tmp/gateway.${PRINCIPAL}` netmask 0.0.0.0 metric 0 dev ${PRINCIPAL}
	if [ $? != "0" ]; then
		Log "### default route on ${PRINCIPAL} error"
		return 1
	fi
	if [ ! -f /tmp/resolv.conf.${PRINCIPAL} ]; then
		Log "/tmp/resolv.conf.${PRINCIPAL} not exits use gateway as DNS"
		cp -f /tmp/gateway.${PRINCIPAL} /etc/resolv.conf
	else
		cp -f /tmp/resolv.conf.${PRINCIPAL} /etc/resolv.conf
	fi
	return 0
}

use_rescue_v2()
{
	ifconfig ${RESCUE} >/dev/null 2>&1
	if [ $? != "0" ]; then
		Log "### No ${RESCUE} rescue iface"
		return 1
	fi
	
	#Log "Use Rescue ${RESCUE}"
	route del default         
	route add default dev ${RESCUE}
	if [ $? != "0" ]; then
		Log "### default route on ${RESCUE} error"
		return 1
	fi
	cp -f /etc/ppp/resolv.conf /etc/resolv.conf
	return 0
}

# Choice of functions : v1 or v2
check=check_v2
use_principal=use_principal_v2
use_rescue=use_rescue_v2


checks()
{
	loadConfig
	
	checkGatewayFile

	$check $i
	cnt=$?

	if [ "${CURRENT}" != "ERROR" ]; then
#		defroute="$(route | grep "default" | awk '{print $8}')"
		defroute="$(route -n | grep "^0.0.0.0" | sed -n '1p' | awk '{print $8}')"
		if [ "$defroute" != "${CURRENT}" ]; then
			Log "### def route $defroute doesnt match ${CURRENT}"
			CURRENT="ERROR"
#		else
#			Log "def route $defroute match ${CURRENT}"
		fi
	fi
	
	if [ $cnt -ge $SUCCESSCOUNT ]; then
#		$use_principal
		if [ "${CURRENT}" != "${PRINCIPAL}" ]; then
			Log "Set and use principal ${PRINCIPAL}"
			$use_principal
			if [ $? == "0" ]; then
				CURRENT=${PRINCIPAL} 
			else
				CURRENT="ERROR"
			fi
		fi
		Log "Stay ${CHECKFREQ} sec on ${PRINCIPAL}/${CURRENT}"
		sleep ${CHECKFREQ}
	else
#		$use_rescue
		if [ "${CURRENT}" != "${RESCUE}" ]; then
			Log "Set and use rescue ${RESCUE}"
			$use_rescue
			if [ $? == "0" ]; then
				CURRENT=${RESCUE} 
			else
				CURRENT="ERROR"
			fi
		fi
		if [ "${CURRENT}" = "${RESCUE}" ]; then
			Log "Stay ${RETURNFREQ} sec on ${RESCUE}/${CURRENT}"
			sleep ${RETURNFREQ}
		else
			Log "Stay ${CHECKFREQ} sec on ${PRINCIPAL}/${CURRENT}"
			sleep ${CHECKFREQ}
		fi
	fi
}

killExisting()
{
	if [ -f /var/run/ifacefailover.pid ]; then
		p=$(cat /var/run/ifacefailover.pid)
		name=$(cat /proc/${p}/cmdline 2>/dev/null | grep ifacefailover >/dev/null)
		if [ $? == "0" ]; then
			Log "Kill existing pid $p"
			kill -9 $p
		fi
	fi
	echo $$ >/var/run/ifacefailover.pid
}

rescueSpv()
{
	if [ "${RESCUESPV}" = "1" ]; then
	$ROOTACT/lrr/com/shells/wirmaar/ifacerescuespv.sh > /dev/null 2>&1 &
	fi
}

loadConfig
killExisting
rescueSpv

Log "#############################################"
Log "### Start $$"                                 
Log "#############################################"                
Log "sleep 5"
checkGatewayFile
sleep 5
ifconfig >>${LOGFILE}                                               
route -n >>${LOGFILE}                                      


while true
do
	checks
done
