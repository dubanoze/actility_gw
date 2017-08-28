#!/bin/sh
#
#	Save lrr files in kerlink system in order to be restored
#	when doing a reset from factory


# set $ROOTACT
if	[ -z "$ROOTACT" ]
then
	if	[ -d /mnt/fsuser-1/actility ]
	then
		export ROOTACT=/mnt/fsuser-1/actility
	else
		export ROOTACT=/home/actility
	fi
fi

# check if $ROOTACT directory exists
if	[ ! -d "$ROOTACT" ]
then
	echo	"$ROOTACT does not exist"
	exit	1
fi

FORCED="0"

# get parameters
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
	if	[ -f $ROOTACT/usr/etc/lrr/saverff_locked ]
	then
		echo	"save rff feature disabled by ~/usr/etc/lrr/saverff_locked"
		exit 1
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

# set global variables
CONFFILE=$ROOTACT/usr/etc/lrr/_parameters.sh
LRRDIR=$ROOTACT/lrr
LRRCONF=$ROOTACT/usr/etc/lrr
SUPDIR="/home/support"
if [ ! -d "$SUPDIR" ]; then
	SUPDIR=""
fi
USERFILES="/etc/passwd /etc/group /etc/shadow /etc/gshadow"
USERFILES="$USERFILES /etc/profile /etc/shells /etc/usbkey.txt"
USERFILES="$USERFILES /etc/ntp.conf /knet/knetd.xml"
USERFILES="$USERFILES /etc/rc.d/init.d/network_functions /etc/sysconfig/network"
USERFILES="$USERFILES /etc/rc.d/init.d/lrr /etc/rc.d/rc*.d/*lrr*"
USERFILES="$USERFILES /etc/rc.d/init.d/firewall /etc/rc.d/rc*.d/*firewall*"
USERFILES="$USERFILES /etc/rc.d/init.d/settime"

TARDIR=/mnt/fsuser-1/dota
TMPFILE=/tmp/lstfile_$$
TARFILE=lrr.tar.gz

SAVERFF=$ROOTACT/usr/etc/lrr/saverff_done
VERSION=$(cat $ROOTACT/lrr/Version)
DATE=$(date -Iseconds)

tm=$(expr $DATE : '\(.*\)+.*')
tz=$(expr $DATE : '.*+\(.*\)')
hh=$(expr $tz : '\([0-9][0-9]\)[0-9][0-9]')
mm=$(expr $tz : '[0-9][0-9]\([0-9][0-9]\)')
DATE=${tm}.0+${hh}:${mm}



# check system
[ -f "$CONFFILE" ] && . $CONFFILE
if [ "$SYSTEM" != "wirmav2" ]
then
	echo "This command is only for system wirmav2 ! Current system is [$SYSTEM]"
	exit	1
fi

# create file containing RFF infos
echo	"RFFVERSION=${VERSION}"		> $SAVERFF
echo	"RFFDATE=${DATE}"		>> $SAVERFF

# create tar file
#find $LRRDIR $LRRCONF $USERFILES $SUPDIR -type f -print >$TMPFILE
find $LRRDIR $LRRCONF $USERFILES $SUPDIR -print >$TMPFILE
tar czf /tmp/$TARFILE -T $TMPFILE
rm -f $TMPFILE

# check tar file
tar tzf /tmp/$TARFILE >/dev/null 2>&1
if [ $? != 0 ]
then
	echo "Failed to create a valid /tmp/$TARFILE ! Command aborted"
	rm $SAVERFF
	exit	1
fi

# do what is required for being in reset from factory system
mv /tmp/$TARFILE $TARDIR/custo_$TARFILE
fw_setenv update 1
fw_setenv lrr_version "${VERSION}"
fw_setenv lrr_date "${DATE}"

#
# some other packages need to add some tarbal ?
#

lst=$(ls ${ROOTACT}/usr/etc/saverff/*.sh)
for i in $lst
do
	echo	"add other pkg to RFF from $i"
	sh {ROOTACT}/usr/etc/saverff/${i} > /dev/null 2>&1
done

echo "backup commited, reboot in progress ..."
$ROOTACT/lrr/com/cmd_shells/reboot.sh
exit $?
