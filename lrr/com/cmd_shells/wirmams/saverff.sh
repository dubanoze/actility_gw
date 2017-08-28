#!/bin/sh
#
#	Save lrr files in kerlink system in order to be restored
#	when doing a reset from factory


# set $ROOTACT
if	[ -z "$ROOTACT" ]
then
	case $(uname -n) in
		klk-lpbs*)
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
OPKGBUILD=$ROOTACT/lrr/com/cmd_shells/wirmams/act-opkg-build.sh

TMPFILE=/tmp/lstfile_$$
TMPDIR=/tmp/_doipk_$$
TARFILE=lrr.tar.gz

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
if [ "$SYSTEM" != "wirmams" ]
then
	echo "This command is only for system wirmams ! Current system is [$SYSTEM]"
	exit	1
fi

echo "Preparing files for saverff, this can take few minutes, please wait ..."
# create file containing RFF infos
echo	"RFFVERSION=${VERSION}"		> $SAVERFF
echo	"RFFDATE=${DATE}"		>> $SAVERFF

# create tar file
find $LRRDIR -type f -print >$TMPFILE
tar cf /tmp/$TARFILE -T $TMPFILE 2>/dev/null
mkdir -p $TMPDIR
cd $TMPDIR
tar xf /tmp/$TARFILE

# do file required for ipk
mkdir -p $TMPDIR/CONTROL
VERSION="$(cat $ROOTACT/lrr/Version)"
cat $LRRDIR/opkg/CONTROL_wirmams/control | sed "s/_REPLACE_VERSION_/$VERSION/" > $TMPDIR/CONTROL/control
cp $LRRDIR/opkg/CONTROL_wirmams/postinst $TMPDIR/CONTROL/postinst


# generate .ipk file
rm -f /user/.updates/*.ipk
$OPKGBUILD $TMPDIR /user/.updates

lrripk=$(ls /user/.updates/lrr_*.ipk 2>/dev/null)
if [ -z "$lrripk" ]
then
	echo "Error during lrr ipk creation. saverff aborted"
	exit 1
fi

rm -rf $TMPDIR

# create tar file
find $LRRCONF /user/rootfs_rw/etc/network/interfaces -type f -print >$TMPFILE
tar cf /tmp/$TARFILE -T $TMPFILE 2>/dev/null
mkdir -p $TMPDIR
cd $TMPDIR
tar xf /tmp/$TARFILE
mv ./user/rootfs_rw/etc/network/interfaces ./user/rootfs_rw/etc/network/interfaces.sav

# do file required for ipk
mkdir -p $TMPDIR/CONTROL
VERSIONCONF="$(date +%y%m%d.%H%M)"
cat > $TMPDIR/CONTROL/control << EOF
Package: lrrconf
Version: ${VERSIONCONF}
Architecture: cortexa9hf-vfp-neon
Maintainer: Actility Support <support@actility.com>
Priority: optional
Section: misc
Source: http://www.actility.com
Description: lrr configuration
EOF

cat > $TMPDIR/CONTROL/preinst << EOF2
#!/bin/sh

rm -f /user/actility/usr/etc/lrr/*.ini
exit 0
EOF2
chmod +x $TMPDIR/CONTROL/preinst

cat > $TMPDIR/CONTROL/postinst << EOF3
#!/bin/sh

mv /user/rootfs_rw/etc/network/interfaces.sav /user/rootfs_rw/etc/network/interfaces
exit 0
EOF3
chmod +x $TMPDIR/CONTROL/postinst

# generate .ipk file
$OPKGBUILD $TMPDIR /user/.updates

lrripk=$(ls /user/.updates/lrrconf_*.ipk 2>/dev/null)
if [ -z "$lrripk" ]
then
	echo "Error during lrr ipk creation. saverff aborted"
	exit 1
fi

rm -rf $TMPDIR
cd /tmp

#
# some other packages need to add some tarbal ?
#

lst=$(ls ${ROOTACT}/usr/etc/saverff/*.sh)
for i in $lst
do
	echo	"add other pkg to RFF from $i"
	sh ${i} > /dev/null 2>&1
done

#
# save in backup
#
dirbackupact="/.update/packages/backupactility"
dirbackup="/.update/packages/backup"
# remount dir in readwrite mode
mount /dev/mmcblk3p2 /.update/ -o remount,rw
[ ! -d "$dirbackupact" ] && mkdir -p $dirbackupact
rm -f $dirbackupact/*.ipk
cp /user/.updates/*.ipk $dirbackupact
rm -f $dirbackup/lrr*.ipk
cp /user/.updates/*.ipk $dirbackup
sync
# remount dir in readonly mode
mount /dev/mmcblk3p2 /.update/ -o remount,ro

# prepare installation of ipk
sync
kerosd -u

echo "backup commited, reboot in progress ..."
$ROOTACT/lrr/com/cmd_shells/reboot.sh
exit $?
