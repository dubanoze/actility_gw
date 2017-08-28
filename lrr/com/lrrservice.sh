#!/bin/sh
#

### BEGIN INIT INFO
# Provides:        lrr
# Required-Start:  $network $remote_fs $syslog
# Required-Stop:   $network $remote_fs $syslog
# Default-Start:   2 3 4 5
# Default-Stop:	   0 6
# Short-Description: Start the lrr
### END INIT INFO

# lrr:       Starts the lrr

exec 2> /dev/null

ROOTACT=_REPLACEWITHROOTACT_
export ROOTACT

CURRENT_DIR=`dirname $0`
. $ROOTACT/lrr/com/functionsservice.sh

OPTIONS=""
LRR_DATA=$ROOTACT/usr/data/lrr
SERVICE="lrr"
SERVICE_RUNDIR="$ROOTACT/lrr/com"
PIDFILE=$LRR_DATA/lrr.pid
STOPFILE=$LRR_DATA/stop

usage() {
    echo "Usage: lrr [<options>] {start|stop|status|restart}"
    echo "  Where options are:"
    echo "   -h|--help    Print this help message"
}

# Gemtek only
reset_power_rf()
{
        echo	"Reset [GIPO_1Cx] (power_rf_x)..."
        io -w -4 0x2000804c $(( $1&$((0xFFFFF)) ))
        io -w -4 0x2000800c $(( $1&$((0xFFFFF)) ))
        io -w -4 0x2000802c $(( $1&$((0xFFFF0)) ))
        sleep 0.5
        io -w -4 0x2000802c $(( $1&$((0xFFFFF)) ))
}

# Gemtek only
reset_sx1301()	
{
        echo	"Reset [GIPO_1Dx] (sx1301_x)..."
        # EN_GPIO
        io -w -4 0x2000804c $(( $1&$((0xFFFFFFF)) ))
        # DIR_GPIO
        io -w -4 0x2000800c $(( $1&$((0xFFFFFFF)) ))
        # DO_OFF_GW
        io -w -4 0x2000802c $(( $1&$((0xFFFFFFF)) ))
        sleep 0.5
        # DO_ON_GW
        io -w -4 0x2000802c $(( $1&$((0xFFFF0FF)) ))
}

preStart() {
	export PATH=.:$PATH:/usr/local/bin

	mkdir -p $LRR_DATA
	mkdir -p $ROOTACT/usr/etc/lrr > /dev/null 2>&1

	echo "$SYSTEM"

	## power on radio board on kerlink boxes
	if	[ -f /usr/local/bin/modem_on.sh ]
	then
		echo	"power on radio board with 'modem_on.sh'"
		cd /usr/local/bin
		./modem_on.sh
	else
		echo	"no command to power on radio board"
	fi

	# reset radio boards on Gemtek
	if [ "$SYSTEM" = "gemtek" ]
	then
		# reset board 0 (/dev/spidev0.0)
		reset_power_rf 0x10001
                reset_sx1301 0x1000100
		# reset board 1 (/dev/spidev1.0)
		reset_power_rf 0x20002
		reset_sx1301 0x2000200
	fi

	# usefull on raspberry pi
	if	[ -x /usr/local/bin/gpio ]
	then
		/usr/local/bin/gpio drive 0 0
	fi

	# treat watchdog for kerlink multislots
	if [ "$SYSTEM" = "wirmams" -o "$SYSTEM" = "wirmaar" ]
	then
		echo "start watchdog"
		watchdog /dev/watchdog

		dirbackact="/.update/packages/backupactility"
		dirback="/.update/packages/backup"
		dirbackupsav="/.update/packages/backup.savact"
		# after an execrff, backup directory disappear => restore it
		if [ -d "$dirbackupsav" -a ! -d "$dirback" ]
		then
			echo "restore backup directory after execrff"
			mount /dev/mmcblk3p2 /.update/ -o remount,rw
			mv $dirbackupsav $dirback
			sync
			mount /dev/mmcblk3p2 /.update/ -o remount,ro
		fi

		if [ -d "$dirbackact" ]
		then
			echo "synchronize backup with actility backup"
			mount /dev/mmcblk3p2 /.update/ -o remount,rw
			lst=$(ls $dirbackact/*.ipk)
			for f in $lst
			do
				# rename lrr_1.6.3_cortexa9hf-vfp-neon.ipk to lrr.ipk
				shortname=$(basename $f | awk 'BEGIN { FS="_" } ; { print $1 }')
				rm -f $dirback/${shortname}_*.ipk
				rm -f $dirback/${shortname}.ipk
				cp $f $dirback/$shortname.ipk
			done
			sync
			mount /dev/mmcblk3p2 /.update/ -o remount,ro
		fi

		if [ "$SYSTEM" = "wirmaar" ]
		then
			calibfile="/tmp/calib_loraloc.json"
			calibfilesav="$ROOTACT/usr/etc/lrr/calib_loraloc.json.sav"
			dyninifile="$ROOTACT/usr/etc/lrr/dyncalib.ini"
			if [ -f "$calibfile" ]
			then
				echo "Analyze calibration ..."
				cp $calibfile $calibfilesav
				python $ROOTACT/lrr/com/shells/wirmaar/convjson.py $calibfilesav > $dyninifile
				echo "File $dyninifile created"
			fi
		fi
	fi

	if [ "$SYSTEM" = "tektelic" ]
	then
		savdir="/home/backupactility"
		execrfffile="$savdir/RESTOREREQUESTED"
		lrrtarfile="$savdir/lrr.tar.gz"
		lrrconftarfile="$savdir/lrrconf.tar.gz"
		lrrdir=$ROOTACT/lrr
		lrrconf=$ROOTACT/usr/etc/lrr
		if [ -f "$execrfffile" -a -f "$lrrtarfile" -a -f "$lrrconftarfile" ]
		then
			echo "execrff has been requested, restoring lrr ..."
			rm -rf $lrrdir
			rm -rf $lrrconf
			cd /
			[ -f "$lrrtarfile" ] && tar xf $lrrtarfile
			[ -f "$lrrconftarfile" ] && tar xf $lrrconftarfile

			# exerff done, remove execrff file
			rm -f $execrfffile
			sync
			# reboot needed because network config files were restored
			echo "rebooting after execrff ..."
			$ROOTACT/lrr/com/cmd_shells/reboot_pending.sh 5
		else
			echo "execrff has been requested but no backup found ! Abort."
			rm -f $execrfffile
		fi
	fi

	if [ "$SYSTEM" = "wirmana" ]
	then
		savdir="/.update/backupactility"
		execrfffile="$savdir/RESTOREREQUESTED"
		lrrtarfile="$savdir/lrr.tar.gz"
		lrrconftarfile="$savdir/lrrconf.tar.gz"
		lrrdir=$ROOTACT/lrr
		lrrconf=$ROOTACT/usr/etc/lrr
		if [ -f "$execrfffile" ]
		then
			echo "execrff has been requested, restoring lrr ..."
			rm -rf $lrrdir
			rm -rf $lrrconf
			cd /
			[ -f "$lrrtarfile" ] && tar xf $lrrtarfile
			[ -f "$lrrconftarfile" ] && tar xf $lrrconftarfile

			# exerff done, remove execrff file
			mount /dev/mmcblk3p2 /.update/ -o remount,rw
			rm -f $execrfffile
			sync
			mount /dev/mmcblk3p2 /.update/ -o remount,ro
			# reboot needed because network config files were restored
			echo "rebooting after execrff ..."
			$ROOTACT/lrr/com/cmd_shells/reboot_pending.sh 5
		fi
	fi

	if [ "$SYSTEM" = "ciscoms" ]; then
		shcalib="$ROOTACT/lrr/com/shells/ciscoms/set_lut_calib.sh"
		if [ ! -f $shcalib ]; then
			echo "Warning: $shcalib tool not found !"
		else
			echo "Analyse calibration ..."
			$shcalib
			ret=$?
			if [ $ret -ne 0 ]; then
				echo "Error during LUT calibration process (err $ret)"
			fi
		fi
	fi

	if [ "$SYSTEM" = "fcloc" -o "$SYSTEM" = "fcmlb" ]; then
		echo "Configuring GPS stty driver"
		stty -F /dev/ttyS5 -echo igncr
	fi

	if [ "$SYSTEM" = "mtac_v1.0" -o "$SYSTEM" = "mtac_v1.5" ]; then
		echo "Use /dev/spidev0.0 regardless of the used MTAC AP slot"
		port1=/sys/devices/platform/mts-io/ap1
		port2=/sys/devices/platform/mts-io/ap2
		lora_hw=$(mts-io-sysfs show lora/hw-version 2> /dev/null)
		if [ -d $port1 ] && [[ $(cat $port1/hw-version) = $lora_hw ]]; then
			ln -sf /dev/spidev32766.2 /dev/spidev0.0
		elif [ -d $port2 ] && [[ $(cat $port2/hw-version) = $lora_hw ]]; then
			ln -sf /dev/spidev32765.2 /dev/spidev0.0
		fi
	fi
}

serviceCommand() {
	echo "lrr.x "$OPTIONS
	if [ "$SYSTEM" == "fcpico"  ]; then
		echo "1" > /sys/class/gpio/gpio67/value
	fi
}

stopService() {
	LRR_PIDS=$(pidof lrr.x)
	[ -n "$LRR_PIDS" ] && kill -TERM $LRR_PIDS
	if [ "$SYSTEM" == "fcpico" ]; then
		echo "0" > /sys/class/gpio/gpio67/value
	fi
}

abortService() {
	LRR_PIDS=$(pidof lrr.x)
	[ -n "$LRR_PIDS" ] && kill -KILL $LRR_PIDS
	if [ "$SYSTEM" == "fcpico" ]; then
		echo "0" > /sys/class/gpio/gpio67/value
	fi
}

[ -f "$ROOTACT/usr/etc/lrr/_parameters.sh" ] && . $ROOTACT/usr/etc/lrr/_parameters.sh

# getopt is not present on major gw
if [ "$SYSTEM" != "ir910" -a "$SYSTEM" != "fcmlb" -a "$SYSTEM" != "fcpico" -a "$SYSTEM" != "fclamp" -a "$SYSTEM" != "wirmams" -a "$SYSTEM" != "wirmaar" -a "$SYSTEM" != "fcloc" -a "$SYSTEM" != "wirmana" -a "$SYSTEM" != "tektelic" ]
then
	GETOPTTEMP=`getopt -o a:hi --long help,init -- $@`
	if [ $? != 0 ] ; then usage >&2 ; exit 1 ; fi
	eval set -- "$GETOPTTEMP"

	# Read the arguments
	while [ -n "$1" ]
	do
	    case "$1" in
		"-a") shift; OPTIONS=$OPTIONS" -a "$1;;
		"-h"|"--help") usage; exit;;
		"-i"|"--init") OPTIONS=$OPTIONS" -i";;
		"--") shift; break ;;
		* ) echo "Internal error ($*)!"; exit 1;;
	    esac
	    shift
	done
fi

handleParams $*

exit $?

