#!/bin/sh

DELAY="15"
if	[ "$1" != "" ]
then
	DELAY=$1
fi
echo    "lrr will be restarted in $DELAY sec"
nohup $ROOTACT/lrr/com/cmd_shells/restart_pending.sh $DELAY > /dev/null 2>&1 &
exit 0
