#!/bin/sh
# vim: ft=sh: set noet ts=4 sw=4:
#
# custom.ini sample
#[antenna:0]
#    gain=6
#    cableloss=3
#

MAX_NB_ANTENNAS=8
MOREINI="$ROOTACT/lrr/moreini/moreini.x"
CUSTOM_INI_FILE="$ROOTACT/usr/etc/lrr/custom.ini"
TMPFILE="/tmp/setants.ini"
RESTART_LRR="$ROOTACT/lrr/com/cmd_shells/restart.sh"

usage() {

	echo "Usage: ants.sh -chain<idx> <antgain>-<cableloss> [-chain<idx> <antgain>-<cableloss>]"
	echo "  with <idx> = [0..$(($MAX_NB_ANTENNAS - 1))]"
	echo "ex: ants.sh -chain0 6-3 -chain1 4-1"
}

if [ $# -lt 2 ] ; then
	usage
	exit 1
fi

case $# in
	*)
		while	[ $# -gt 0 ]
		do
			case	"$1" in
				-chain0)
					shift
					CHAIN_0="${1}"
					shift
					;;
				-chain1)
					shift
					CHAIN_1="${1}"
					shift
					;;
				-chain2)
					shift
					CHAIN_2="${1}"
					shift
					;;
				-chain3)
					shift
					CHAIN_3="${1}"
					shift
					;;
				-chain4)
					shift
					CHAIN_4="${1}"
					shift
					;;
				-chain5)
					shift
					CHAIN_5="${1}"
					shift
					;;
				-chain6)
					shift
					CHAIN_6="${1}"
					shift
					;;
				-chain7)
					shift
					CHAIN_7="${1}"
					shift
					;;
				-chain*)
					echo "$1: chain index out of range (0..$(($MAX_NB_ANTENNAS - 1)))"
					shift
					;;
				*)
					shift
					;;
			esac
		done
	;;
esac



set_ant_n() {
	# $1 = ant index [0..$MAX_NB_ANTENNAS-1]
	# $2 = ant chain
	AntIdx=$1
	AntChain="$2"
	if [ ! -z $AntChain ] ; then
		echo "Setting antenna $1 with chain $2"
		AntGain=$(echo $AntChain | awk -F"-" '{print $1}')
		LineLoss=$(echo $AntChain | awk -F"-" '{print $2}')
		
		{
			echo "[antenna:${AntIdx}]"
			echo "	gain=${AntGain}"
			echo "	cableloss=${LineLoss}"
		} >> $TMPFILE
	fi
}


cat /dev/null > $TMPFILE

for i in $(seq 0 $(($MAX_NB_ANTENNAS - 1)) ) ; do
	case $i in
		0)	set_ant_n $i "$CHAIN_0" ;;
		1)	set_ant_n $i "$CHAIN_1" ;;
		2)	set_ant_n $i "$CHAIN_2" ;;
		3)	set_ant_n $i "$CHAIN_3" ;;
		4)	set_ant_n $i "$CHAIN_4" ;;
		5)	set_ant_n $i "$CHAIN_5" ;;
		6)	set_ant_n $i "$CHAIN_6" ;;
		7)	set_ant_n $i "$CHAIN_7" ;;
		*)	echo "wrong antenna index" ;;
	esac

done

$MOREINI -t $CUSTOM_INI_FILE -a $TMPFILE -y
ret=$?
rm -f $TMPFILE > /dev/null 2>&1
if [ $ret -ne 0 ] ; then
	echo "error with $MOREINI. lrr will not restart"
else
	# Restart LRR
	$RESTART_LRR
fi

exit $ret

