#!/bin/sh

# use by suplog because it does not get the lrr.x env context
if [ "$1" = "--readfile" ]
then
	file="$ROOTACT/usr/etc/lrr/_parameters.sh"
	[ -f "$file" ] && . $file
fi

echo	"OUI=${LRROUI}"
echo	"GID=${LRRGID}"
echo	"UID=${LRROUI}-${LRRGID}"
exit $?
