#!/bin/sh

#this works with vsftpd server on ubuntu
#sudo apt-get install vsftpd
#sudo mkdir /srv/ftp
#sudo usermod -d /srv/ftp ftp 
#sudo /etc/init.d/vsftpd restart
#source file name are relative to /srv/ftp directory

ARCHIVE=/mnt/mmcblk0p1

#default values
VERSION=""
MD5VERIF=""
FORCED="0"
OPTSYSTEM=""
DOWNLOAD="1"

[ -z "$FTPUSERDL" ] && FTPUSERDL=ftp
[ -z "$FTPPASSDL" ] && FTPPASSDL=
[ -z "$FTPHOSTDL" ] && FTPHOSTDL=${LRCPRIMARY}
[ -z "$FTPPORTDL" ] && FTPPORTDL=21
[ -z "$USE_SFTP" ] && USE_SFTP=0

#tests values
#FTPUSERDL=ftp
#FTPPASSDL=
#FTPHOSTDL=192.168.1.159
#FTPPORTDL=21
#MD5VERIF="5ce98edbeff1adf55773cfceb1bc80a1"

cpkg_unpack () {
	# $1: cpkg file
	# $2: rsa public key (X.509 / OpenSSL format)
	# $3: name of the .tar.gz file extracted from .cpkg
	signSize=256
	du -b $1 > /dev/null 2>&1
	if [ $? -eq 0 ]; then
		cpkgFileSize=$(du -b $1 | awk -F" " '{print $1}')
	else
		cpkgFileSize=$(ls -l $1 | awk -F" " '{print $5}')
	fi
	srcFileSize=$(($cpkgFileSize - $signSize))

	# split in two files: filelrr=.tar.gz filesign=signature
	filelrr="/tmp/_filelrr$$.tar.gz"
	filesign="/tmp/_filesign$$"
	dd if=${1} skip=$(expr $srcFileSize) of=$filesign bs=1 > /dev/null 2>&1
	dd if=${1} of=$filelrr bs=1 count=$(expr $srcFileSize) > /dev/null 2>&1

	# check signature
	openssl dgst -sha1 -verify $2 -signature $filesign $filelrr 2>/dev/null
	ret=$?
	if [ $ret -ne 0 ]; then
		echo "Incorrect signature."
		rm -f $filelrr $filesign
		exit 1
	fi
	mv $filelrr $3
	rm -f $filesign
}

LRRDEV=$(uname -n)
if	[ "$LRRDEV" = "Wirgrid_08020016" -o "$LRRDEV" = "pca-ESPRIMO-P5730" ]
then
	FTPPORTDL=21
	FTPHOSTDL=192.168.1.159
fi
if	[ -z "$FTPHOSTDL" ]
then
	FTPHOSTDL=lrc1.thingpark.com
fi

case	$# in
	*)
		while	[ $# -gt 0 ]
		do
			case	$1 in
				-F)
					shift
					FORCED="${1}"
					shift
				;;
				-V)
					shift
					VERSION="${1}"
					shift
				;;
				-M)
					shift
					MD5VERIF="${1}"
					shift
				;;
				-A)
					shift
					FTPHOSTDL="${1}"
					shift
				;;
				-P)
					shift
					FTPPORTDL="${1}"
					shift
				;;
				-U)
					shift
					FTPUSERDL="${1}"
					shift
				;;
				-W)
					shift
					FTPPASSDL="${1}"
					shift
				;;
				-S)
					shift
					OPTSYSTEM="${1}"
					shift
				;;
				-D)
					shift
					DOWNLOAD="${1}"
					shift
				;;
				-X)
					shift
					USE_SFTP="${1}"
					shift
				;;
				*)
					shift
				;;
			esac
		done
	;;
esac



export PATH=.:$PATH

if	[ -z "$ROOTACT" ]
then
	if	[ -d /mnt/fsuser-1/actility ]
	then
		export ROOTACT=/mnt/fsuser-1/actility
	else
		export ROOTACT=/home/actility
	fi
fi

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then
	. ${ROOTACT}/lrr/com/_functions.sh
fi

cd $ROOTACT
CURRENTVERSION=`cat lrr/Version`

echo	"current version $CURRENTVERSION"
echo	"request version $VERSION"

if	[ -z "$VERSION" ]
then
	echo	"requested version missing (-V option)"
	exit 1
fi

if	[ "$FORCED" = "0" ]
then
	if	[ "$CURRENTVERSION" = "$VERSION" ]
	then
		echo	"Version $VERSION already installed"
		exit 0
	fi
fi

if	[ "$FORCED" != "2" ]
then
	if	[ -f $ROOTACT/usr/etc/lrr/upgrade_locked ]
	then
		echo	"auto upgrade feature disabled by ~/usr/etc/lrr/upgrade_locked"
		exit 1
	fi
fi

# old installs do not have SYSTEM in _parameters.sh
if	[ -z "$SYSTEM" ]
then
	SYSTEM=$LRRSYSTEM
fi

if	[ "$OPTSYSTEM" != "" ]
then
	SYSTEM=$OPTSYSTEM
fi

FILESRC="lrr-${VERSION}-${SYSTEM}.cpkg"
FILEDST="/tmp/lrr-${VERSION}-${SYSTEM}.cpkg"
FILESRCMD5="lrr-opk.pubkey"
FILEDSTMD5="/tmp/lrr-opk.pubkey"

if	[ "$DOWNLOAD" = "1" -a -f "$FILEDST" ]
then
	echo	"rm $FILEDST"
	rm $FILEDST
fi

if	[ "$DOWNLOAD" = "1" ]
then
	echo "lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -r ${FILESRC} -l ${FILEDST} -s ${USE_SFTP}"
	lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -r ${FILESRC} -l ${FILEDST} -s ${USE_SFTP}

	if [ $? != "0" ]
	then                     
		echo    "lrr_DownloadFromRemote failure $FILEDST not downloaded"
		exit 1
	fi   
	if [ ! -s "$FILEDST" ]
	then
		echo	"lrr_DownloadFromRemote failure $FILEDST not downloaded size = 0"
		exit 1
	fi
fi

if [ ! -f "$FILEDST" -o ! -s "$FILEDST" ]
then
	echo	"lrr_DownloadFromRemote failure $FILEDST not downloaded size = 0"
	exit 1
fi


type openssl > /dev/null 2>&1
if [ $? != 0 ]
then
	MD5VERIF=no
	echo	"openssl command not found => no signature checking"
fi
if	[ "$MD5VERIF" != "no" ]
then
	if	[ -z "$MD5VERIF" ]
	then
		if	[ "$DOWNLOAD" = "1" -a -z "$MD5VERIF" ]
		then
			echo "lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -r ${FILESRCMD5} -l ${FILEDSTMD5} -s ${USE_SFTP}"
			lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -r ${FILESRCMD5} -l ${FILEDSTMD5} -s ${USE_SFTP}
		fi
		if	[ ! -s "${FILEDSTMD5}" ]
		then
			echo	"signature checking failure ${FILEDSTMD5} not downloaded"
			exit 1
		fi
	fi
	filedstnew="/tmp/lrr-${VERSION}-${SYSTEM}.tar.gz"
	cpkg_unpack $FILEDST $FILEDSTMD5 $filedstnew
	echo	"file $FILEDST downloaded and signature verified"
	FILEDST="$filedstnew"
else
	echo	"file $FILEDST downloaded and signature NOT verified"
fi

cd $ROOTACT

if	[ -d lrr ]
then
	if	[ -d lrr.previous ]
	then
		rm -fr lrr.previous
	fi
	mv lrr lrr.previous
fi

tar zxf $FILEDST

if	[ $? != "0" -o ! -x ./lrr/com/lrr.x ]
then
	echo	"tar extract error"
	if	[ -d lrr.previous ]
	then
		rm -fr lrr
		mv lrr.previous lrr
	fi
	exit	1
fi

if	[ -x ./lrr/com/_afterupgrade.sh ]
then
	export CURRENTVERSION
	export VERSION
	./lrr/com/_afterupgrade.sh
# the afterupgrade may have change some params, read them
	. $ROOTACT/lrr/com/_parameters.sh
fi
sleep 1


TEST=$(./lrr/com/lrr.x --version)

echo "TESTS results :"
echo "$TEST"

NEWVERSION=$(echo "$TEST" | grep "$VERSION")
if	[ -z "$NEWVERSION" ]
then
	echo	"Version "$VERSION" not retrieved in run test"
	if	[ -d lrr.previous ]
	then
		rm -fr lrr
		mv lrr.previous lrr
	fi
	exit 1
fi

if [ "$DOWNLOAD" = "1" -a -d $ARCHIVE ]
then
	mkdir -p ${ARCHIVE}/actility/packages
	mv $FILEDST ${ARCHIVE}/actility/packages
fi

$ROOTACT/lrr/com/sysconfiglrr.sh #This is added because of new hostip service only for Cisco Corsica
$ROOTACT/lrr/com/cmd_shells/reboot.sh #Needed for clean start, only restarts LXC

exit $?
