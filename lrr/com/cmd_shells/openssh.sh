#!/bin/sh

#
#

# PATH for sshpass command
PATH=.:$PATH

cd $ROOTACT/lrr/com
#killall sshpass.x

[ -z "$SSHUSERSUPPORT" ] && SSHUSERSUPPORT=support
[ -z "$SSHPASSSUPPORT" ] && SSHPASSSUPPORT=support
[ -z "$SSHHOSTSUPPORT" ] && SSHHOSTSUPPORT=support1.actility.com
[ -z "$SSHPORTSUPPORT" ] && SSHPORTSUPPORT=81

[ -z "$BKPSSHUSERSUPPORT" ] && BKPSSHUSERSUPPORT=support
[ -z "$BKPSSHPASSSUPPORT" ] && BKPSSHPASSSUPPORT=support
[ -z "$BKPSSHHOSTSUPPORT" ] && BKPSSHHOSTSUPPORT=support2.actility.com
[ -z "$BKPSSHPORTSUPPORT" ] && BKPSSHPORTSUPPORT=81

[ -z "$FTPUSERSUPPORT" ] && FTPUSERSUPPORT=ftp
[ -z "$FTPPASSSUPPORT" ] && FTPPASSSUPPORT=
[ -z "$FTPHOSTSUPPORT" ] && FTPHOSTSUPPORT=support1.actility.com
[ -z "$FTPPORTSUPPORT" ] && FTPPORTSUPPORT=21

[ -z "$BKPFTPUSERSUPPORT" ] && BKPFTPUSERSUPPORT=ftp
[ -z "$BKPFTPPASSSUPPORT" ] && BKPFTPPASSSUPPORT=
[ -z "$BKPFTPHOSTSUPPORT" ] && BKPFTPHOSTSUPPORT=support2.actility.com
[ -z "$BKPFTPPORTSUPPORT" ] && BKPFTPPORTSUPPORT=21


REQUIREDPORT=2009
BREAKCNX=3600
AUTOSTART="0"

#-o StrictHostKeyChecking is not available on wirmav2
#-Itmt is only available on wirmav2 but it seems it does not work

#echo	"SYSTEM=$SYSTEM"

case	$SYSTEM in
	wirmav2)
		SSHOPT="-y -N -I600"
	;;
	fcloc|fclamp|fcpico|tektelic)
		SSHOPT="-y -N -y -y"
	;;
	*)
		SSHOPT="-y -N -o StrictHostKeyChecking=no"
	;;
esac

while	[ $# -gt 0 ]
do
	case	$1 in 
		-a)
			shift
			AUTOSTART="1"
		;;
		-P)
			shift
			REQUIREDPORT="${1}"
			shift
		;;
		-I)
			shift
			BREAKCNX="${1}"
			shift
		;;
		-K)
			shift
			rm ${HOME}/.ssh/known_hosts
		;;
		-A)
			shift
			SSHHOSTSUPPORT="${1}"
			shift
		;;
		-D)
			shift
			SSHPORTSUPPORT="${1}"
			shift
		;;
		-U)
			shift
			SSHUSERSUPPORT="${1}"
			shift
		;;
		-W)
			shift
			SSHPASSSUPPORT="${1}"
			shift
		;;
		*)
			shift
		;;
	esac
done

if [ "$AUTOSTART" = "1" ]
then
	FILEDST=/tmp/autorevssh.txt
	URLSRC=ftp://${FTPUSERSUPPORT}@${FTPHOSTSUPPORT}:${FTPPORTSUPPORT}
	URLSRC=${URLSRC}/AUTOREVSSH_LRR/${LRRID}
	rm ${FILEDST} 2> /dev/null
	echo "wget -O ${FILEDST} ${URLSRC}"
	wget -O ${FILEDST} ${URLSRC}
	if [ $? = "0" -a -s "$FILEDST" ]
	then
		REQUIREDPORT=$(cat $FILEDST)
		echo	"support provides auto reverse ssh port ${REQUIREDPORT}"
	else
		echo	"support does not provide auto reverse ssh port"
		FTPUSERSUPPORT=${BKPFTPUSERSUPPORT}
		FTPPASSSUPPORT=${BKPFTPPASSSUPPORT}
		FTPHOSTSUPPORT=${BKPFTPHOSTSUPPORT}
		FTPPORTSUPPORT=${BKPFTPPORTSUPPORT}

		URLSRC=ftp://${FTPUSERSUPPORT}@${FTPHOSTSUPPORT}:${FTPPORTSUPPORT}
		URLSRC=${URLSRC}/AUTOREVSSH_LRR/${LRRID}
		rm ${FILEDST} 2> /dev/null
		echo "wget -O ${FILEDST} ${URLSRC}"
		wget -O ${FILEDST} ${URLSRC}
		if [ $? = "0" -a -s "$FILEDST" ]
		then
			REQUIREDPORT=$(cat $FILEDST)
			echo	"support bkp provides auto reverse ssh port ${REQUIREDPORT}"
		else
			echo	"support bkp does not provide auto reverse ssh port"
		fi
	fi
fi


ACCOUNT="${SSHUSERSUPPORT}@${SSHHOSTSUPPORT}"
REVERSE="${SSHHOSTSUPPORT}:${REQUIREDPORT}:localhost:22"

# -e option of sshpass
SSHPASS=${SSHPASSSUPPORT}
export SSHPASS

# the space (' ') between -R and "${REVERSE}" is mandatory !!!!

echo sshpass.x -e ssh ${SSHOPT} -p${SSHPORTSUPPORT} -R "${REVERSE}" "${ACCOUNT}"
sshpass.x -e ssh ${SSHOPT} -p${SSHPORTSUPPORT} -R "${REVERSE}" "${ACCOUNT}" &
bkgpid=$!
echo	"pidssh $bkgpid remoteport ${REQUIREDPORT}"
while	[ $BREAKCNX -gt 0 ]
do
	kill -0 $bkgpid
	if	[ $? != "0" ]
	then
		exit	1
	fi
#	echo	"pidssh $bkgpid still present"
	sleep	1
	BREAKCNX=$(expr $BREAKCNX - 1)
done
kill $bkgpid
exit 0
