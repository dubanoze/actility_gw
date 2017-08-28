#!/bin/sh
#
#	Save lrr files in kerlink system in order to be restored
#	when doing a reset from factory

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
SAVEDIR=/.update/backupactility
OPKGBUILD=$ROOTACT/lrr/com/cmd_shells/wirmana/act-opkg-build.sh

TMPFILE=/tmp/lstfile_$$
TARFILE=lrr.tar.gz
CONFTARFILE=lrrconf.tar.gz

SAVERFF=$ROOTACT/usr/etc/lrr/saverff_done
VERSION=$(cat $ROOTACT/lrr/Version)
DATE=$(date +%FT%T%z)

tm=$(expr $DATE : '\(.*\)+.*')
tz=$(expr $DATE : '.*+\(.*\)')
hh=$(expr $tz : '\([0-9][0-9]\)[0-9][0-9]')
mm=$(expr $tz : '[0-9][0-9]\([0-9][0-9]\)')
DATE=${tm}.0+${hh}:${mm}



# check system
[ -f "$CONFFILE" ] && . $CONFFILE
if [ "$SYSTEM" != "wirmana" ]
then
	echo "This command is only for system wirmana ! Current system is [$SYSTEM]"
	exit	1
fi

echo "Preparing files for saverff, please wait"
# create file containing RFF infos
echo	"RFFVERSION=${VERSION}"		> $SAVERFF
echo	"RFFDATE=${DATE}"		>> $SAVERFF

echo "saving lrr ..."
# create tar file
find $LRRDIR -type f -print >$TMPFILE
tar czf /tmp/$TARFILE -T $TMPFILE 2>/dev/null

# create conf tar file
echo "saving configuration ..."
netcfgfile="/user/rootfs_rw/etc/network/connman/lan.config"
netcfgfile2="/user/rootfs_rw/etc/network/connman/wlan.config"
find $LRRCONF $netcfgfile $netcfgfile2 -type f -print >$TMPFILE
tar czf /tmp/$CONFTARFILE -T $TMPFILE 2>/dev/null

cd /tmp

#
# some other packages need to add some tarbal ?
#

lst=$(ls ${ROOTACT}/usr/etc/saverff/*.sh 2>/dev/null)
for i in $lst
do
	echo	"add other pkg to RFF from $i"
	sh ${i} > /dev/null 2>&1
done

#
# save in backup
#
# remount dir in readwrite mode
mount /dev/mmcblk3p2 /.update/ -o remount,rw
[ ! -d "$SAVEDIR" ] && mkdir -p $SAVEDIR
mv /tmp/$TARFILE $SAVEDIR
mv /tmp/$CONFTARFILE $SAVEDIR
# remount dir in readonly mode
mount /dev/mmcblk3p2 /.update/ -o remount,ro

echo "saverff done"
exit $?
