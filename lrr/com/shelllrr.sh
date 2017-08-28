#!/bin/sh

#PATH=${PATH//".:"/}
#PATH=${PATH//":.:"/}
export PATH=.:$PATH

LRRCMDSERIAL="0"
export LRRCMDSERIAL

COMMAND="${1}"
shift

DATE=$(date +%FT%T%z)

if	[ -z "$COMMAND" ]
then
	echo	"#no command"
	exit	1
fi

if	[ "${1}" = "-Z" ]
then
	shift
	LRRCMDSERIAL="${1}"
	export LRRCMDSERIAL
	shift
fi

if	[ "$COMMAND" = "stoplrr" -o "$COMMAND" = "stoplrr.sh" ]
then
	echo	"#lrr can not stop itself"
	exit	1
fi

if	[ "$COMMAND" = "startlrr" -o "$COMMAND" = "startlrr.sh" ]
then
	echo	"#lrr can not start itself"
	exit	1
fi

if	[ -z "$ROOTACT" ]
then
	export ROOTACT=/mnt/fsuser-1/actility
fi

if	[ ! -d "$ROOTACT" ]
then
	echo	"#ROOTACT does not exist"
	exit	1
fi

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then
	. ${ROOTACT}/lrr/com/_functions.sh
fi

lrr_CloseFiles

# old installs do not have SYSTEM in _parameters.sh
if	[ -z "$SYSTEM" ]
then
	SYSTEM=$LRRSYSTEM
fi
export SYSTEM

echo	"#"
echo	"#DATE=$DATE"
#echo	"#SYSTEM=$SYSTEM"
echo	"#LRRSYSTEM=$LRRSYSTEM"
echo	"#ROOTACT=$ROOTACT"
echo	"#LRRCMDSERIAL=$LRRCMDSERIAL"

cd $ROOTACT
DIRCUSTOM=$ROOTACT/usr/etc/lrr/cmd_shells
DIRCOMMAND=$ROOTACT/lrr/com/cmd_shells

# command specific to the customer
if	[ -x ${DIRCUSTOM}/${COMMAND}.sh ]
then
	echo "#${DIRCUSTOM}/${COMMAND}.sh $*"
	echo "#"
	exec ${DIRCUSTOM}/${COMMAND}.sh $*
	exit $?
fi

# command specific to the system but in the package
if	[ -x ${DIRCOMMAND}/${LRRSYSTEM}/${COMMAND}.sh ]
then
	echo "#${DIRCOMMAND}/${LRRSYSTEM}/${COMMAND}.sh $*"
	echo "#"
	exec ${DIRCOMMAND}/${LRRSYSTEM}/${COMMAND}.sh $*
	exit $?
fi

# generic command
if	[ -x ${DIRCOMMAND}/${COMMAND}.sh ]
then
	echo "#${DIRCOMMAND}/${COMMAND}.sh $*"
	echo "#"
	exec ${DIRCOMMAND}/${COMMAND}.sh $*
	exit $?
fi

echo	"#command ${COMMAND}.sh not found"

exit 1
