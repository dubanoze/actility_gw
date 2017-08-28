#!/bin/sh

LRC="${LRCPRIMARY}"
if	[ -z "$LRC" ]
then
	LRC=lrc1.thingpark.com
fi

if	[ $# = "0" ]
then
	ping -c 10 $LRC
else
	ping -c 10 $*
fi
exit $?
