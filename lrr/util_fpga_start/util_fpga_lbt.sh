#!/bin/sh

case	"$1"	in
	900)
	${ROOTACT}/lrr/util_fpga_start/util_fpga_lbt_${1} -l
	;;
	800)
	${ROOTACT}/lrr/util_fpga_start/util_fpga_lbt_${1} -l
	;;
	*)
	;;
esac
exit $?
