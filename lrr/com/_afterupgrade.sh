#!/bin/sh

FROM="${CURRENTVERSION}"
TO="${VERSION}"
FROMTO=${FROM}_${TO}

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then
	. ${ROOTACT}/lrr/com/_functions.sh
fi

echo	"_afterupgrade.sh on=$SYSTEM from=$FROM to=$TO"

if [ "$SYSTEM" = "ciscoms" -o "$SYSTEM" = "fcpico" -o "$SYSTEM" = "fclamp" -o "$SYSTEM" = "fcmlb" -o "$SYSTEM" = "fcloc" -o "$SYSTEM" = "gemtek" ]; then
	${ROOTACT}/lrr/com/sysconfiglrr.sh
fi

case	$FROMTO	in
	1.2.10_1.2.11)
	;;
	1.2.11_1.4.*)
		if	[ "$SYSTEM" = "wirmav2" ]
		then
			${ROOTACT}/lrr/com/sysconfiglrr.sh -m spi -x x1
		fi
		reboot
	;;
	*)
	;;
esac
