$ROOTACT/lrr/com/lrr.x --config 2>&1 | cut  -f 4,5,6,7,8,9,10 -d " " | grep -v sortchan
if [ -f $ROOTACT/usr/etc/lrr/DNCONFIG.log ]
then
	echo	"last specific configuration downloaded :"
	cat	$ROOTACT/usr/etc/lrr/dnconfig_last
else
	echo	"no specific configuration downloaded"
fi
exit 0
