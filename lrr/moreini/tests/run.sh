#!/bin/bash

OUTFILE="/tmp/testmoreini_out.ini"

if [ ! -z "$1" ]
then
	LST="$1"
else
	LST=$(ls)
fi

if [ ! -f "../moreini.x" ]
then
	echo "moreini.x doesn't exist !"
	exit 1
fi

for d in $LST
do
	[ ! -d "$d" ] && continue

	echo -n "Running TEST$d ... "
	rm -f "$OUTFILE"
	../moreini.x -t "$d/target.ini" -a "$d/add.ini" -o "$OUTFILE"
	grep -v ";" $OUTFILE > $OUTFILE.clean

	diff "$d/new_target.ini" $OUTFILE.clean
	if [ $? = 0 ]
	then
		echo "OK"
	else
		echo "ERROR"
	fi
done
