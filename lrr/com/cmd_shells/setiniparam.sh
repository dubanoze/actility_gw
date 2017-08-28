#!/bin/sh

# set a parameter in a .ini file
# Use: setiniparam <inifile> <section> <attribute> <value>
#	<inifile>: ini file to modify/create. If '.ini' extension is not specify it will be added.
#	<section>: section name
#	<attribute>: attribute name
#	<value>: value. It can be several 'words', or empty
#	ex: setiniparam lgw gen power 8
#

MOREINI="$ROOTACT/lrr/moreini/moreini.x"
TMPFILE=/tmp/setiniparam_$$

if [ $# -lt 3 ]
then
	echo "Not enough parameters"
	echo "Use: setiniparam <inifile> <section> <attribute> <value>"
	echo "<inifile>: ini file to modify/create. If '.ini' extension is not specify it will be added."
	echo "<section>: section name"
	echo "<attribute>: attribute name"
	echo "<value>: value. It can be several 'words', or empty"
	echo "ex: setiniparam lgw gen power 8"
	exit 1
fi

len=$(expr "$1" : '.*.\.ini$')
if [ $len -gt 0 ]
then
	INIFILE="$1"
else
	INIFILE="$1.ini"
fi

{
echo "[$2]"
echo -n "	$3="
shift 3
if [ $# -gt 1 ]
then
	echo "\"$*\""
else
	echo "$*"
fi
} > $TMPFILE

$MOREINI -t $ROOTACT/usr/etc/lrr/$INIFILE -a $TMPFILE -y
ret=$?
rm -f $TMPFILE
exit $ret
