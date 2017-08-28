#!/bin/sh
# vim: ft=sh: set noet ts=4 sw=4:

#this works with vsftpd server on ubuntu
#sudo apt-get install vsftpd
#sudo mkdir /srv/ftp
#sudo usermod -d /srv/ftp ftp 
#sudo /etc/init.d/vsftpd restart
#source file name are relative to /srv/ftp directory

#default values
MD5VERIF=""
FORCED="0"
CONFIG=""
DNCONFIG=""
EXTRACT=""
RESTART=""
CFGTYPE=""
CFGHARD=""
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
				-C)
					shift
					CONFIG="${1}"
					shift
				;;
				-H)
					shift
					CFGHARD="$1"
					shift
				;;
				-R)
					shift
					EXTRACT="1"
				;;
				-S)
					shift
					RESTART="${1}"
					shift
				;;
				-T)
					shift
					CFGTYPE="${1}"
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

if	[ "$FORCED" = "0" ]
then
	if	[ -f $ROOTACT/usr/etc/lrr/dnconfig_locked ]
	then
		echo	"auto config feature disabled by ~/usr/etc/lrr/dnconfig_locked"
		exit 1
	fi
fi

if	[ -z "$CONFIG" ]
then
	FILESRC="cfglrr-${LRRID}.tar.gz"
	FILEDST="/tmp/cfglrr-${LRRID}.tar.gz"
	FILESRCMD5="cfglrr-${LRRID}.md5"
	FILEDSTMD5="/tmp/cfglrr-${LRRID}.md5"
	DNCONFIG=${LRRID}
else
	if [ ! -z "$CFGHARD" ]
	then
		CFGVER="_v2"
		[ "$SYSTEM" != "wirmaar" -a "$SYSTEM" != "ciscoms" -a "$SYSTEM" != "tektelic" ] && CFGVER="_v1"
		CFGHARD="-CONF_${CFGHARD}${CFGVER}"
	fi
	FILESRC="cfglrr-${CONFIG}${CFGHARD}.tar.gz"
	FILEDST="/tmp/cfglrr-${CONFIG}.tar.gz"
	FILESRCMD5="cfglrr-${CONFIG}${CFGHARD}.md5"
	FILEDSTMD5="/tmp/cfglrr-${CONFIG}.md5"
	DNCONFIG=${CONFIG}
fi

if	[ -f "$FILEDST" ]
then
	echo	"rm $FILEDST"
	rm $FILEDST
fi
if	[ -f "$FILEDSTMD5" ]
then
	echo	"rm $FILEDSTMD5"
	rm $FILEDSTMD5
fi

echo "lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -l ${FILEDST} -r ${FILESRC} -s ${USE_SFTP}"
lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -l ${FILEDST} -r ${FILESRC} -s ${USE_SFTP}

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
		echo "lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -l ${FILEDSTMD5} -r ${FILESRCMD5} -s ${USE_SFTP}"
		lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -l ${FILEDSTMD5} -r ${FILESRCMD5} -s ${USE_SFTP}
		if	[ -s "${FILEDSTMD5}" ]
		then
			MD5VERIF=`cat $FILEDSTMD5 | tr -d '\015'`
			echo	"md5 $MD5VERIF from $FILEDSTMD5"
		else
			echo	"md5 file not downloaded"
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

if	[ -z "${EXTRACT}" ]
then
	cd $ROOTACT/usr/etc/lrr
else
	cd $ROOTACT
fi

case	${CFGTYPE} in
	rfregion)
		echo "check presence of lgw.ini and channels.ini"
		tar ztvf $FILEDST lgw.ini channels.ini
		if [ $? != "0" ]
		then
			echo "tar does not contains lgw.ini and channels.ini"
			exit 1
		fi
		tar zxvf $FILEDST
		sleep 1
		echo	${DNCONFIG} > $ROOTACT/usr/etc/lrr/dnrfregion_last
		RESTART="radiorestart"
	;;
	*)
		tar zxvf $FILEDST
		sleep 1
		echo	${DNCONFIG} > $ROOTACT/usr/etc/lrr/dnconfig_last
	;;
esac



if	[ "$RESTART" = "lrrrestart" ]
then
	$ROOTACT/lrr/com/cmd_shells/restart.sh
fi
if	[ "$RESTART" = "radiorestart" ]
then
	touch	"/tmp/lrrradiorestart"
fi

exit $?
