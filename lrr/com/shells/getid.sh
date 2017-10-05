#!/bin/sh
#
#	get gateway uid
#	This shell must return the gateway uniq identifier
#	The string return must be alphanum only
#

while	[ $# -gt 0 ]
do
	case	$1 in 
		-o)
			shift
			MODE="oui"
		;;
		-u)
			shift
			MODE="uid"
		;;
		*)
			shift
		;;
	esac
done

oui='uniden'
uid='unidentified'
case	$SYSTEM in
	natrbpi)
	;;
	fcloc)
		oui='001558'
		uid=$(ifconfig eth0 | grep HWaddr | awk '{ print $5 }' | sed 's/://g' | tr '[:upper:]' '[:lower:]')
	;;
	fcpico)
		oui='001558'
		uid=$(nvram get bs_id)
	;;
	ciscoms)
		oui='005f86'
	;;
	wirma*)
		oui='7076ff'
		uid=$(ifconfig eth0 | grep HWaddr | awk '{ print $5 }' | sed 's/://g' | tr '[:upper:]' '[:lower:]')
	;;
	mtac*|mtcap)
		oui='000800'
		uid=$(mts-io-sysfs show device-id)
	;;
	linux*)
		oui='0016C0'
		uid=$($ROOTACT/lrr/com/lrr.x --stpicoid)
esac

# New requirement: results must now be in uppercase
oui=$(echo $oui | tr '[:lower:]' '[:upper:]')
uid=$(echo $uid | tr '[:lower:]' '[:upper:]')

# Verbose off when used from sysconfiglrr.sh to set LRRUID in _parameters.sh
if [ "$MODE" = "oui" ]
then
	echo "$oui"
elif [ "$MODE" = "uid" ]
then
	echo "$uid"
else
	echo "Vendor oui:'$oui' Gateway uid:'$uid'"
fi
