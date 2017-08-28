#!/bin/sh

if	[ "$1" = "lrr" ]
then
	tail -n50 $ROOTACT/var/log/lrr/TRACE.log
	exit $?
fi

if	[ "$1" = "iface" ]
then
	tail -n50 $ROOTACT/var/log/lrr/ifacefailover.log
	exit $?
fi

if	[ "$1" = "system" ]
then
	tail -n50 /var/log/messages
	exit $?
fi

if	[ "$1" = "dtc" ]
then
	cat $ROOTACT/var/log/lrr/DTC.log
	exit $?
fi


tail -n50 $ROOTACT/var/log/lrr/TRACE.log
exit $?
