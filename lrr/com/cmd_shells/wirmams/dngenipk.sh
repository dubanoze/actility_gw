#!/bin/sh

# Use: dngenipk -T <ipkname> [-M <md5>] [-A <ftpserver>] [-P <port>] [-U <user>] [-W <password>] [-F 1]
#   -T <ipkname>: the file to install must be named <tarname>.ipk, and the command is "-T <ipkname>"
#                 without the ".ipk"
#   -M <md5>: specify the md5 string used to control the ipk file. This option is required
#             if the file <ipkname>.md5 is not available on the ftp server
#   -A <ftpserver>: specify the ftp server name or address (default: "lrc1.thingpark.com")
#   -P <port>: specify the ftp port to use for the download (default: 21)
#   -U <user>: specify the username to use for ftp download (default: "ftp")
#   -W <password>: specify the password to use for ftp download (default: "")
#   -F 1: force download even if $ROOTACT/usr/etc/lrr/dngenipk_locked file exists
# Install a .ipk file on a lrr

ARCHIVE=/mnt/mmcblk3p3
SAVECMD="$0 $*"

#default values
USER="ftp"
PASSWORD=""
PORT=21
LRC="${LRCPRIMARY}"
MD5VERIF=""
FORCED="0"
IPKFILE=""
DNGENIPK=""
IPKDIR="/user/.updates"

LRRDEV=$(uname -n)

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
					LRC="${1}"
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
					PASSWORD="${1}"
					shift
				;;
				-T)
					shift
					IPKFILE="${1}"
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
	if	[ -f $ROOTACT/usr/etc/lrr/dngenipk_locked ]
	then
		echo	"ipk package installation feature disabled by ~/usr/etc/lrr/dngenipk_locked"
		exit 1
	fi
fi

if	[ -z "$IPKFILE" ]
then
	echo	"the name of the file to install is required"
	exit 1
else
	FILESRC="${IPKFILE}.ipk"
	FILEDST="/tmp/${IPKFILE}.ipk"
	FILESRCMD5="${IPKFILE}.md5"
	FILEDSTMD5="/tmp/${IPKFILE}.md5"
	DNGENIPK=${IPKFILE}
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

URLSRC=ftp://${USER}@${LRC}:${PORT}/${FILESRC}
echo "lrr_GetFtp -O ${FILEDST} ${URLSRC}"
lrr_GetFtp -O ${FILEDST} ${URLSRC}

if [ $? != "0" ]
then                     
	echo    "lrr_GetFtp failure $FILEDST not downloaded"
	exit 1
fi   
if [ ! -s "$FILEDST" ]
then
	echo	"lrr_GetFtp failure $FILEDST not downloaded size = 0"
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
		URLSRC=ftp://${USER}@${LRC}:${PORT}/${FILESRCMD5}
		echo "lrr_GetFtp -O ${FILEDSTMD5} ${URLSRC}"
		lrr_GetFtp -O ${FILEDSTMD5} ${URLSRC}
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

[ ! -d "$IPKDIR" ] && mkdir $IPKDIR

cp $FILEDST $IPKDIR

if [ -d $ARCHIVE ]
then
        mkdir -p ${ARCHIVE}/actility/packages
        mv $FILEDST ${ARCHIVE}/actility/packages
else
	rm $FILEDST
fi
sync

echo	${DNGENIPK} > $ROOTACT/usr/etc/lrr/dngenipk_last
echo	"$(date '+%Y/%m/%d %H:%M:%S') : ${DNGENIPK} cmd=[$SAVECMD]" >> $ROOTACT/usr/etc/lrr/dngenipk_history

kerosd -u

echo "Reboot required to terminate installation, reboot in 15 s ..."
LRRDIR=$ROOTACT/lrr
sh $LRRDIR/com/cmd_shells/reboot_pending.sh 15 &

exit $?
