
#-----------------------------------------------------------------------------
#
#	mkIR910.sh
#
#-----------------------------------------------------------------------------

PRODUCT=""
VERSION=""
PROC=DoTar
TMPDIR=/tmp/mkIR910_$$_`date +%y%m%d_%H%M%S`

Usage()
{
	exec >&2

	echo "Usage: mkIR910.sh -p product"
	exit 1
}

Error()
{
	echo "mkIR910.sh: $1" >&2
}

ErrorExit()
{
	echo "mkIR910.sh: $1" >&2
	exit 1
}

DoTar()
{
	(
	tar cf /tmp/_junk.tar --ignore-failed-read -T ${LISTFILES}
	mkdir -p $TMPDIR/CONTROL
	[ -d ${PRODUCT}/CONTROL ] && cp ${PRODUCT}/CONTROL/* $TMPDIR/CONTROL
	cd $TMPDIR
	mkdir actility
	cd actility
	tar xf /tmp/_junk.tar
	rm /tmp/_junk.tar
	cd ..

	(
		echo "Package: ${PRODUCT}"
		echo "Version: ${VERSION}"
		echo "Description: ${PRODUCT}"
		echo "Section: misc"
		echo "Priority: optional"
		echo "Maintainer: Actility Support <support@actility.com>"
		echo "Architecture: ir910"
		echo "Depends:"
	) > CONTROL/control


	/opt/sdk_1.1/opkg/opkg-build $TMPDIR $ROOTACT/deliveries
	cp /opt/sdk_1.1/opkg/keys/public.key $ROOTACT/deliveries/${PRODUCT}-opk.pubkey
	echo "Public key is available in $ROOTACT/deliveries/${PRODUCT}-opk.pubkey"
	)
	rm -r $TMPDIR
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


$PROC


