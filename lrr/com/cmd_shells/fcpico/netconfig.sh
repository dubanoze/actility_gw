#!/bin/bash
#
#	Analyze a /etc/network/interfaces, ntp.conf and vpn.cfg files
#
#	The format expected for /etc/network/interfaces is the following:
#...
#iface eth0 inet static
#        address 192.168.1.205
#        netmask 255.255.255.0
#        gateway 192.168.1.1
#        dns-nameservers 192.168.1.254 192.168.1.1
# 		<= blank line, end of the 'iface eth0' section
#...
#
# That means: no blank line inside 'iface eth0' section, and one blank line
# is mandatory at then end of the section
#
# 	The format expected for ntp.conf is:
#...
#server pool.ntp.org
#...
#
#	The format expected for vpn.cfg is:
#...
#SRV=195.65.47.47
#...
#

TMPINTER="/tmp/_lrrinter"
TMPNTP="/tmp/_lrrntp"
TMPKEY="/tmp/_lrrkey"
DHCP=0
IP=""
MASK=""
DNS=""
GW=""
NTP=""
KEYINST=""

use()
{
	echo "netconfig [-i <interfacesfile>] [-n <ntpfile>] -k <keyinstfile> [--dhcp 0|1 --address <ipaddr> --dns <dnslist> --ntp <ntpaddr> --mask <mask>] --gw <gatewayaddr> --key <keyinstaddr>]"
	echo "  options -i and -n are useless for this gateway"
	exit 1
}

finished()
{
	echo "DHCP=$DHCP"
	echo "IP=$IP"
	echo "MASK=$MASK"
	echo "GW=$GW"
	echo "DNS=$DNS"
	echo "NTP=$NTP"
	echo "KEYINST=$KEYINST"

	exit 0
}

printconf()
{
	if [ $DHCP = 1 ]
	then
		echo "iface eth0 inet dhcp"
		echo
		return
	fi

	echo "iface eth0 inet static"
	echo "	address $IP"
	echo "	netmask $MASK"
	[ ! -z "$GW" ] && echo "	gateway $GW"
	echo "	dns-nameservers $DNS"
}

readkey()
{
	if [ ! -f "$1" ]
	then
		echo "File '$1' not found ! >&2"
		return
	fi
	KEYINST=$(grep "^SRV=" $1 | awk -F '=' '{ print $2 }')
}

readntp()
{
	NTP=$(nvram get ntp_serv_addr)
}

readinter()
{
	DHCP=$(nvram get eth_ip_type)
	IP=$(nvram get eth_ipaddr)
	MASK=$(nvram get eth_netmask)
	GW=$(nvram get eth_gwaddr)
	DNS="$(nvram get eth_dns_pri) $(nvram get eth_dns_sec)"
}

writeinter()
{
	nvram set eth_ip_type=$DHCP
	permnvram save eth_ip_type
	if [ $DHCP = 0 ]
	then
		nvram set eth_ipaddr=$IP
		nvram set eth_netmask=$MASK
		nvram set eth_gwaddr=$GW
		if [ ! -z "$DNS" ]
		then
			set $DNS
			nvram set eth_dns_pri=$1
			nvram set eth_dns_sec=$2
		else
			nvram set eth_dns_pri=
			nvram set eth_dns_sec=
		fi
		permnvram save eth_ipaddr,eth_netmask,eth_gwaddr,eth_dns_pri,eth_dns_sec
	fi
}

writentp()
{
	nvram set ntp_serv_addr=$NTP
	permnvram save ntp_serv_addr
}

writekey()
{
	keyf="$1"
	keyfout="$2"

	if [ ! -f "$keyf" ]
	then
		echo "File '$keyf' not found !"
		return
	fi

	cat $keyf | sed "s/^SRV=.*/SRV=$KEYINST/" > $TMPKEY
	# save file if same file for input and output and if .sav doesn't exist
	[ "$keyf" = "$keyfout" -a ! -f "$keyf.sav" ] && mv $keyf $keyf.sav
	cp $TMPKEY $keyfout
}

mode="read"
while [ $# -gt 0 ]
do
	case $1 in
		--dhcp)	shift
			DHCP=$1
			mode="write"
			;;
		--address)	shift
			IP="$1"
			mode="write"
			;;
		--gw)	shift
			GW="$1"
			mode="write"
			;;
		--dns)	shift
			DNS="$1"
			mode="write"
			;;
		--ntp)	shift
			NTP="$1"
			mode="write"
			;;
		--mask)	shift
			MASK="$1"
			mode="write"
			;;
		--keyinst)	shift
			KEYINST="$1"
			mode="write"
			;;
		-i)	shift
			interfile="$1"
			;;
		-n)	shift
			ntpfile="$1"
			;;
		-k)	shift
			keyfile="$1"
			[ -z "$keyfout" ] && keyfout="$1"
			;;
		-a)	NODATE=1
			;;
		-v)	VERBOSE=1
			;;
		--interout) shift
			interfout="$1"
			;;
		--ntpout) shift
			ntpfout="$1"
			;;
		--keyout) shift
			keyfout="$1"
			;;
		-*)	use
			;;
	esac
	shift
done

if [ -z "$keyfile" ]
then
	echo "Error option mandatory"
	use
fi

if [ "$mode" = "read" ]
then
	readinter
	readntp
	readkey $keyfile
	finished
else
	# all parameters for the configuration are mandatory
	if [ "$DHCP" = "0" ]
	then
		if [ -z "$IP" ]
		then
			echo "Error: ip address mandatory"
			use
		fi
		if [ -z "$DNS" ]
		then
			echo "Error: dns address mandatory"
			use
		fi
		if [ -z "$MASK" ]
		then
			echo "Error: address mask mandatory"
			use
		fi
		if [ -z "$GW" ]
		then
			echo "Error: gateway address mandatory"
			use
		fi
		
	fi
	if [ -z "$NTP" ]
	then
		echo "Error: ntp address mandatory"
		use
	fi
	if [ -z "$KEYINST" ]
	then
		echo "Error: key-installer address mandatory"
		use
	fi

	writeinter $interfile $interfout
	writentp $ntpfile $ntpfout
	writekey $keyfile $keyfout
fi
