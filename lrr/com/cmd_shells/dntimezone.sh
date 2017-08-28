#!/bin/sh

# Use: dntimezone -T <timezone> [-A <ftpserver>] [-P <port>] [-U <user>] [-W <password>] [-D <directory>] [-Z <timezone-directory>] [-L <localtime-location>] [-F 1] [-S 1]
#   -T <timezone>: the timezone file to add
#   -A <ftpserver>: specify the ftp server name or address (default: FTPHOSTSUPPORT)
#   -P <port>: specify the ftp port to use for the download (default: FTPPORTSUPPORT)
#   -U <user>: specify the username to use for ftp download (default: FTPUSERSUPPORT)
#   -W <password>: specify the password to use for ftp download (default: FTPPASSSUPPORT)
#   -D <directory>: specify the directory where the file is located on server (default: 'TIMEZONE_BASE_LRR/zoneinfo/')
#   -Z <timezone-directory>: specify the directory where the timezone are stored (default: '/usr/share/zoneinfo')
#   -L <localtime-location>: specify the location of the localtime simlink  (default: '/etc/localtime')
#   -F 1: force download even if $ROOTACT/usr/etc/lrr/dntimezone_locked file exists
#   -S 1: use sftp
# or dntimezone --get-list
# Change the timezone on a lrr


ARCHIVE=/mnt/mmcblk0p1
SAVECMD="$0 $*"

#default values
FORCED="0"
GETLIST="0"
TIMEZONEFILE=""
DNTIMEZONE=""
TMPDIR="/tmp"
TIMEZONEDIR="/usr/share/zoneinfo"
LOCALTIME="/etc/localtime"
[ -z "$FTPUSERSUPPORT" ]    && FTPUSERSUPPORT=
[ -z "$FTPPASSSUPPORT" ]    && FTPPASSSUPPORT=
[ -z "$FTPHOSTSUPPORT" ]    && FTPHOSTSUPPORT=
[ -z "$FTPPORTSUPPORT" ]    && FTPPORTSUPPORT=
[ -z "$FTPHOSTDIR" ]        && FTPHOSTDIR="TIMEZONE_BASE_LRR/zoneinfo"
[ -z "$USE_SFTP_SUPPORT" ]  && USE_SFTP_SUPPORT=0

case    $# in
    *)
        while   [ $# -gt 0 ]
        do
            case    $1 in
                -F)
                    shift
                    FORCED="${1}"
                    shift
                ;;
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
                -T)
                    shift
                    TIMEZONEFILE="${1}"
                    shift
                ;;
                -S)
                    shift
                    USE_SFTP_SUPPORT="${1}"
                    shift
                ;;
                -D)
                    shift
                    FTPHOSTDIR="${1}"
                    shift
                ;;
                -Z)
                    shift
                    TIMEZONEDIR="${1}"
                    shift
                ;;
                -L)
                    shift
                    LOCALTIME="${1}"
                    shift
                ;;
                -h | --help | -H | --h | --H)
                    shift
                    echo "Use: dntimezone -T <timezone> [-A <ftpserver>] [-P <port>] [-U <user>] [-W <password>] [-D <directory>] [-F 1] [-S 1]
    -T <timezone>: the timezone file to add
    -A <ftpserver>: specify the ftp server name or address (default: FTPHOSTSUPPORT)
    -P <port>: specify the ftp port to use for the download (default: FTPPORTSUPPORT)
    -U <user>: specify the username to use for ftp download (default: FTPUSERSUPPORT)
    -W <password>: specify the password to use for ftp download (default: FTPPASSSUPPORT)
    -D <directory>: specify the directory where the file is loctaed on server (default: 'TIMEZONE_BASE_LRR/zoneinfo/')
    -Z <timezone-directory>: specify the directory where the timezone are stored (default: '/usr/share/zoneinfo')
    -L <localtime-location>: specify the location of the localtime simlink  (default: '/etc/localtime')
    -F 1: force download even if $ROOTACT/usr/etc/lrr/dntimezone_locked file exists
    -S 1: use sftp"
                    exit 1
                ;;
                --get-list)
                    shift
                    GETLIST=1
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

if  [ -z "$ROOTACT" ]
then
    if  [ -d /mnt/fsuser-1/actility ]
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

if  [ "$GETLIST" = "1" ]
then
    FTPHOSTDIR="TIMEZONE_BASE_LRR"
    TIMEZONEFILE="TIMEZONE_LIST"

    FILESRC="$FTPHOSTDIR/$TIMEZONEFILE"
    FILETMP="$TMPDIR/$TIMEZONEFILE"

    if  [ -f $FILETMP ]
    then
        echo "rm $FILETMP"
        rm $FILETMP
    fi
    
    echo "lrr_DownloadFromRemote -u ${FTPUSERSUPPORT} -w ${FTPPASSSUPPORT} -a ${FTPHOSTSUPPORT} -p ${FTPPORTSUPPORT} -l ${FILETMP} -r ${FILESRC} -s ${USE_SFTP_SUPPORT}"
    lrr_DownloadFromRemote -u ${FTPUSERSUPPORT} -w ${FTPPASSSUPPORT} -a ${FTPHOSTSUPPORT} -p ${FTPPORTSUPPORT} -l ${FILETMP} -r ${FILESRC} -s ${USE_SFTP_SUPPORT}
    
    exit 1
fi


if  [ "$FORCED" = "0" ]
then
    if  [ -f $ROOTACT/usr/etc/lrr/dntimezone_locked ]
    then
        echo "Timezone changed feature disabled by ~/usr/etc/lrr/dntimezone_locked"
        exit 1
    fi
fi

if  [ -z "$TIMEZONEFILE" ]
then
    echo "the name of the timezone file is required"
    exit 1
else
    FILESRC="$FTPHOSTDIR/$TIMEZONEFILE"
    FILETMP="$TMPDIR/$TIMEZONEFILE"
    FILEDST="$TIMEZONEDIR/$TIMEZONEFILE"
    DNTIMEZONE=${TIMEZONEFILE}
fi

if  [ -f $FILETMP ]
then
    echo    "rm $FILETMP"
    rm $FILETMP
fi

echo "lrr_DownloadFromRemote -u ${FTPUSERSUPPORT} -w ${FTPPASSSUPPORT} -a ${FTPHOSTSUPPORT} -p ${FTPPORTSUPPORT} -l ${FILETMP} -r ${FILESRC} -s ${USE_SFTP_SUPPORT}"
lrr_DownloadFromRemote -u ${FTPUSERSUPPORT} -w ${FTPPASSSUPPORT} -a ${FTPHOSTSUPPORT} -p ${FTPPORTSUPPORT} -l ${FILETMP} -r ${FILESRC} -s ${USE_SFTP_SUPPORT}

if [ $? != "0" ]
then                     
    echo    "lrr_DownloadFromRemote failure $FILETMP not downloaded"
    exit 1
fi   

if [ ! -s $FILETMP ]
then
    echo    "lrr_DownloadFromRemote failure $FILETMP not downloaded size = 0"
    exit 1
fi

# Change timezone
mkdir -p $TIMEZONEDIR
echo "cp $FILETMP $FILEDST"
cp $FILETMP $FILEDST

echo "ln -sf $FILEDST $LOCALTIME"
ln -sf $FILEDST $LOCALTIME

if [ -d $ARCHIVE ]
then
        mkdir -p ${ARCHIVE}/actility/packages
        mv $FILETMP ${ARCHIVE}/actility/packages
else
    rm $FILETMP
fi

echo    ${DNTIMEZONE} > $ROOTACT/usr/etc/lrr/dntimezone_last
echo    "$(date '+%Y/%m/%d %H:%M:%S') : ${DNTIMEZONE} cmd=[$SAVECMD]" >> $ROOTACT/usr/etc/lrr/dntimezone_history

exit $?
