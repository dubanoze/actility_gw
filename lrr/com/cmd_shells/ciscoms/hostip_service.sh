#!/bin/sh
#
# hostip:       Starts the hostip

exec 2> /dev/null

ROOTACT=/tmp/mdm/pktfwd/firmware
export ROOTACT

CURRENT_DIR=`dirname $0`
. $ROOTACT/lrr/com/functionsservice.sh

OPTIONS=""
LRR_DATA=$ROOTACT/usr/data/lrr
SERVICE="hostip.sh"
SERVICE_RUNDIR="$ROOTACT/lrr/com/cmd_shells/ciscoms/"
PIDFILE=$LRR_DATA/hostip.pid
STOPFILE=$LRR_DATA/stop

usage() {
    echo "Usage: hostip [<options>] {start|stop|status|restart}"
    echo "  Where options are:"
    echo "   -h|--help    Print this help message"
}


preStart() {
        :
}

serviceCommand() {
        echo "./hostip.sh "$OPTIONS
}

stopService() {
        hostip_PIDS=$(pidof hostip.sh)
        [ -n "$hostip_PIDS" ] && kill -TERM $hostip_PIDS
}

abortService() {
        hostip_PIDS=$(pidof hostip.sh)
        [ -n "$hostip_PIDS" ] && kill -KILL $hostip_PIDS
}

handleParams $*

exit $?
