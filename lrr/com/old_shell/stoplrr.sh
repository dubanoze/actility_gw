#!/bin/sh

#
#

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then
	. ${ROOTACT}/lrr/com/_functions.sh
fi


lrr_init_off
#kill -HUP 1
lrr_telinit
lrr_telinit
lrr_init_on

killall startlrr_pending.sh

cd $ROOTACT/lrr/com

# PATH for nohup command
PATH=/usr/local/bin:$PATH
rm nohup.out
nohup ./startlrr_pending.sh &
