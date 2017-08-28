#!/bin/sh
#
#	Execute a reset from factory
#	


# set $ROOTACT
if	[ -z "$ROOTACT" ]
then
	export ROOTACT=/home/actility
fi

# check if $ROOTACT directory exists
if	[ ! -d "$ROOTACT" ]
then
	echo	"$ROOTACT does not exist"
	exit	1
fi

# set global variables
CONFFILE=$ROOTACT/usr/etc/lrr/_parameters.sh
LRRDIR=$ROOTACT/lrr
LRRCONF=$ROOTACT/usr/etc/lrr
SAVERFF=$ROOTACT/usr/etc/lrr/saverff_done
FORCED="0"

# check system
[ -f "$CONFFILE" ] && . $CONFFILE
if [ "$SYSTEM" != "tektelic" ]
then
	echo "This command is only for system tektelic ! Current system is [$SYSTEM]"
	exit	1
fi

if	[ ! -f $SAVERFF ]
then
	exit	1
fi

cat $SAVERFF
exit $?
