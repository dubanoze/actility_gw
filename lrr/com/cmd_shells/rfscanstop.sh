#!/bin/sh

killall rfscanv0.sh
killall rfscanv1.sh
killall util_rssi_histogram
killall util_spectral_scan

rm /tmp/rfscan.pid

exit 0
