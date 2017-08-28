#!/bin/bash

LST=$(ls)

for d in $LST
do
	[ ! -d "$d" ] && continue

	echo "*****************  TEST$d ********************"

	echo "target.ini:"
	if [ -f "$d/target.ini" ]
	then
		cat "$d/target.ini"
	else
		echo "no file"
	fi

	echo
	echo "+ add.ini:"
	if [ -f "$d/add.ini" ]
	then
		cat "$d/add.ini"
	else
		echo "no file"
	fi

	echo
	echo "=> new_target.ini:"
	if [ -f "$d/new_target.ini" ]
	then
		cat "$d/new_target.ini"
	else
		echo "no file"
	fi
done
