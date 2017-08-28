killall lrr.x
rm TRACE*.log core core.* > /dev/null 2>&1

./lrr.x &

sleep 5
tail -f ./TRACE.log
