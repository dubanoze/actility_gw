#!/bin/sh
#			treatbootsrv.sh
# Get configuration from bootserver and update lrr config
# 1) get uid
# 2) send uid to bootserver and get configuration file
# 3) parse file and update lrr configuration files
# vim: ft=sh: set noet ts=4 sw=4:

SAVECMD="$0 $*"
GETUID="$ROOTACT/lrr/com/cmd_shells/getuid.sh --readfile"
SETINIPARAM="$ROOTACT/lrr/com/cmd_shells/setiniparam.sh"
USE_SFTP="1"

case	$# in
	*)
		while	[ $# -gt 0 ]
		do
			case	$1 in
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

# get UUID
# need to be renamed because UID already exist in current env and eval
# fails if trying to erase existing environment variable
res=$($GETUID | sed "s/UID/FULLUID/")

if [ -z "$res" ]
then
	echo "Failed while running '$GETUID'"
	exit 1
fi

echo "res=$res"
eval $res

echo "SYSTEM=$SYSTEM"
if	[ -z "$FULLUID" ]
then
	echo "Can not get UID !"
	exit 1
else
	FILESRC="${FULLUID}"
	FILEDST="/tmp/${FULLUID}"
fi

if	[ -f "$FILEDST" ]
then
	echo	"rm $FILEDST"
	rm $FILEDST
fi

# get file from bootserver
LRRSYSTEM=$SYSTEM
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

# modify _state.ini and lrr.ini
. $FILEDST
$SETINIPARAM dynlap lrr uidfrombootsrv 0x$LRR_ID
$SETINIPARAM dynlap lrr masterid $MASTER_ID	# modify lrr.ini instead of _state.ini
$SETINIPARAM dynlap laplrc:0 addr $RAN_IP1_ADDR	# modify lrr.ini instead of _state.ini
$SETINIPARAM dynlap laplrc:0 port $RAN_IP1_PORT	# modify lrr.ini instead of _state.ini
$SETINIPARAM dynlap laplrc:1 addr $RAN_IP2_ADDR	# modify lrr.ini instead of _state.ini
$SETINIPARAM dynlap laplrc:1 port $RAN_IP2_PORT	# modify lrr.ini instead of _state.ini
$SETINIPARAM dynlap lrr nblrc 2	# modify lrr.ini instead of _state.ini

echo	"$(date '+%Y/%m/%d %H:%M:%S') : ${FULLUID}" > $ROOTACT/usr/etc/lrr/bootserver_last

exit $?
