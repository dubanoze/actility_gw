#!/bin/sh

ifconfig
echo	"--------------------------------------------------------------------"
route -n
echo	"--------------------------------------------------------------------"

if	[ -f /etc/resolv.conf ]; then
	cat /etc/resolv.conf
fi
echo	"--------------------------------------------------------------------"
type iptables >/dev/null 2>&1
if	[ $? = "0" ]; then
	iptables -L -n -v
fi
exit 0

