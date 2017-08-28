#!/bin/sh

#this works with vsftpd server on ubuntu
#sudo apt-get install vsftpd
#sudo mkdir /srv/ftp
#sudo usermod -d /srv/ftp ftp 
#sudo /etc/init.d/vsftpd restart
#source file name are relative to /srv/ftp directory

ARCHIVE=/mnt/mmcblk0p1

#default values
USER="ftp"
PASSWORD=""
PORT=21
LRC=lrc1.thingpark.com
VERSION=""
MD5VERIF=""
FORCED="0"
OPTSYSTEM=""
DOWNLOAD="1"

#tests values
#USER="ftp"
#PASSWORD=""
#PORT=21
#LRC=192.168.1.159
#MD5VERIF="5ce98edbeff1adf55773cfceb1bc80a1"

LRRDEV=$(uname -n)
if	[ "$LRRDEV" = "Wirgrid_08020016" -o "$LRRDEV" = "pca-ESPRIMO-P5730" ]
then
	PORT=21
	LRC=192.168.1.159
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
				-V)
					shift
					VERSION="${1}"
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
				-S)
					shift
					OPTSYSTEM="${1}"
					shift
				;;
				-D)
					shift
					DOWNLOAD="${1}"
					shift
				;;
				-p)
					shift
					CISCOPWD="${1}"
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
CURRENTVERSION=`cat lrr/Version`

echo	"current version $CURRENTVERSION"
echo	"request version $VERSION"

if	[ -z "$VERSION" ]
then
	echo	"requested version missing (-V option)"
	exit 1
fi

if	[ "$FORCED" = "0" ]
then
	if	[ "$CURRENTVERSION" = "$VERSION" ]
	then
		echo	"Version $VERSION already installed"
		exit 0
	fi
fi

if	[ "$FORCED" != "2" ]
then
	if	[ -f $ROOTACT/usr/etc/lrr/upgrade_locked ]
	then
		echo	"auto upgrade feature disabled by ~/usr/etc/lrr/upgrade_locked"
		exit 1
	fi
fi

# old installs do not have SYSTEM in _parameters.sh
if	[ -z "$SYSTEM" ]
then
	SYSTEM=$LRRSYSTEM
fi

if	[ "$OPTSYSTEM" != "" ]
then
	SYSTEM=$OPTSYSTEM
fi

if [ "$SYSTEM" = "ir910" ]
then
	export PYTHONPATH=/usr/bin/cal
	FILESRC="lrr_${VERSION}_${SYSTEM}.opk"
	FILEDST="/mnt/apps/lrr_${VERSION}_${SYSTEM}.opk"
	FILESRCKEY="lrr-opk.pubkey"
	FILEDSTKEY="/mnt/apps/lrr-opk.pubkey"
else
	FILESRC="lrr-${VERSION}-${SYSTEM}.tar.gz"
	FILEDST="/tmp/lrr-${VERSION}-${SYSTEM}.tar.gz"
	FILESRCMD5="lrr-${VERSION}-${SYSTEM}.md5"
	FILEDSTMD5="/tmp/lrr-${VERSION}-${SYSTEM}.md5"
fi

if	[ "$DOWNLOAD" = "1" -a -f "$FILEDST" ]
then
	echo	"rm $FILEDST"
	rm $FILEDST
fi
if	[ "$DOWNLOAD" = "1" -a -f "$FILEDSTMD5" ]
then
	echo	"rm $FILEDSTMD5"
	rm $FILEDSTMD5
fi

#echo "ftpget -v -P $PORT -u $USER -p '$PASSWORD' $LRC $FILEDST $FILESRC"
#ftpget -v -P $PORT -u $USER -p "$PASSWORD" $LRC $FILEDST $FILESRC

if	[ "$DOWNLOAD" = "1" ]
then
	URLSRC=ftp://${USER}@${LRC}:${PORT}/${FILESRC}
	if [ "$SYSTEM" = "ir910" ]
	then
		echo "curl -o ${FILEDST} ${URLSRC}"
		curl -o ${FILEDST} ${URLSRC}
	else
		echo "wget -O ${FILEDST} ${URLSRC}"
		wget -O ${FILEDST} ${URLSRC}
	fi

	if [ $? != "0" ]
	then                     
		echo    "transfer failure $FILEDST not downloaded"
		exit 1
	fi   
	if [ ! -s "$FILEDST" ]
	then
		echo	"transfer failure $FILEDST not downloaded size = 0"
		exit 1
	fi
fi

if	[ "$SYSTEM" = "ir910" ]
then
	URLSRC=ftp://${USER}@${LRC}:${PORT}/${FILESRCKEY}
	echo "curl -o ${FILEDSTKEY} ${URLSRC}"
	curl -o ${FILEDSTKEY} ${URLSRC}
	if [ $? != "0" ]
	then                     
		echo    "transfer failure $FILEDSTKEY not downloaded"
		exit 1
	fi   
	if [ ! -s "$FILEDSTKEY" ]
	then
		echo	"transfer failure $FILEDSTKEY not downloaded size = 0"
		exit 1
	fi
fi

if	[ -z "$MD5VERIF" -a "$SYSTEM" != "ir910" ]
then
	if	[ "$DOWNLOAD" = "1" -a -z "$MD5VERIF" ]
	then
		URLSRC=ftp://${USER}@${LRC}:${PORT}/${FILESRCMD5}
		echo "wget -O ${FILEDSTMD5} ${URLSRC}"
		wget -O ${FILEDSTMD5} ${URLSRC}
	fi
	if	[ -s "${FILEDSTMD5}" ]
	then
		MD5VERIF=`cat $FILEDSTMD5 | tr -d '\015'`
		echo	"md5 $MD5VERIF from $FILEDSTMD5"
	else
		echo	"md5 failure ${FILEDSTMD5} not downloaded"
		exit 1
	fi
fi

if [ ! -f "$FILEDST" -o ! -s "$FILEDST" ]
then
	echo	"transfer failure $FILEDST not downloaded size = 0"
	exit 1
fi

if	[ "$SYSTEM" != "ir910" ]
then
	MD5SUM=`md5sum $FILEDST`
	if [ $? != "0" ]
	then                     
		echo	"can not md5sum on $FILEDST"
		exit 1
	fi
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
	echo	"file $FILEDST downloaded"
fi


cd $ROOTACT

if	[ -d lrr ]
then
	if	[ -d lrr.previous ]
	then
		rm -fr lrr.previous
	fi
	mv lrr lrr.previous
fi

if	[ "$SYSTEM" = "ir910" ]
then
	# check if lrr public key is installed
	checkkey=$(clish -c "enable $CISCOPWD" -c "show package key" | grep lrr)
	echo "checking key: $checkkey"

	if [ -z "$checkkey" ]
	then
		# install public key
		instkey=$(clish -c "enable $CISCOPWD" -c "package import-key lrr flash0:/lrr-opk.pubkey")
		echo "install key=[$instkey]"

	fi

	instlrr=$(clish -c "enable $CISCOPWD" -c "package install flash0:/$FILESRC")
	if	[ $? != "0" -o ! -x ./lrr/com/lrr.x ]
	then
		echo	"install lrr error"
		if	[ -d lrr.previous ]
		then
			rm -fr lrr
			mv lrr.previous lrr
		fi
		exit	1
	fi
	echo "install lrr:"
	echo "$instlrr"

else
	#tar xvf $FILEDST
	tar xf $FILEDST
	if	[ $? != "0" -o ! -x ./lrr/com/lrr.x ]
	then
		echo	"tar extract error"
		if	[ -d lrr.previous ]
		then
			rm -fr lrr
			mv lrr.previous lrr
		fi
		exit	1
	fi
fi

if	[ -x ./lrr/com/_afterupgrade.sh ]
then
	export CURRENTVERSION
	export VERSION
	./lrr/com/_afterupgrade.sh
# the afterupgrade may have change some params, read them
	. $ROOTACT/lrr/com/_parameters.sh
fi
sleep 1


TEST=$(./lrr/com/lrr.x --version)

echo "TESTS results :"
echo "$TEST"

NEWVERSION=$(echo "$TEST" | grep "$VERSION")
if	[ -z "$NEWVERSION" ]
then
	echo	"Version "$VERSION" not retrieved in run test"
	if	[ -d lrr.previous ]
	then
		rm -fr lrr
		mv lrr.previous lrr
	fi
	exit 1
fi

if [ "$DOWNLOAD" = "1" -a -d $ARCHIVE ]
then
	mkdir -p ${ARCHIVE}/actility/packages
	mv $FILEDST ${ARCHIVE}/actility/packages
fi

$ROOTACT/lrr/com/cmd_shells/restart.sh
exit $?
