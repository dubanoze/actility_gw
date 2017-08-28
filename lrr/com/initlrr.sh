#!/bin/sh

#
#	only used for old versions where lrr.x was started via /etc/inittab
#


env > /tmp/initlrr.sh.env

# modem_on.sh calls others programs under /usr/local/bin, "." is required
# in PATH
export PATH=.:$PATH:/usr/local/bin
echo "PATH forced to $PATH"

if	[ -z "$ROOTACT" ]
then
	if	[ -d /mnt/fsuser-1/actility ]
	then
		export ROOTACT=/mnt/fsuser-1/actility
	else
		export ROOTACT=/home/actility
	fi
else
	echo	"ROOTACT already set : $ROOTACT"
fi

echo	"-----------------------------------------" >> /tmp/initlrr.sh.env
env >> /tmp/initlrr.sh.env

if	[ ! -d "$ROOTACT" ]
then
	echo	"$ROOTACT does not exist"
	exit	0
fi

mkdir -p $ROOTACT/usr/etc/lrr > /dev/null 2>&1

## power on semtech board on kerlink boxes
if	[ -f /usr/local/bin/modem_on.sh ]
then
	echo	"power on radio board with 'modem_on.sh'"
	cd /usr/local/bin
	./modem_on.sh
else
	echo	"no command to power on radio board"
fi

if	[ -x /usr/local/bin/gpio ]
then
	/usr/local/bin/gpio drive 0 0
fi

cd $ROOTACT/lrr/com
sleep 15
exec $ROOTACT/lrr/com/lrr.x $*
