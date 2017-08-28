#!/bin/sh

#
#	on wirmaar killing sshpass.x does not kill ssh child process
#	we kill all child processes of sshpass.x
#

KillChild()
{
	ppid="$1"
	[ -z "$ppid" ] && return
	lstpid=$(grep "^PPid:.*$ppid"  /proc/*/status | sed "s?^/proc/??" | sed "s?/.*??" 2>/dev/null)
	echo "child $lstpid"
	kill $lstpid 2> /dev/null
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
