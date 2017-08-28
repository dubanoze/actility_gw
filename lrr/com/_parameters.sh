#!/bin/sh


INIT_PROF="/etc/profile"
INIT_FILE="/etc/inittab"
INIT_COMM="lrr/com/initlrr.sh -s inittab"

# on kerlink the default runlevel is 3
INIT_RUNL="3"

# reload local parameters mainly to redefine the runlevel
if [ -f ${ROOTACT}/usr/etc/lrr/_parameters.sh ]
then
	. ${ROOTACT}/usr/etc/lrr/_parameters.sh
fi

EXPR_LINE="lr:[0-9]*:respawn:${ROOTACT}/${INIT_COMM}"
INIT_PREF="lr:${INIT_RUNL}:respawn"
INIT_LINE="${INIT_PREF}:${ROOTACT}/${INIT_COMM}"


# on kerlink we have an ONG	(not mandatory)
ROOT_ONG="/mnt/fsuser-1/ong"
