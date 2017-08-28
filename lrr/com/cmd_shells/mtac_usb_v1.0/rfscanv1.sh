#!/bin/sh
#
#	Execute a radio scan
#
######################################################################################################################
#                                                                                                                    #
# ! ! ! WARNING : this rfscanv1 perform an util_rssi_ + reboot instead of standard util_spectral_scan ! ! !          #
#                                                                                                                    #
######################################################################################################################
SCAN=${ROOTACT}/lrr/util_rssi_histogram/util_rssi_histogram

[ -z "$FTPUSERSUPPORT" ] && FTPUSERSUPPORT=ftp
[ -z "$FTPPASSSUPPORT" ] && FTPPASSSUPPORT=
[ -z "$FTPHOSTSUPPORT" ] && FTPHOSTSUPPORT=support1.actility.com
[ -z "$FTPPORTSUPPORT" ] && FTPPORTSUPPORT=21
[ -z "$USE_SFTP_SUPPORT" ] && USE_SFTP_SUPPORT=0

TOKEN=""
FORCE="0"
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
		-F)
			shift
			FORCE="1"
		;;
		-T)
			shift
			TOKEN="${1}"
			shift
		;;
		-X)
			shift
			USE_SFTP_SUPPORT="${1}"
			shift
		;;
		*)
			break
		;;
	esac
done


# set $ROOTACT
if	[ -z "$ROOTACT" ]
then
	if	[ -d /mnt/fsuser-1/actility ]
	then
		export ROOTACT=/mnt/fsuser-1/actility
	else
		export ROOTACT=/home/actility
	fi
fi

# check if $ROOTACT directory exists
if	[ ! -d "$ROOTACT" ]
then
	echo	"$ROOTACT does not exist"
	exit	1
fi

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then
	. ${ROOTACT}/lrr/com/_functions.sh
fi

#give time to lrr process to stop radio thread
sleep 3


if [ ! -x ${SCAN} ]
then
	exit 1
fi

cd /tmp
if	[ "$FORCE" = "1" ]
then
	rm -f /tmp/rfscan.pid
fi
if	[ -f /tmp/rfscan.pid ]
then
	echo "rfscan already running"
	exit 1
fi
echo $$ > /tmp/rfscan.pid
rm -f rssi_histogram.csv rfscanv0.log
[ ! -z "$SPIDEVICE" ] && OPTDEVICE="-d $SPIDEVICE"
echo "start util_rssi_scan from rfscanv1 without parameters(not compatible)"
echo "$*" > rfscanv0.log
$SCAN $OPTDEVICE >> rfscanv0.log 2>&1

#sleep again if scan util exit immediatly
sleep 3


# the exit code of the scan util is always 0 ... we return a empty file
if [ ! -f rssi_histogram.csv ]
then
	touch rfscan_${LRRID}.csv
else
	mv rssi_histogram.csv rfscan_${LRRID}.csv
fi

DATE=$(date '+%Y%m%d%H%M%S')
FILESRC="/tmp/rfscan_${LRRID}.csv"
if	[ "$TOKEN" = "" ]
then
	FILEDST="UPLOAD_LRR/RFSCAN/rfscan_${LRRID}_${DATE}.csv"
else
	FILEDST="UPLOAD_LRR/RFSCAN/rfscan_${LRRID}_${TOKEN}_${DATE}.csv"
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
	rm /tmp/rfscan.pid
	echo    "lrr_UploadToRemote failure $FILEDST not uploaded"
	exit 1
fi   
rm /tmp/rfscan.pid
echo "rfscan finished: Result uploaded: Rebooting gateway!..."
reboot
exit 0
