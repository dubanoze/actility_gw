#!/bin/sh
# vim: ft=sh: set noet ts=4 sw=4:

# Use: moreini [-T] [-M <md5>] [-A <ftpserver>] [-P <port>] [-U <user>] [-W <password>] <tarname>
#   <tarname>: the tar file containing the .ini file(s) to merge
#   -T: test mode. The result is only displayed, no .ini file is modified/created
#   -M <md5>: specify the md5 string used to control the tar file. This option is required
#             if the file <tarname>.md5 is not available on the ftp server
#   -A <ftpserver>: specify the ftp server name or address (default: "lrc1.thingpark.com")
#   -P <port>: specify the ftp port to use for the download (default: 21)
#   -U <user>: specify the username to use for ftp download (default: "ftp")
#   -W <password>: specify the password to use for ftp download (default: "")
# Merge .ini configuration files in $ROOTACT/usr/etc/lrr/ directory
# The files can be lrr.ini, lgw.ini or channels.ini.
# At least one of these files must exist in the tar
# The .ini files must be in the root directory of the tar

ARCHIVE=/mnt/mmcblk0p1
SAVECMD="$0 $*"

#default values
MD5VERIF=""
FORCED=""
TARFILE=""
TARGETDIR="/tmp"
PREINSTALL="preinstall.sh"
POSTINSTALL="postinstall.sh"
MICMD="$ROOTACT/lrr/moreini/moreini.x"
NOW=$(date '+%Y%m%d-%H%M%S')
[ -z "$FTPUSERDL" ] && FTPUSERDL=ftp
[ -z "$FTPPASSDL" ] && FTPPASSDL=
[ -z "$FTPHOSTDL" ] && FTPHOSTDL=${LRCPRIMARY}
[ -z "$FTPPORTDL" ] && FTPPORTDL=21
[ -z "$USE_SFTP" ] && USE_SFTP=0

testmode()
{
	file="$1"

	[ ! -f "$file" ] && return

	$MICMD -t $ROOTACT/usr/etc/lrr/$file -a /tmp/$file -o /tmp/moreiniout$$
	cat /tmp/moreiniout$$
	rm -f /tmp/moreiniout$$
}

domerge()
{
	file="$1"

	[ ! -f "$file" ] && return

	if [ -d $ARCHIVE -a -f "$ROOTACT/usr/etc/lrr/$file" ]
	then
		mkdir -p ${ARCHIVE}/actility/packages
		cp "$ROOTACT/usr/etc/lrr/$file" "${ARCHIVE}/actility/packages/${file}_${NOW}"
	fi
	if [ -z "$FORCED" ]; then
		$MICMD -t $ROOTACT/usr/etc/lrr/$file -a /tmp/$file
	else
		echo "no confirm to overwrite target file"
		$MICMD -t $ROOTACT/usr/etc/lrr/$file -y -a /tmp/$file
	fi

	echo	${file} > $ROOTACT/usr/etc/lrr/moreini_last
	echo	"$(date '+%Y/%m/%d %H:%M:%S') : cmd=[$MICMD -t $ROOTACT/usr/etc/lrr/$file -a /tmp/$file]" >> $ROOTACT/usr/etc/lrr/moreini_history

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
				-Y)
					shift
					FORCED="1"
				;;
				-T)
					shift
					TESTMODE="1"
				;;
				-X)
					shift
					USE_SFTP="${1}"
					shift
				;;
				*)
					TARFILE="${1}"
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

if	[ -z "$TARFILE" ]
then
	echo	"the name of the file to install is required"
	exit 1
else
	FILESRC="${TARFILE}.tar.gz"
	FILEDST="/tmp/${TARFILE}.tar.gz"
	FILESRCMD5="${TARFILE}.md5"
	FILEDSTMD5="/tmp/${TARFILE}.md5"
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
	echo "lrr_DownloadFromRemote failure $FILEDST not downloaded"
	exit 1
fi
if [ ! -s "$FILEDST" ]
then
	echo "lrr_DownloadFromRemote failure $FILEDST not downloaded size = 0"
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



if [ ! -d "$TARGETDIR" ]
then
	echo	"directory [$TARGETDIR] doesn't exist, abort"
	rm $FILEDST
	exit 1
fi

echo	"file $FILEDST downloaded and verified (md5sum)"

cd "$TARGETDIR"
echo "extraction of $FILESRC in [$TARGETDIR]"
tar zxv --exclude "$PREINSTALL" --exclude "./$PREINSTALL" --exclude "$POSTINSTALL" --exclude "./$POSTINSTALL" -f $FILEDST 

if [ ! -f "lrr.ini" -a ! -f "lgw.ini" -a ! -f "channels.ini" ]
then
	echo "No ini file found ! At least one of lrr.ini, lgw.ini or channels.ini must exist !\n"
	exit 1
fi

if [ "$TESTMODE" = "1" ]
then
	testmode lrr.ini
	testmode lgw.ini
	testmode channels.ini
	exit 0
fi

domerge lrr.ini
domerge lgw.ini
domerge channels.ini

if [ -d $ARCHIVE ]
then
	mkdir -p ${ARCHIVE}/actility/packages
	mv $FILEDST "${ARCHIVE}/actility/packages/${TARFILE}_${NOW}"
else
	rm $FILEDST
fi

exit 0
