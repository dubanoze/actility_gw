#!/bin/sh

cat $ROOTACT/var/log/lrr/DTC.log | sort
exit $?
