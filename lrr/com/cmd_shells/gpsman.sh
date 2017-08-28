#!/bin/sh


GPSMAN="${ROOTACT}/usr/etc/lrr/gpsman.ini"

USEGPSPOSITION="0"
LAT="0.0"
LON="0.0"
ALT="0"

if      [ $# = "0" ]
then         
	if      [ ! -f ${GPSMAN} ]  
	then                        
		echo    "no GPS location set manually"
		exit    1            
	fi                                   
	cat     ${GPSMAN}                         
	exit    $?                                
fi

case	$# in
	*)
		while	[ $# -gt 0 ]
		do
			case	$1 in
				-LAT)
					shift
					LAT="${1}"
					shift
				;;
				-LON)
					shift
					LON="${1}"
					shift
				;;
				-ALT)
					shift
					ALT="${1}"
					shift
				;;
				*)
					shift
				;;
			esac
		done
	;;
esac

echo "LAT=${LAT} LON=${LON} ALT=${ALT}"


echo	"[lrr]"						>  ${GPSMAN}
echo	"	usegpsposition=${USEGPSPOSITION}"	>> ${GPSMAN}
echo	"	lat=${LAT}"				>> ${GPSMAN}
echo	"	lon=${LON}"				>> ${GPSMAN}
echo	"	alt=${ALT}"				>> ${GPSMAN}

$ROOTACT/lrr/com/cmd_shells/restart.sh 3
exit $?
