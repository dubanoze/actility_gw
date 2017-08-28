#!/bin/sh



# check if $ROOTACT directory exists
if      [ ! -d "$ROOTACT" ]
then
        echo    "$ROOTACT does not exist"
        exit    1
fi

CONFFILE=$ROOTACT/usr/etc/lrr/_parameters.sh
SAVERFF=$ROOTACT/usr/etc/lrr/saverff_done

# check system
[ -f "$CONFFILE" ] && . $CONFFILE
if [ "$SYSTEM" != "ciscoms" ]
then
        echo "This command is only for system ciscoms! Current system is [$SYSTEM]"
        exit    1
fi

if      [ ! -f $SAVERFF ]
then
        exit    1
fi

cat $SAVERFF
exit $?