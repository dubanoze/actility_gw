#!/bin/sh

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

echo "BEFORE ANYTHING -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -s ${USE_SFTP}"

#tests values
#FTPUSERDL=ftp
#FTPPASSDL=
#FTPHOSTDL=192.168.1.159
#FTPPORTDL=21
#MD5VERIF="5ce98edbeff1adf55773cfceb1bc80a1"

if      [ -z "$FTPHOSTDL" ]
then
        FTPHOSTDL=lrc-dev.thingpark.com
fi

case    $# in
        *)
                while   [ $# -gt 0 ]
                do
                        case    $1 in
                                -V)
                                        shift
                                        VERSION="${1}"
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

if      [ -z "$VERSION" ]
then
        echo    "requested version missing (-V option)"
        exit 1
fi

export PATH=.:$PATH

if      [ -z "$ROOTACT" ]
then
        export ROOTACT=/tmp/mdm/pktfwd/firmware
fi

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
        . ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then

        . ${ROOTACT}/lrr/com/_functions.sh
fi




# old installs do not have SYSTEM in _parameters.sh
if      [ -z "$SYSTEM" ]
then
        SYSTEM=$LRRSYSTEM
fi

if      [ "$OPTSYSTEM" != "" ]
then
        SYSTEM=$OPTSYSTEM
fi

FILESRC="firmware-${VERSION}-${SYSTEM}.tar.gz"
FILEDST="/tmp/firmware-${VERSION}-${SYSTEM}.tar.gz"


echo "lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -r ${FILESRC} -l ${FILEDST} -s ${USE_SFTP}"
lrr_DownloadFromRemote -u ${FTPUSERDL} -w ${FTPPASSDL} -a ${FTPHOSTDL} -p ${FTPPORTDL} -r ${FILESRC} -l ${FILEDST} -s ${USE_SFTP}

if [ $? != "0" ]
then
        echo    "lrr_DownloadFromRemote failure $FILEDST not downloaded"
        exit 1
fi
if [ ! -s "$FILEDST" ]
then
        echo    "lrr_DownloadFromRemote failure $FILEDST not downloaded size = 0"
        exit 1
fi

if [ ! -f "$FILEDST" -o ! -s "$FILEDST" ]
then
        echo    "lrr_DownloadFromRemote failure $FILEDST not downloaded size = 0"
        exit 1
fi



sleep 1

echo "Firmware is succesfully put in place at ${FILEDST}"
echo "Upgrade/Downgrade is performing..."
echo "It will last for couple of minutes..."
# Copy firmware to host's flash: , delete firmware from /tmp directory and start upgrade/downgrade
$ROOTACT/lrr/com/cmd_shells/ciscoms/exec_updowngrade.sh ${FILEDST}

exit $?

