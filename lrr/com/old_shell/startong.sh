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


# PATH for nohup command
PATH=/usr/local/bin:$PATH
if [ -d "${ROOT_ONG}" ]
then
	cd ${ROOT_ONG}/etc
# use nohup oherwise some processes get a signal 15
	rm nohup.out
	nohup ./ong start &
else
	echo	"ONG not installed"
fi

