#!/bin/bash
# $1 is the day of the week [1..7]

BACKUPFS=/home

# Note: this directory is also hard coded in suplog, if changed here
# it must also be changed in suplog
BACKUPDIR=${BACKUPFS}/actility/traces


if      [ $# = "0" ]
then         
	exit 1
fi

cd $ROOTACT/var/log/lrr

if	[ ! -d ${BACKUPFS} ]
then
	rm TRACE_0?.log
	exit 0
fi

if	[ ! -d ${BACKUPDIR} ]
then
	mkdir -p ${BACKUPDIR}
fi

# No ifacefailover.log.backup yet for fclamp
#mv ifacefailover.log.backup ${BACKUPDIR}

# do not backup the traces of the day
for f in 1 2 3 4 5 6 7
do
	if	[ $f != "${1}" ]
	then
		tr=TRACE_0${f}.log
		if	[ -f ${tr} ]
		then
			echo "mv $tr ${BACKUPDIR}"
			mv $tr ${BACKUPDIR}
		fi
	fi
done


exit 0
