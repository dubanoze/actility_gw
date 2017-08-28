#!/bin/sh

#
#	on fclamp(as on fcpico) killing sshpass.x does not kill ssh child process
#	we kill all child processes of sshpass.x
#

KillChild()
{
	LST=$(ls /proc)
	for p in $LST
	do
		case $p in
		[0-9]*)
			statp=$(cat /proc/${p}/stat 2> /dev/null)
			if [ "$statp" = "" ]
			then
				continue
			fi
			ppid=$(echo $statp | awk '{print $4}')
			if [ "$ppid" = "$1" ]
			then
				echo "child $p"
				kill $p 2> /dev/null
			fi
		;;
		esac
	done
}

KillProc()
{
	LST=$(pidof $1 2> /dev/null)
	if [ "$LST" = "" ]
	then
		return
	fi
	for pp in $LST
	do
		case $pp in
		[0-9]*)
			echo "$1 parent $pp"
			KillChild $pp
			kill $pp 2> /dev/null
			;;
		esac
	done
}

KillProc "sshpass.x"
exit 0
