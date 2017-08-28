#!/bin/sh

exec > /dev/null 2>&1

DELAY="15"
if	[ "$1" != "" ]
then
	DELAY=$1
fi
sleep $DELAY
reboot
