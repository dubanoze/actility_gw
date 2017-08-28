#!/bin/sh
# vim: ft=sh: set noet ts=4 sw=4:
#
# custom.ini sample
#[board:0]
#    aeskey=D6AABE10F7D06A25E70C99EA45A71D92
#

MAX_NB_BOARDS=4
MOREINI="$ROOTACT/lrr/moreini/moreini.x"
KEY_INI_FILE="$ROOTACT/usr/etc/lrr/custom.ini"
TMPFILE="/tmp/setkey.ini"
RESTART_LRR="$ROOTACT/lrr/com/cmd_shells/restart.sh"

usage() {

	echo "Usage: setkeys.sh -card<idx> <key> [-card<idx> <key>]"
	echo "  with <idx> = [0..$(($MAX_NB_BOARDS - 1))]"
	echo "ex: setkeys.sh -card0 D6AABE10F7D06A25E70C99EA45A71D92 -card1 1D3DC9EBD0D62112687A7BFCEF1869A3"
}

if [ $# -lt 2 ] ; then
	usage
	exit 1
fi

case $# in
	*)
		while	[ $# -gt 0 ]
		do
			case	$1 in
				-card0)
					shift
					KEY_0="${1}"
					shift
					;;
				-card1)
					shift
					KEY_1="${1}"
					shift
					;;
				-card2)
					shift
					KEY_2="${1}"
					shift
					;;
				-card3)
					shift
					KEY_3="${1}"
					shift
					;;
				-card*)
					echo "$1: board index out of range (0..$(($MAX_NB_BOARDS - 1)))"
					shift
					;;
				*)
					shift
					;;
			esac
		done
	;;
esac


set_key_n() {
	# $1 = board index [0..$MAX_NB_BOARDS-1]
	# $2 = aes key
	BoardIdx=$1
	BoardKey=$2
	if [ ! -z $BoardKey ] ; then
		echo "Setting board $1 with AES-128 key $2"

		{
			echo "[board:${BoardIdx}]"
			echo "	aeskey=${BoardKey}"
		} >> $TMPFILE
	fi
}


cat /dev/null > $TMPFILE

for i in $(seq 0 $(($MAX_NB_BOARDS - 1)) ) ; do
	case $i in
		0)	set_key_n $i $KEY_0 ;;
		1)	set_key_n $i $KEY_1 ;;
		2)	set_key_n $i $KEY_2 ;;
		3)	set_key_n $i $KEY_3 ;;
		*)	echo "wrong card index" ;;
	esac

done

$MOREINI -t $KEY_INI_FILE -a $TMPFILE -y
ret=$?
rm -f $TMPFILE > /dev/null 2>&1
if [ $ret -ne 0 ] ; then
	echo "error with $MOREINI. lrr will not restart"
else
	# Restart LRR
	$RESTART_LRR
fi

exit $ret

