#!/bin/sh


GPSMAN="${ROOTACT}/usr/etc/lrr/gpsman.ini"

if      [ ! -f ${GPSMAN} ]  
then                        
	echo    "no GPS location set manually"
	exit    1            
fi                                   

rm -f ${GPSMAN}

$ROOTACT/lrr/com/cmd_shells/restart.sh 3
exit $?
