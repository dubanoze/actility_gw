#!/bin/sh

# ask a question
# $1: variable name for the result
# $2: question to ask
# $3 $4 ...: choices
ask()
{
	resvarname="$1"
	question="$2"
	shift 2
	choice="$*"

	resp=0
	while [ $resp = 0 ]
	do
		# ask question
		echo "$question"

		# display choices
		count=1
		set $choice
		tot=$#
		while [ $count -le $tot ]
		do
			echo "$count	$1"

			shift
			count=$(($count + 1))
		done

		# get response
		echo -n "> "
		read resp

		# check response
		[ $resp -gt 0 -a $resp -lt $count ] && break
		resp=0
	done

	# set resulting variable with the choice made
	set $choice
	eval "$resvarname=\$$resp"
}

checkSupportUser()
{
	if [ "$SYSTEM" = "linux-x86" ]; then
		return
	fi
	if [ "$SYSTEM" = "linux-x86_64" ]; then
		return
	fi

	# create /etc/shells if required
	if [ ! -f "/etc/shells" ]
	then
		echo "/bin/sh" >>/etc/shells
		echo "$ROOTACT/lrr/suplog/suplog.x" >>/etc/shells
	else
		# if /etc/shells exists, check if suplog.x is present
		check="$(grep "suplog.x" /etc/shells)"
		[ -z "$check" ] && echo "$ROOTACT/lrr/suplog/suplog.x" >>/etc/shells
	fi

	STDOPT="-s"
	if [ "$SYSTEM" = "oielec" ]; then
		STDOPT="--shell"
	fi
	PWD_FILE=/etc/passwd
	# create user support if required
	check="$(grep "^support:" $PWD_FILE)"
	if [ -z "$check" ]
	then
		adduser $STDOPT $ROOTACT/lrr/suplog/suplog.x support <<END_ADD
support
support
END_ADD
	else
		#check that the user support has a different ID from root
		check="$(grep "^support:x:0" $PWD_FILE)"
		if [ ! -z "$check" ]
		then
			SUPPORTID=1000
			check="$(grep $SUPPORTID $PWD_FILE)"
			while [ ! -z "$check" ]
			do
				let SUPPORTID=SUPPORTID+1
				check="$(grep $SUPPORTID $PWD_FILE)"
			done
			SUPPORTGROUP="$(id -g support)"
			sed -i "s/support:x:0:${SUPPORTGROUP}/support:x:${SUPPORTID}:${SUPPORTID}/g" $PWD_FILE
		fi

		# check the existing user support has suplog.x as shell
		check=$(grep "^support:" $PWD_FILE | grep suplog.x)
		if [ -z "$check" ]
		then
			sed -i "/^support/s#/bin/sh#${ROOTACT}/lrr/suplog/suplog.x#g" $PWD_FILE
		fi
	fi

	# check setuid bit on /bin/su
}

checkSupportUserWirmaMS()
{
	[ -f "/user/login" ] && rm -f /user/login

	[ -f "$ROOTACT/lrr/suplog/suplog.x" ] && ln -s $ROOTACT/lrr/suplog/suplog.x /user/login
}

checkSupportUserWirmaAR()
{
	rm -f /user/login

	[ -f "$ROOTACT/lrr/suplog/suplog.x" ] && ln -s $ROOTACT/lrr/suplog/suplog.x /user/login

	# command klk_set_passwd not available at 'postinst' time
	# klk_set_passwd -u support -p support
	# change password by modifying /etc/shadow (kerlink recommendation !)
	sed -i -e '/support/ s/support:.*/support:$1$T.thdwGL$TiEFih3F0zxhVF12mWVrN\/:16896:0:99999:7:::/' /user/rootfs_rw/etc/shadow
}

checkSupportUserWirmaNA()
{
	checkSupportUserWirmaAR
	cfgssh="/user/rootfs_rw/etc/ssh/sshd_config"
	res=$(grep "AllowUsers.*support" $cfgssh)
	[ -z "$res" ] && echo "AllowUsers support" >> $cfgssh

}

checkSupportUserCiscoms()
{
	# As permissions are more restrictive by default than others gateways,
	chmod -R g=rwX,o=rX $ROOTACT
	chmod 777 /tmp
	# Create group users GID 100 if needed
	check="$(grep "^users:" /etc/group)"
	if [ -z "$check" ];  then
		addgroup users -g 100
	else
		echo "group users already exists"
	fi
	# Create user support UID 1001 if needed
	check="$(grep "^support:" /etc/passwd)"
	if [ -z "$check" ];  then
		adduser support -u 1001 -G users -g "Support LRR" -s $ROOTACT/lrr/suplog/suplog.x << END_ADD
support
support
END_ADD
	else
		echo "user support already exists"
	fi
	# Add umask in profile if needed
	line=$(grep "umask 002" $INIT_PROF)
	if [ -z "$line" ]; then
		echo "umask 002" >> $INIT_PROF
	else
		echo "umask already present in $INIT_PROF"
	fi

	# Because permissions where not already available
	chown root:root $ROOTACT/lrr/suplog/suplog.x
	chmod u+s $ROOTACT/lrr/suplog/suplog.x
}

checkSupportUserGemtek()
{
        # create /etc/shells if required
        if [ ! -f "/etc/shells" ]
        then
                echo "/bin/sh" >>/etc/shells
                echo "$ROOTACT/lrr/suplog/suplog.x" >>/etc/shells
        else
                # if /etc/shells exists, check if suplog.x is present
                check="$(grep "suplog.x" /etc/shells)"
                [ -z "$check" ] && echo "$ROOTACT/lrr/suplog/suplog.x" >>/etc/shells
        fi

        STDOPT="-shell"
        PWD_FILE=/etc/passwd
        # create user support if required
        check="$(grep "^support:" $PWD_FILE)"
        if [ -z "$check" ]
        then
                adduser $STDOPT $ROOTACT/lrr/suplog/suplog.x support <<END_ADD
support
support




y
END_ADD
        else
                #check that the user support has a different ID from root
                check="$(grep "^support:x:0" $PWD_FILE)"
                if [ ! -z "$check" ]
                then
                        SUPPORTID=1000
                        check="$(grep $SUPPORTID $PWD_FILE)"
                        while [ ! -z "$check" ]
                        do
                                let SUPPORTID=SUPPORTID+1
                                check="$(grep $SUPPORTID $PWD_FILE)"
                        done
                        SUPPORTGROUP="$(id -g support)"
                        sed -i "s/support:x:0:${SUPPORTGROUP}/support:x:${SUPPORTID}:${SUPPORTID}/g" $PWD_FILE
                fi

                # check the existing user support has suplog.x as shell
                check=$(grep "^support:" $PWD_FILE | grep suplog.x)
                if [ -z "$check" ]
                then
                        sed -i "/^support/s#/bin/sh#${ROOTACT}/lrr/suplog/suplog.x#g" $PWD_FILE
                fi
        fi

        # check setuid bit on /bin/su
}

addIptables_wirmav2()
{
	type iptables > /dev/null 2>&1
	if [ $? != "0" ]
	then
		echo	"iptables are not available"
		return 1
	fi
	if [ ! -f /etc/init.d/firewall.kerlink -a -f /etc/init.d/firewall ]
	then
		cp /etc/init.d/firewall /etc/init.d/firewall.kerlink
	fi

	cp $ROOTACT/lrr/com/shells/wirmav2/firewall /etc/init.d/firewall
	chmod +x /etc/init.d/firewall
	rm -f /etc/rc.d/rc3.d/S80firewall
	ln -s /etc/init.d/firewall /etc/rc.d/rc3.d/S80firewall

	echo "SERVICEFIREWALL=/etc/init.d/firewall" >> $ROOTACT/usr/etc/lrr/_parameters.sh

	echo	"iptables are installed"
	return 0
}

addIptables_wirmams()
{
	cat $ROOTACT/lrr/com/shells/wirmams/firewall | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /user/rootfs_rw/etc/rcU.d/S89lrrfirewall
	chmod +x /user/rootfs_rw/etc/rcU.d/S89lrrfirewall

	echo "SERVICEFIREWALL=/etc/rcU.d/S89lrrfirewall" >> $ROOTACT/usr/etc/lrr/_parameters.sh

	echo	"iptables are installed"
	return 0
}

addIptables_wirmaar()
{
	addIptables_wirmams
}

addIptables_wirmana()
{
	addIptables_wirmams
}

useBashInteadOfDash(){
	echo "dash dash/sh boolean false" | debconf-set-selections
	dpkg-reconfigure -f noninteractive dash
}

use()
{
	echo "$0 [-m usb|spi] [-x x1|x8]"
	echo "Incorrect command, default values used !"
}

#
# Main
#

# get parameters
while [ $# -gt 0 ] 
do 
	case $1 in
                -m)
                        SERIALMODE="$2"
			if [ "$SERIALMODE" != "usb" -a "$SERIALMODE" != "spi" ]
			then
				use
				SERIALMODE="spi"
			fi
                        shift
                ;;
                -x)
                        BOARDTYPE="$2"
			if [ "$BOARDTYPE" != "x1" -a "$BOARDTYPE" != "x8" ]
			then
				use
				BOARDTYPE="x1"
			fi
                        shift
                ;;
        esac
        shift
done

if	[ -z "$ROOTACT" ]
then
	if	[ -d /mnt/fsuser-1/actility ]
	then
		export ROOTACT=/mnt/fsuser-1/actility
	else
		case $(uname -n) in
			klk-lpbs*)
				export ROOTACT=/user/actility
				;;
			klk-wifc*)
				export ROOTACT=/user/actility
				;;
			*)
				export ROOTACT=/home/actility
				;;
		esac
	fi
fi

if	[ ! -z "$(uname -a | grep -i lora-modem)" ]
then
	echo	"Cisco lora-modem detected (ciscoms)"
	echo	"WARNING: you are trying to install lrr directly on the station !"
	echo	"On this device, the lrr must be installed in a lxc container"
	echo	"Run 'lxc-info -n lab' to find the ip address of the container,"
	echo	"then connect to this address and proceed to lrr installation in the container"
	echo	"Installation aborted."
	exit 0
fi

if	[ ! -d "$ROOTACT" ]
then
	echo	"$ROOTACT does not exist"
	exit	0
fi

if [ -f $ROOTACT/lrr/suplog/suplog.x ]
then
chown root:root $ROOTACT/lrr/suplog/suplog.x
chmod u+s $ROOTACT/lrr/suplog/suplog.x
fi

mkdir -p $ROOTACT/usr/etc/lrr > /dev/null 2>&1

echo	"$ROOTACT used as root directory"

if [ -f ${ROOTACT}/lrr/com/_parameters.sh ]
then
	. ${ROOTACT}/lrr/com/_parameters.sh
fi
if [ -f ${ROOTACT}/lrr/com/_functions.sh ]
then
	. ${ROOTACT}/lrr/com/_functions.sh
fi

if	[ -z "$SYSTEM" ]
then
	if	[ ! -z "$(uname -a | grep -i Wirgrid)" ]
	then
		echo	"System : kerlink/wirmav2"
		SYSTEM=wirmav2
	fi
	if	[ ! -z "$(uname -a | grep -i raspberrypi)" ]
	then
		#If this is a SPI model then there will be  /dev/spidev0.0 and /dev/spidev0.1 devices
		if	[ ! -z "$(ls /dev/spidev*)" ]
		then
			echo	"System : rbpi_v1.0"
			SYSTEM=rbpi_v1.0
		else
			echo	"System : natrbpi_usb_v1.0"
			SYSTEM=natrbpi_usb_v1.0
		fi
	fi
	if	[ ! -z "$(uname -a | grep -i Router)" ]
	then
		echo	"System : ir910"
		SYSTEM=ir910
	fi
	if	[ ! -z "$(uname -a | grep -i am335x)" ]
	then
#		As we can not distinguish fcmlb, fcloc, fcpico and fclamp with uname methode,
#		we can use hw_version value from nvram, with defaults should be
#			For Macro GW 1.0/1.5	: BST100_Rev1.4
#			For Macro GW 2.1	: BST200_Rev1.0
#			For Pico GW 1.5		: PCG020C-V2
#			For Orange Lamp		: LGW010E-V0 or F04I0xx
#			Rem: Orange Lamp is seen as a Pico GW for now
		if [ ! -z "$(nvram get hw_version | grep -i BST200)" ]; then
			echo    "System : fcloc"
			SYSTEM=fcloc
		elif [ ! -z "$(nvram get hw_version | grep -e PCG020)" ]; then
			echo	"System : fcpico"
			SYSTEM=fcpico
		elif [ ! -z "$(nvram get hw_version | grep -e LGW010 -e F04I0)" ]; then
			echo    "System : fclamp"
			SYSTEM=fclamp
		else
			echo    "System : fcmlb"
			SYSTEM=fcmlb
		fi
	fi
	#Multitech Conduit (MTCDT)
	if	[ ! -z "$(uname -a | grep -i mtcdt)" ]
		then
		#Set system to mtac in case the mts-io-sysfs ioctl fails
		SYSTEM=mtac
		if [ ! -z "$(mts-io-sysfs show lora/hw-version | grep -i MTAC-LORA-1.5)" ]
		then
			# LoRa V1.5, must be SPI, Refresh or not? Refresh GWs have GPS capability
			if [ ! -z "$(mts-io-sysfs show capability/gps | grep -i 1)" ]
			then
				SYSTEM=mtac_refresh_v1.5
			else
				SYSTEM=mtac_v1.5
			fi
		else
			#Not a V1.5 so it may be 1.0, check LoRa version just to be sure
			if [ ! -z "$(mts-io-sysfs show lora/product-id | grep -i 'H\|SPI')" ] && [ ! -z "$(mts-io-sysfs show lora/hw-version | grep -i MTAC-LORA-1.0)" ]
			then
				echo "system mtac_v1.0"
				SYSTEM=mtac_v1.0
			else
				echo "system mtac_usb_v1.0"
				SYSTEM=mtac_usb_v1.0
			fi
		fi
	fi
	#Multitech Pico (MTCAP)
	if	[ ! -z "$(uname -a | grep -i mtcap)" ]
	then
		echo	"System : mtcap"
		SYSTEM=mtcap
	fi
	if	[ ! -z "$(uname -a | grep -i klk-lpbs)" ]
	then
#		Nothing found to distinguish wirmams and wirmaar at boot time
#		echo	"System : kerlink/wirmams"
#		SYSTEM=wirmams
		echo	"System : kerlink/wirmaar"
		SYSTEM=wirmaar
	fi
	if	[ ! -z "$(uname -a | grep -i klk-wifc)" ]
	then
		echo	"System : kerlink/wirmana"
		SYSTEM=wirmana
	fi
	if	[ ! -z "$(uname -a | grep -i 'Linux lab')" ]
	then
		echo	"System : ciscoms"
		SYSTEM=ciscoms
	fi
	if	[ ! -z "$(uname -a | grep -i 'Kona')" ]
	then
		echo	"System : tektelic"
		SYSTEM=tektelic
	fi
	if	[ ! -z "$(uname -a | grep -i 'Linux debian-armhf')" ]
	then
		echo	"System : oielec"
		SYSTEM=oielec
	fi
	if      [ ! -z "$(uname -a | grep -i 'Linux OutdoorAP')" ]
	then
	        echo    "System : gemtek"
	        SYSTEM=gemtek
	fi
fi

if	[ -z "$SYSTEM" ]
then
	echo	"cannot find host system"
	exit	0
fi

echo	"# Generated lrr configuration file ($(date))" > $ROOTACT/usr/etc/lrr/_parameters.sh
echo	"SYSTEM=${SYSTEM}" >> $ROOTACT/usr/etc/lrr/_parameters.sh

echo	"System : $SYSTEM"
export SYSTEM

date > $ROOTACT/usr/etc/lrr/sysconfig_done

#wirgrid=$(uname -a | grep -i Wirgrid)
#if	[ -z "$wirgrid" ]
#then
#	echo	"System is not a kerlink/wirma/wirgrid : no configuration"
#	exit	0
#fi

#
# add following lines in /etc/profile if not present
#
if [ "$SYSTEM" != "wirmams" -a "$SYSTEM" != "wirmaar" -a "$SYSTEM" != "wirmana"  -a "$SYSTEM" != "linux-x86" -a "$SYSTEM" != "linux-x86_64" ]
then
	PROF_LINE="export ROOTACT=${ROOTACT}"
	PATH_LINE="export PATH=\$PATH:\$ROOTACT/lrr/com"
	ALIA_LINE="alias cdr='cd \$ROOTACT'"
	line=$(grep "$PROF_LINE" $INIT_PROF)
	if	[ -z "$line" ]
	then
		echo	"ROOTACT not present in $INIT_PROF : add it"
		echo	>> $INIT_PROF
		echo	$PROF_LINE >> $INIT_PROF
		echo	$PATH_LINE >> $INIT_PROF
		echo	$ALIA_LINE >> $INIT_PROF
		echo "alias l='ls --color=auto'" >> $INIT_PROF
		echo "alias ll='ls -l --color=auto'" >> $INIT_PROF
	else
		echo	"ROOTACT already present in $INIT_PROF"
	fi
else
	echo	"Do not change /etc/profile on target $SYSTEM"
fi

# force INIT_FILE because it will be removed from configuration file
# and it has always been /etc/profile
INIT_FILE=/etc/inittab
if	[ -f $INIT_FILE ]
then
#
# remove following line from /etc/inittab if present
#
#"lr:X:respawn:$ROOTACT/lrr/com/initlrr.sh -s inittab"

	line=$(grep "initlrr.sh" $INIT_FILE)
	if	[ ! -z "$line" ]
	then
		echo	"remove initlrr.sh line from $INIT_FILE"
		cp $INIT_FILE /tmp
		sed '/initlrr.sh/d' /tmp/$(basename $INIT_FILE) > $INIT_FILE
	fi
fi

case $SYSTEM in
	wirmav2)	#Kerlink
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc.d/rc3.d/S90lrr" ]
		then
			# default runlevel is 3
			# according to kerlink wiki, lvl 2 and 4 are reserved for kerlink stuff
			ln -s /etc/init.d/lrr /etc/rc.d/rc3.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc.d/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc.d/rc6.d/K01lrr
		fi
		checkSupportUser
		addIptables_wirmav2
	;;
	wirmams)	#Kerlink
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=ms" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/rcU.d/S90lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh

		# need to use /user/rootfs_rw/etc/rcU.d/S90lrr instead of /etc/rcU.d/S90lrr
		# because at ipk installation time /etc/rcU.d does not exist yet
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /user/rootfs_rw/etc/rcU.d/S90lrr
		chmod +x /user/rootfs_rw/etc/rcU.d/S90lrr
		checkSupportUserWirmaMS
		addIptables_wirmams
	;;
	wirmaar)	#Kerlink
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x8" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/rcU.d/S90lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		# need to use /user/rootfs_rw/etc/rcU.d/S90lrr instead of /etc/rcU.d/S90lrr
		# because at ipk installation time /etc/rcU.d does not exist yet
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /user/rootfs_rw/etc/rcU.d/S90lrr
		chmod +x /user/rootfs_rw/etc/rcU.d/S90lrr
		checkSupportUserWirmaAR
		addIptables_wirmaar
	;;
	wirmana)	#Kerlink
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/rcU.d/S90lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		# need to use /user/rootfs_rw/etc/rcU.d/S90lrr instead of /etc/rcU.d/S90lrr
		# because at ipk installation time /etc/rcU.d does not exist yet
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /user/rootfs_rw/etc/rcU.d/S90lrr
		chmod +x /user/rootfs_rw/etc/rcU.d/S90lrr
		checkSupportUserWirmaNA
		addIptables_wirmana
	;;
	rbpi_v1.0)	#RaspBerry SPI V1.0
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S90lrr" ]
		then
			ln -s /etc/init.d/lrr /etc/rc2.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc3.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc4.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc5.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
		fi
	;;
	natrbpi_usb_v1.0)	#RaspBerry USB V1.0
		echo "SERIALMODE=usb" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S90lrr" ]
		then
			ln -s /etc/init.d/lrr /etc/rc2.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc3.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc4.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc5.d/S90lrr
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
		fi
	;;
	ir910)		#Cisco
		echo "SERIALMODE=usb" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/mnt/apps/etc/init.d/S06initlrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /mnt/apps/etc/init.d/S06initlrr
		chmod +x /mnt/apps/etc/init.d/S06initlrr
	;;

	ciscoms)	#Cisco lora-modem multislots
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x8" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/S55initlrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/S55initlrr
                cat $ROOTACT/lrr/com/cmd_shells/ciscoms/hostip_service.sh > /etc/init.d/S56hostip
                chmod +x /etc/init.d/S56hostip
                cat $ROOTACT/lrr/com/cmd_shells/ciscoms/iptables.sh > /etc/init.d/S57iptables
                chmod +x /etc/init.d/S57iptables
		chmod +x /etc/init.d/S55initlrr
		if [ ! -f "$ROOTACT/usr/etc/lrr/credentials.txt" ]
		then
			echo "secret" >> $ROOTACT/usr/etc/lrr/credentials.txt
			echo "username" >> $ROOTACT/usr/etc/lrr/credentials.txt
			echo "password" >> $ROOTACT/usr/etc/lrr/credentials.txt
		fi
		if [ ! -f "/etc/hosts" ]
                then
			touch /etc/hosts
		fi
		if [ ! -f "/etc/nsswitch.conf" ]
		then
			cat $ROOTACT/lrr/com/cmd_shells/ciscoms/nsswitch.conf > /etc/nsswitch.conf
		fi
		rm -f /tmp/log
		mkdir /tmp/log
		mkdir /tmp/log/_LRRLOG
		checkSupportUserCiscoms
	;;

	tektelic)	#Tektelic
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x8" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S99lrr" ]
		then
			# remove '2&>1', the command do nothing with that !
			# ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr > /dev/null 2&>1
			ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr > /dev/null
			ln -s /etc/init.d/lrr /etc/rc3.d/S99lrr > /dev/null
			ln -s /etc/init.d/lrr /etc/rc4.d/S99lrr > /dev/null
			ln -s /etc/init.d/lrr /etc/rc5.d/S99lrr > /dev/null
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr > /dev/null
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr > /dev/null
		fi
		checkSupportUser
	;;

	fcmlb)	#Foxconn MLB
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S99lrr" ]
		then
			ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc3.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc4.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc5.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
		fi
		checkSupportUser
	;;

	fcpico|fclamp)	#Foxconn Pico GW 1.5 or Orange lamp GW 1.0
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S99lrr" ]
		then
			ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc3.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc4.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc5.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
		fi
		checkSupportUser
	;;

	fcloc)	#Foxconn Macro GW 2.1
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x8" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S99lrr" ]
		then
			ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc3.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc4.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc5.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
		fi
		checkSupportUser
	;;

	mtac_v1.0|mtac_v1.5|mtac_refresh_v1.5|mtcap|mtac)		#Multitech MTAC V1.0 SPI or V1.5 SPI + MTCAP V1.5 SPI / mtac is a fallback mechanism in case of failure of LoRa MCARD detection
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S99lrr" ]
		then
			ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc3.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc4.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc5.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
		fi
	;;
	mtac_usb_v1.0)		#Multitech V1.0 USB
		echo "SERIALMODE=usb" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		if [ ! -f "/etc/rc2.d/S99lrr" ]
		then
			ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc3.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc4.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc5.d/S99lrr
			ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
			ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
		fi
	;;
	oielec)	#OIelec
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		update-rc.d lrr defaults
		checkSupportUser
	;;
	gemtek) #Gemtek
		echo "SERIALMODE=spi" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
		chmod +x /etc/init.d/lrr
		update-rc.d lrr defaults
		checkSupportUserGemtek
		useBashInteadOfDash
	;;
	linux-x86|linux-x86_64) #linux 32.64 bits generic with semtech pico via tty
		echo "SERIALMODE=tty" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		LRROUI=$($ROOTACT/lrr/com/shells/getid.sh -o)
		LRRGID=$($ROOTACT/lrr/com/shells/getid.sh -u)
		echo "LRROUI=$LRROUI" >> $ROOTACT/usr/etc/lrr/_parameters.sh
		echo "LRRGID=$LRRGID" >> $ROOTACT/usr/etc/lrr/_parameters.sh
#		cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
#		chmod +x /etc/init.d/lrr
#		update-rc.d lrr defaults

		echo "Do not create user support on target $SYSTEM"
		echo "Do not create service LRR on targe $SYSTEM"
	;;
        rdkxxx)         #Technicolor XB6
                echo "SERIALMODE=tty" >> $ROOTACT/usr/etc/lrr/_parameters.sh
                echo "BOARDTYPE=x1" >> $ROOTACT/usr/etc/lrr/_parameters.sh
                echo "SERVICELRR=/etc/init.d/lrr" >> $ROOTACT/usr/etc/lrr/_parameters.sh
                cat $ROOTACT/lrr/com/lrrservice.sh | sed "s?_REPLACEWITHROOTACT_?$ROOTACT?" > /etc/init.d/lrr
                chmod +x /etc/init.d/lrr
               # if [ ! -f "/etc/rc2.d/S99lrr" ]
               # then
               #         ln -s /etc/init.d/lrr /etc/rc2.d/S99lrr
               #         ln -s /etc/init.d/lrr /etc/rc3.d/S99lrr
               #         ln -s /etc/init.d/lrr /etc/rc4.d/S99lrr
               #         ln -s /etc/init.d/lrr /etc/rc5.d/S99lrr
               #         ln -s /etc/init.d/lrr /etc/rc0.d/K01lrr
               #         ln -s /etc/init.d/lrr /etc/rc6.d/K01lrr
               # fi
        ;;


esac

echo	"System configuration done"

exit	0
