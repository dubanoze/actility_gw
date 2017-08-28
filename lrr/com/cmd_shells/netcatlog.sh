#!/bin/sh

#default values



HOST=""
PORT=""

while	[ $# -gt 0 ]
do
	case	$1 in
		-A)
			shift
			HOST="${1}"
			shift
		;;
		-P)
			shift
			PORT="${1}"
			shift
		;;
		*)
			shift
		;;
	esac
done

[ -z "$HOST" ] && exit 1
[ -z "$PORT" ] && exit 1

echo	"$HOST $PORT"

nc $HOST $PORT -e tail -f $ROOTACT/var/log/lrr/TRACE.log
exit $?
