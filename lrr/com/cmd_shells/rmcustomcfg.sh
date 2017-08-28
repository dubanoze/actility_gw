#!/bin/sh

#
#

case $# in
	0)
	rm $ROOTACT/usr/etc/lrr/lrr.ini
	rm $ROOTACT/usr/etc/lrr/lgw.ini
	rm $ROOTACT/usr/etc/lrr/channels.ini
	rm $ROOTACT/usr/etc/lrr/gpsman.ini
	;;
	*)
	while   [ $# -gt 0 ]
	do
		rm $ROOTACT/usr/etc/lrr/${1}
		shift
	done
	;;
esac
