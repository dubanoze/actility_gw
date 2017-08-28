#!/bin/sh

#this works with vsftpd server on ubuntu
#sudo apt-get install vsftpd
#sudo mkdir /srv/ftp
#sudo usermod -d /srv/ftp ftp 
#sudo /etc/init.d/vsftpd restart
#source file name are relative to /srv/ftp directory

#default values

[ -z "$FTPUSERSUPPORT" ] && FTPUSERSUPPORT=ftp
[ -z "$FTPPASSSUPPORT" ] && FTPPASSSUPPORT=
[ -z "$FTPHOSTSUPPORT" ] && FTPHOSTSUPPORT=support1.actility.com
[ -z "$FTPPORTSUPPORT" ] && FTPPORTSUPPORT=21
[ -z "$USE_SFTP_SUPPORT" ] && USE_SFTP_SUPPORT=0


DEFCONFIG="0"

while	[ $# -gt 0 ]
do
	case	$1 in
		-A)
			shift
			FTPHOSTSUPPORT="${1}"
			shift
		;;
		-P)
			shift
			FTPPORTSUPPORT="${1}"
			shift
		;;
		-U)
			shift
			FTPUSERSUPPORT="${1}"
			shift
		;;
		-W)
			shift
			FTPPASSSUPPORT="${1}"
			shift
		;;
		-D)
			shift
			DEFCONFIG="1"
		;;
		-X)
			shift
			USE_SFTP_SUPPORT="${1}"
			shift
		;;
		*)
			shift
		;;
	esac
done



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


DATE=$(date '+%Y%m%d%H%M%S')
FILESRC="/tmp/cfglrr-${LRRID}.tar"
FILEDST="UPLOAD_LRR/CONFIG/cfglrr-${LRRID}-${DATE}.tar"

if	[ -f "$FILESRC" ]
then
	echo	"rm $FILESRC"
	rm $FILESRC
fi

cd $ROOTACT

./lrr/com/lrr.x --config > usr/etc/lrr/_config_review.log 2>&1
if	[ "$DEFCONFIG" = "0" ]
then
	tar cvf $FILESRC usr/etc/lrr
else
	tar cvf $FILESRC usr/etc/lrr lrr/config
fi
type gzip > /dev/null 2>&1
if [ $? = 0 ]
then
	rm -f ${FILESRC}.gz
	gzip -f $FILESRC
	FILESRC=${FILESRC}.gz
	FILEDST=${FILEDST}.gz
fi

echo "lrr_UploadToRemote -u ${FTPUSERSUPPORT} -w ${FTPPASSSUPPORT} -a ${FTPHOSTSUPPORT} -p ${FTPPORTSUPPORT} -l ${FILESRC} -r ${FILEDST} -s ${USE_SFTP_SUPPORT}"
lrr_UploadToRemote -u ${FTPUSERSUPPORT} -w ${FTPPASSSUPPORT} -a ${FTPHOSTSUPPORT} -p ${FTPPORTSUPPORT} -l ${FILESRC} -r ${FILEDST} -s ${USE_SFTP_SUPPORT}

if [ $? != "0" ]
then                     
	echo    "lrr_UploadToRemote failure $FILEDST not uploaded"
	exit 1
fi

exit 0
