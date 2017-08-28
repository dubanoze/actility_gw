#!/bin/sh
# vim: ft=sh: set noet ts=4 sw=4:

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

if [ "$SYSTEM" = "wirmams" -o "$SYSTEM" = "wirmaar" -o "$SYSTEM" = "wirmana" ]
then
	FILESRC="lrr_${VERSION}_${SYSTEM}.ipk"
	FILEDST="/tmp/lrr_${VERSION}_${SYSTEM}.ipk"
	FILESRCMD5="lrr_${VERSION}_${SYSTEM}.md5"
	FILEDSTMD5="/tmp/lrr_${VERSION}_${SYSTEM}.md5"
else
	FILESRC="lrr-${VERSION}-${SYSTEM}.tar.gz"
	FILEDST="/tmp/lrr-${VERSION}-${SYSTEM}.tar.gz"
	FILESRCMD5="lrr-${VERSION}-${SYSTEM}.md5"
	FILEDSTMD5="/tmp/lrr-${VERSION}-${SYSTEM}.md5"
fi

if	[ "$DOWNLOAD" = "1" -a -f "$FILEDST" ]
then
	echo	"rm $FILEDST"
	rm $FILEDST
fi
if	[ "$DOWNLOAD" = "1" -a -f "$FILEDSTMD5" ]
then
	echo	"rm $FILEDSTMD5"
	rm $FILEDSTMD5
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


type md5sum > /dev/null 2>&1
if [ $? != 0 ]
then
	MD5VERIF=no
	echo	"md5sum command not found => no md5 check"
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
		if	[ -s "${FILEDSTMD5}" ]
		then
			MD5VERIF=`cat $FILEDSTMD5 | tr -d '\015'`
			echo	"md5 $MD5VERIF from $FILEDSTMD5"
		else
			echo	"md5 failure ${FILEDSTMD5} not downloaded"
			exit 1
		fi
	fi
	MD5SUM=`md5sum $FILEDST`
	set $MD5SUM
	echo $1
	if	[ "${1}" != "$MD5VERIF" ]
	then
		echo	"wrong md5sum check"
		echo	"$MD5SUM"
		echo	"$MD5VERIF"
		rm $FILEDST
		exit 1
	fi
	echo	"file $FILEDST downloaded and verified (md5sum)"
else
	echo	"file $FILEDST downloaded and not verified (no md5sum)"
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

if [ "$SYSTEM" = "wirmams" -o "$SYSTEM" = "wirmaar" -o "$SYSTEM" = "wirmana" ]
then
	opkg install --force-downgrade --force-reinstall $FILEDST
else
	tar zxf $FILEDST
fi

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

$ROOTACT/lrr/com/cmd_shells/restart.sh
exit $?
