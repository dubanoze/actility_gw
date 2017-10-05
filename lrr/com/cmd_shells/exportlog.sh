#!/bin/sh

#default values

HOST=""
PORT=""
USER=""
PASS=""
SFTP="1"
LOGDIR=""

while	[ $# -gt 0 ]
do
	case	$1 in
		-A)
			shift
			HOST="${1}"
			shift
		;;
		-P)
			shift
			PORT="${1}"
			shift
		;;
		-U)
			shift
			USER="${1}"
			shift
		;;
		-W)
			shift
			PASS="${1}"
			shift
		;;
		-S)
			shift
			SFTP="${1}"
			shift
		;;
		-L)
			shift
			LOGDIR="${1}"
			shift
		;;
		-C)
			shift
			CRYPTED="-c"
		;;
		*)
			shift
		;;
	esac
done

[ -z "$HOST" ] && exit 1
[ -z "$PORT" ] && exit 1
[ -z "$USER" ] && exit 1
[ -z "$PASS" ] && exit 1

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then
	. ${ROOTACT}/lrr/com/_functions.sh
fi
[ -z "$LRRSYSTEM" ] && LRRSYSTEM="$SYSTEM"

LOGFILE="LOGS_$(date '+%y%m%d_%H%M%S').tar.gz"

# tar directories
tarfile="/tmp/sendlog.tar"
rm -f $tarfile $tarfile.gz
cmd="tar cvhf $tarfile $ROOTACT/var/log/lrr $ROOTACT/usr/etc/lrr $ROOTACT/lrr/config $LOGDIR"
[ "$SYSTEM" != "ciscoms" ] && cmd="$cmd /var/log/messages"
$cmd

# gzip
gzip $tarfile

lrr_UploadToRemote $CRYPTED -u $USER -w "$PASS" -a $HOST -p $PORT -l $tarfile.gz -r $LOGFILE -s $SFTP

exit $?
