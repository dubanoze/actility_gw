#!/bin/bash
# Just launch the script to perform a tar.gz with all binaries.

LRRFILE="$1"
OPKG_DIR="/tmp/deploy$$"
DEST_DIRECTORY="${OPKG_DIR}/user/actility"

if [ -z "$LRRFILE" -o ! -f "$LRRFILE" ]
then
	echo "Use: deploy.sh <lrrtarfile>"
	echo "Generate a .ipk file from a lrr .tar.gz file"
	exit 1
fi

mkdir -p $DEST_DIRECTORY
bn=$(basename $LRRFILE)
cp $LRRFILE /tmp
cd $DEST_DIRECTORY
tar xvf /tmp/$bn

VERSION="$(cat $DEST_DIRECTORY/lrr/Version)"
mkdir -p ${OPKG_DIR}/CONTROL
cat $DEST_DIRECTORY/lrr/opkg/CONTROL_wirmaar/control | sed "s/_REPLACE_VERSION_/$VERSION/" > ${OPKG_DIR}/CONTROL/control

cp $DEST_DIRECTORY/lrr/opkg/CONTROL_wirmaar/postinst ${OPKG_DIR}/CONTROL/postinst

$ROOTACT/opkg-build -o root -g root ${OPKG_DIR} $ROOTACT/deliveries

rm -rf $OPKG_DIR
