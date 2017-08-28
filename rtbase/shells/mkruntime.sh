#!/bin/sh
#-----------------------------------------------------------------------------
#
#	mktar.sh
#
#-----------------------------------------------------------------------------

PRODUCT=""
VERSION=""
PROC=DoTar
GLOBAL_LISTFILES=/tmp/dotar_listfiles.lst

Usage()
{
	exec >&2

	echo "Usage: mkruntime.sh -p product"
	exit 1
}

Error()
{
	echo "mkruntime.sh: $1" >&2
}

ErrorExit()
{
	echo "mkruntime.sh: $1" >&2
	exit 1
}

DoTar()
{
	PACKAGE_BASENAME=${PRODUCT}-${VERSION}-${SYSTEM}	# Ex: lrr-1.8.19-ciscoms
	DELIV_DIR=$ROOTACT/deliveries
	echo "Cmde: tar cf ${DELIV_DIR}/${PACKAGE_BASENAME}.tar -T ${GLOBAL_LISTFILES}"
	tar cf ${DELIV_DIR}/${PACKAGE_BASENAME}.tar -T ${GLOBAL_LISTFILES} 
	if [ $? -ne 0 ] ; then
		echo "Error: creating ${PACKAGE_BASENAME}.tar"
		return 1
	fi

	cd $DELIV_DIR
	if [ -f ${PACKAGE_BASENAME}.tar.gz ] ; then
		echo "Warning: Package ${PACKAGE_BASENAME}.tar.gz exists. It will be overwrite"
	fi
	gzip -f ${PACKAGE_BASENAME}.tar
	if [ $? -ne 0 ] ; then
		echo "Error: creating ${PACKAGE_BASENAME}.tar.gz"
		return 1
	fi
	MD5SUM=$(md5sum ${PACKAGE_BASENAME}.tar.gz)
	set $MD5SUM
	echo $1 > ${PACKAGE_BASENAME}.md5
	echo "SUCCESS: Package ${PACKAGE_BASENAME}.tar.gz created with MD5 checksum"
}

CheckFileList ()
{
	if [ -z $1 ]; then
		echo "CheckFileList(): No file list provided"
		return 1
	fi
	for i in $(cat $1); do
	#	echo "Check file $i"
		if [ -f $ROOTACT/$i ]; then
			echo $i >> $GLOBAL_LISTFILES
		else
			echo "Warning: file $i not found. Ignore it"
		fi
	done
}

while [ $# -gt 0 ]
do
	case $1 in
		-p)	shift
			if [ $# = 0 ]
			then
				Usage
			fi
			PRODUCT=$1
			shift
		;;
	esac
done

if [ -z "$ROOTACT" ]
then
	Error "ROOTACT not specified"
	Usage
fi
if [ -z "$PRODUCT" ]
then
	Error "product name not specified"
	Usage
fi

. $ROOTACT/rtbase/base/system

mkdir -p $ROOTACT/deliveries > /dev/null 2>&1



cd $ROOTACT/$PRODUCT || ErrorExit "ROOTACT/$PRODUCT does not exist"

if [ ! -f "./Version" ]
then
	ErrorExit "./Version file not found"
fi
VERSION=`cat ./Version`
cd ..
#echo "rtbase/MAKE" > /tmp/XX.lst
#LISTFILES=/tmp/XX.lst
LISTFILES=$ROOTACT/$PRODUCT/package/runtime.lst
if [ ! -f $LISTFILES ]
then
	if [ -f $ROOTACT/rtbase/package/$PRODUCT-runtime.lst ]
	then
	LISTFILES=$ROOTACT/rtbase/package/$PRODUCT-runtime.lst
	Error "use special runtime list from rtbase/package for $PRODUCT"
	else
	LISTFILES=/tmp/$PRODUCT-${VERSION}-${SYSTEM}.lst
	Error "./package/runtime.lst file not found (default list created !!!)"
	find $PRODUCT -type f ! -name "*.[clyo]" -a ! -name "[Mm]akefile" | grep -v '.git' > $LISTFILES
	fi
fi

cat /dev/null > $GLOBAL_LISTFILES
CheckFileList $LISTFILES

LISTFILESSUP=$ROOTACT/$PRODUCT/package/$SYSTEM/runtime.lst
if [ -f "$LISTFILESSUP" ]
then
	CheckFileList $LISTFILESSUP
fi


$PROC


