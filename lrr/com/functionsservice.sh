
SERVICE=""
PIDFILE=""
STOPFILE=""
FORCE="0"
SERVICE_PID=""
SERVICE_NAME=""
START_DATE=""
NB_RESTARTS=""
OPTIONS=""

writeStatus() {
    if [ "$1" = "0" ]; then
        echo "[OK]"
    else
        echo "[FAILED]"
    fi
}

extractPidFile() {
    SERVICE_PID=$(cat $PIDFILE | cut -d "|" -f1)
    SERVICE_NAME=$(cat $PIDFILE | cut -d "|" -f2)
    START_DATE=$(cat $PIDFILE | cut -d "|" -f3)
    NB_RESTARTS=$(cat $PIDFILE | cut -d "|" -f4)
}

serviceStarted() {
    if [ -z "$SERVICE_PID" -a -f "$PIDFILE" ]; then
        extractPidFile
    fi
    ret=1
    if [ -n "$SERVICE_PID" ]; then
        PID_NAME=$(cat /proc/$SERVICE_PID/stat 2> /dev/null | cut -d " " -f2)
        SS_NB_ATTEMPTS=0
        while [ "$PID_NAME" = "(nohup)" -a $SS_NB_ATTEMPTS -lt 20 ]; do
            sleep 1
            PID_NAME=$(cat /proc/$SERVICE_PID/stat 2> /dev/null | cut -d " " -f2)
            SS_NB_ATTEMPTS=$(expr $SS_NB_ATTEMPTS + 1)
        done
        if [ "$PID_NAME" = "($SERVICE_NAME)" ]; then
            kill -0 $SERVICE_PID
            ret=$?
        fi
    fi
    return $ret
}

respawnService() {
    retval=0

    rm -f $STOPFILE

    [ ! -z "$SERVICE_RUNDIR" ] && cd $SERVICE_RUNDIR

    COMMAND=$(serviceCommand)
    eval $COMMAND
    retval=$?

    while [ ! -f $STOPFILE ]
    do
        sleep 5
        echo "Respawning service $SERVICE" | stdin-logger $ROOTACT/var/log/respawn/ respawn 32 2 1 1 0
        extractPidFile
        NB_RESTARTS=$(expr $NB_RESTARTS + 1)
        echo "$SERVICE_PID|$SERVICE_NAME|$START_DATE|$NB_RESTARTS" > $PIDFILE
        eval $COMMAND
        retval=$?
    done

    rm -f $STOPFILE

    exit $retval
}

start() {
    echo -n "Starting $SERVICE service: "

    if serviceStarted; then
        ret=$?
        writeStatus $ret
        echo "$SERVICE is already started with PID $SERVICE_PID"
    else
        cd $(dirname $0)
        SERVICE_DIR=$(pwd)
        cd - > /dev/null
        SERVICE_NAME=$(basename $0)
        SERVICE_PID=""
        preStart

        START_DATE=$(date +%Y-%m-%dT%H:%M:%S%z)
        nohup $SERVICE_DIR/$SERVICE_NAME _respawnService $OPTIONS < /dev/null > /dev/null 2>&1 &
        SERVICE_PID=$!
        PIDDIR=$(dirname $PIDFILE)
        [ ! -d $PIDDIR ] && mkdir -p $PIDDIR
        echo "$SERVICE_PID|$SERVICE_NAME|$START_DATE|0" > $PIDFILE
        ret=$?

        postStart $ret
        ret=$?

        writeStatus $ret
    fi

    return $ret
}

stop() {
    echo -n "Stopping $SERVICE service: "
    if serviceStarted; then
        preStop

        touch $STOPFILE

        if [ $FORCE == 1 ]; then
            abortService
        else
            stopService

            S_NB_ATTEMPTS=0
            while serviceStarted && [ $S_NB_ATTEMPTS -lt 20 ]; do
                sleep 1
                stopService
                S_NB_ATTEMPTS=$(expr $S_NB_ATTEMPTS + 1)
            done

            # normal shutdown failed, force shutdown
            if serviceStarted; then
                echo "[FAILED]"
                ALL_PIDS=$($ROOTACT/etc/pids $SERVICE_PID | tr -s '\n' ' ')
                echo -n "Abort $SERVICE service: "
                abortService
            fi
        fi

        # normal shutdown failed, force shutdown
        if serviceStarted; then
            echo "[FAILED]"
            ALL_PIDS=$($ROOTACT/etc/pids $SERVICE_PID | tr -s '\n' ' ')
            echo -n "Shutting down pids: "
            kill -KILL $ALL_PIDS
            sleep 5
        fi
        rm -f $PIDFILE
        ret=$?

        postStop

        writeStatus $ret
    else
        ret=1
        writeStatus $ret

        echo "$SERVICE is already stopped"
    fi
    return $ret
}

status() {
    if serviceStarted; then
        echo "$SERVICE service is started (name=$SERVICE_NAME,pid=$SERVICE_PID,startDate=$START_DATE,nbRestarts=$NB_RESTARTS)"
        detailedStatus
    else
        echo "$SERVICE service is stopped"
    fi
}

printInfosAndCheck() {
    if serviceStarted; then
        printInfos
    else
        echo "$SERVICE service is stopped"
        return 1
    fi
}

dumpStateAndCheck() {
    if serviceStarted; then
        dumpState $1 $2
    else
        echo "$SERVICE service is stopped"
        return 1
    fi
}

healthAndCheck() {
    if [ "$1" = "test" ]; then
      return 0
    fi
    if serviceStarted; then
        health
    else
        echo "$SERVICE service is stopped"
        return 1
    fi
}

restart() {
    stop $*
    sleep 3
    start $*
}

handleParams() {
    case "$1" in
        _respawnService)
            shift
            respawnService
            ;;
        start)
            shift
            start
            ;;
        stop)
            shift
            stop
            ;;
        restart)
            shift
            restart
            ;;
        status)
            shift
            status
            ;;
        printInfos)
            shift
            printInfosAndCheck
            ;;
        dumpState)
            shift
            dumpStateAndCheck $1 $2
            ;;
        health)
            shift
            healthAndCheck $1
            ;;
        *)
            usage
            exit 1
    esac
}

#Function to override

usage() {
    :
}


# Called before starting the service
# Can be use to init and prepare the startService function (which occurs in a child process)
# Available parameters:
#  * SERVICE_NAME (in) = name that is used to start the service
#  * OPTIONS (out) = options that will be passed to _startService function
preStart() {
   :
}

# Called after the service is started in a child process
# Perform post installation actions like waiting that the the service is effectively started
# Available parameters:
#  * SERVICE_NAME (in) = name that is used to start the service
#  * SERVICE_PID (in) = pid of the child process which has started the service
#  * $1 (in) = status of the previously started service (if different from 0, the service has not been started)
postStart() {
    :
}

# Prints the service command that will be executed through a respawn script. This function is called in a child process
# Available parameters:
#  * <empty>
serviceCommand() {
    :
}

# Called before stopping the service
# Can be use to prepare the stopService function
# Available parameters:
#  * SERVICE_NAME (in) = name that is used to start the service
#  * SERVICE_PID (in) = pid of the child process which has started the service
preStop() {
    :
}

# Called after stopping the service
# Can be use to cleanup service data if needed
# Available parameters:
#  * SERVICE_NAME (in) = name that is used to start the service
#  * SERVICE_PID (in) = pid of the child process which has started the service
postStop() {
    :
}

# Called for stopping the service
# Available parameters:
#  * SERVICE_NAME (in) = name that is used to start the service
#  * SERVICE_PID (in) = pid of the child process which has started the service
stopService() {
    :
}

abortService() {
    :
}

detailedStatus() {
    :
}

printInfos() {
    :
}

dumpState() {
    :
}

health() {
    :
}



