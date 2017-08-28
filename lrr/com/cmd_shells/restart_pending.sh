#!/bin/sh

exec > /dev/null 2>&1

DELAY="15"
if	[ "$1" != "" ]
then
	DELAY=$1
fi

sleep $DELAY

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi

if	[ -z "$SERVICELRR" ]
then
	killall lrr.x
	sleep 1
	killall -9 lrr.x
else
	$SERVICELRR restart
fi

exit 0
