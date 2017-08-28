#!/bin/sh

file="$1"
if      [ -z $file ]
then
        exit    1
fi

if	[ -f $file ]
then
	tail -n50 $file
	exit $?
fi

if	[ -f $ROOTACT/$file ]
then
	tail -n50 $ROOTACT/$file
	exit $?
fi

if	[ -f $ROOTACT/var/log/lrr/$file ]
then
	tail -n50 $ROOTACT/var/log/lrr/$file
	exit $?
fi
        
exit	1
