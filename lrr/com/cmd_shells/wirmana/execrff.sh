#!/bin/sh
#
#	Execute a reset from factory
#	

# set $ROOTACT
if	[ -z "$ROOTACT" ]
then
	case $(uname -n) in
		klk-wifc*)
			export ROOTACT=/user/actility
				;;
		*)
			export ROOTACT=/home/actility
			;;
	esac
fi

# check if $ROOTACT directory exists
if	[ ! -d "$ROOTACT" ]
then
	echo	"$ROOTACT does not exist"
	exit	1
fi

# set global variables
CONFFILE=$ROOTACT/usr/etc/lrr/_parameters.sh
LRRDIR=$ROOTACT/lrr
LRRCONF=$ROOTACT/usr/etc/lrr
SAVERFF=$ROOTACT/usr/etc/lrr/saverff_done
FORCED="0"

# check system
[ -f "$CONFFILE" ] && . $CONFFILE
if [ "$SYSTEM" != "wirmana" ]
then
	echo "This command is only for system wirmana ! Current system is [$SYSTEM]"
	exit	1
fi

# get command parameters
while	[ $# -gt 0 ]
do
	case	$1 in
		-L)
			shift
			LRRIDPARAM=$(echo ${1} | tr '[:upper:]' '[:lower:]')
		;;
		-F)
			shift
			FORCED="${1}"
		;;
	esac
	shift
done

# check if forced needed
if	[ "$FORCED" = "0" ]
then
	if	[ -f $ROOTACT/usr/etc/lrr/execrff_locked ]
	then
		echo	"reset feature disabled by ~/usr/etc/lrr/execrff_locked"
		exit 1
	fi
	if	[ ! -f ${SAVERFF} ]
	then
		echo	"rff not saved"
		exit	1
	fi
fi

# Check if -L option has been used
if	[ -z "$LRRIDPARAM" -a "$FORCED" = "0" ]
then
	echo "Option -L <lrrid> required. Command aborted."
	exit	1
fi

# Check lrrid
if	[ "$LRRIDPARAM" != "$LRRID" ]
then
	echo "Bad lrrid ('$LRRIDPARAM' != '$LRRID'). Command aborted."
	exit	1
fi

# Check if backup directory exists
SAVEDIR=/.update/backupactility
EXECRFFFILE=/.update/backupactility/RESTOREREQUESTED
if [ ! -d "$SAVEDIR" ]
then
	echo "Actility backup directory doesn't exist. Command aborted."
	exit 1
fi

# Need to save backup because it disappear after "kerosd -b"
# remount dir in readwrite mode
mount /dev/mmcblk3p2 /.update/ -o remount,rw
date >> $EXECRFFFILE
sync
# remount dir in readonly mode
mount /dev/mmcblk3p2 /.update/ -o remount,ro

# start reset factory
kerosd -b
echo "Reset factory in progress, reboot in 5 s ..."
$LRRDIR/com/cmd_shells/reboot_pending.sh 5

exit 0
