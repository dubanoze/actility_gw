#!/bin/sh
# vim: ft=sh: set noet ts=4 sw=4:

# Use: dngentar -T <tarname> [-D <targetdir>] [-M <md5>] [-A <ftpserver>] [-P <port>] [-U <user>] [-W <password>] [-F 1]
#   -T <tarname>: the file to install must be named <tarname>.tar.gz, and the command is "-T <tarname>"
#                 without the ".tar.gz"
#   -D <targetdir>: the extraction is done in <targetdir> instead of "/tmp"
#   -M <md5>: specify the md5 string used to control the tar file. This option is required
#             if the file <tarname>.md5 is not available on the ftp server
#   -A <ftpserver>: specify the ftp server name or address (default: "lrc1.thingpark.com")
#   -P <port>: specify the ftp port to use for the download (default: 21)
#   -U <user>: specify the username to use for ftp download (default: "ftp")
#   -W <password>: specify the password to use for ftp download (default: "")
#   -F 1: force download even if $ROOTACT/usr/etc/lrr/dngentar_locked file exists
# Install a tar.gz file on a lrr
# tar is extracted in /tmp on the lrr
# the tar file can contain "preinstall.sh" and "postinstall.sh" scripts executed
# respectively before and after the extraction. They must be located at the base
# of the tar tree, and these 2 files are always extrated in /tmp, even if option -R is used
# the execution right must be set on these 2 files

ARCHIVE=/mnt/mmcblk0p1
SAVECMD="$0 $*"

#default values
MD5VERIF=""
FORCED="0"
TARFILE=""
DNGENTAR=""
TARGETDIR="/tmp"
PREINSTALL="preinstall.sh"
POSTINSTALL="postinstall.sh"
[ -z "$FTPUSERDL" ] && FTPUSERDL=ftp
[ -z "$FTPPASSDL" ] && FTPPASSDL=
[ -z "$FTPHOSTDL" ] && FTPHOSTDL=${LRCPRIMARY}
[ -z "$FTPPORTDL" ] && FTPPORTDL=21
[ -z "$USE_SFTP" ] && USE_SFTP=0

LRRDEV=$(uname -n)
if	[ "$LRRDEV" = "Wirgrid_08020016" -o "$LRRDEV" = "pca-ESPRIMO-P5730" ]
then
	FTPPORTDL=21
	FTPHOSTDL=192.168.1.159
fi

case	$# in
	*)
		while	[ $# -gt 0 ]
		do
			case	$1 in
				-D)
					shift
					TARGETDIR="${1}"
					shift
				;;
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
				-T)
					shift
					TARFILE="${1}"
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

# Launch shell script through sh instead of selfexecutable to bypass noexec option on /tmp 
SHELLLAUNCH=
if      [ "$SYSTEM" = "fclamp" ]
then
        SHELLLAUNCH=sh
fi

cd $ROOTACT

if	[ "$FORCED" = "0" ]
then
	if	[ -f $ROOTACT/usr/etc/lrr/dngentar_locked ]
	then
		echo	"generic package installation feature disabled by ~/usr/etc/lrr/dngentar_locked"
		exit 1
	fi
fi

if	[ -z "$TARFILE" ]
then
	echo	"the name of the file to install is required"
	exit 1
else
	FILESRC="${TARFILE}.tar.gz"
	FILEDST="/tmp/${TARFILE}.tar.gz"
	FILESRCMD5="${TARFILE}.md5"
	FILEDSTMD5="/tmp/${TARFILE}.md5"
	DNGENTAR=${TARFILE}
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

if [ ! -d "$TARGETDIR" ]
then
	echo	"directory [$TARGETDIR] doesn't exist, abort"
	rm $FILEDST
	exit 1
fi


cd /tmp
[ -f "$PREINSTALL" ] && rm "$PREINSTALL"
tar zxvf $FILEDST "./${PREINSTALL}" "${PREINSTALL}" 2>/dev/null
if	[ -f "$PREINSTALL" ]
then
	echo "pre-installation script found"
	$SHELLLAUNCH ./$PREINSTALL
	rm "$PREINSTALL"
fi

cd "$TARGETDIR"
echo "extraction of $FILESRC in [$TARGETDIR]"
if [ "$SYSTEM" = "fcmlb" -o "$SYSTEM" = "fcpico" -o "$SYSTEM" = "fclamp" -o "$SYSTEM" = "wirmaar" -o "$SYSTEM" = "wirmana" -o "$SYSTEM" = "tektelic" ]
then
	fileexclude="/tmp/_tarexclude$$"
	echo "$PREINSTALL" > $fileexclude
	echo "./$PREINSTALL" >> $fileexclude
	echo "$POSTINSTALL" >> $fileexclude
	echo "./$POSTINSTALL" >> $fileexclude
	tar zxv -X $fileexclude -f $FILEDST 
	rm -f $fileexclude
else
	tar zxv --exclude "$PREINSTALL" --exclude "./$PREINSTALL" --exclude "$POSTINSTALL" --exclude "./$POSTINSTALL" -f $FILEDST 
fi

sleep 1

cd /tmp	
[ -f "$POSTINSTALL" ] && rm "$POSTINSTALL"
tar zxvf $FILEDST "./${POSTINSTALL}" "${POSTINSTALL}" 2>/dev/null
if	[ -f "$POSTINSTALL" ]
then
	echo "post-installation script found"
	$SHELLLAUNCH ./$POSTINSTALL
	rm "$POSTINSTALL"
fi

if [ -d $ARCHIVE ]
then
        mkdir -p ${ARCHIVE}/actility/packages
        mv $FILEDST ${ARCHIVE}/actility/packages
else
	rm $FILEDST
fi

echo	${DNGENTAR} > $ROOTACT/usr/etc/lrr/dngentar_last
echo	"$(date '+%Y/%m/%d %H:%M:%S') : ${DNGENTAR} cmd=[$SAVECMD]" >> $ROOTACT/usr/etc/lrr/dngentar_history

exit $?
