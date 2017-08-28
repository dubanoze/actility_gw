#!/bin/bash
#
# usage: `basename $0` <top level pkt forwarder/LRR directory path> <output pkg file prefix> <private-key-file for signing>"
# The top directory must have a scripts sub-dir that must have:"
#   MANIFEST - file with version detail"
#   START - script to start the program"
#   STOP  - script to stop the program"
#   RESTART - script to restart the program"
#   STATUS - script to check running status of the program. Return 0 if running. 1 if not running"
#   VERSION - script to return program version"
#   POSTINSTALL - script to run after the package is installed"
#   PREINSTALL - script to run before packet is un-installed"
# See each script for additional information"

LRRDIR="$ROOTACT/lrr"
LRRFILE="$1"
LRROUTFILE="$(echo $LRRFILE | sed 's/.tar.gz/.cpkg/')"
TMPFILE=/tmp/makelrrcpkg$$

if [ -z "$ROOTACT" ]
then
	echo "ROOTACT not set"
	exit 1
fi

if [ ! -f "$LRRFILE" ]
then
	echo "Cannot find file '$LRRFILE'"
	exit 1
fi

case $LRRFILE in
	*.tar.gz)
		;;
	*)	echo "File '$LRRFILE' is not a .tar.gz file !"
		exit 1
		;;
esac

# get list of files from tar.gz
tar tvf $LRRFILE > $TMPFILE

res=$(grep "lrr/MANIFEST" $TMPFILE)
if [ -z "$res" ]
then
	echo "File lrr/MANIFEST not found in $LRRFILE"
	exit 1
fi

scripts="START STOP RESTART STATUS VERSION POSTINSTALL PREUNINSTALL"

for script in $scripts
do
	res=$(grep "lrr/scripts/$script" $TMPFILE)
	if [ -z "$res" ]
	then
		echo "File lrr/scripts/$script not found in $LRRFILE"
		exit 1
	fi
done

#
# Make the image.
#
sigf=/tmp/sig$$
signedpkgf=/tmp/signedpkg.tar.gz$$

echo ""
echo ""
echo "Signing"
/usr/bin/openssl dgst -sha1 -sign /opt/sdk_1.1/opkg/keys/private.key -out $sigf $LRRFILE
if [ $? -ne 0 ]; then
	echo "Error in signing"
	exit 1
fi

cat $LRRFILE $sigf > $signedpkgf

mv $signedpkgf $LRROUTFILE
echo "$LRROUTFILE created"

# cleanup everything.
rm -f $signedpkgf $sigf
