#!/bin/sh
#-----------------------------------------------------------------------------
#
#	mk-halversion.sh
#
#	Construction d'une what-string de la version HAL utilisée
#	pour compiler le LRR
#
#-----------------------------------------------------------------------------

if [ -f $ROOTACT/lora_gateway/VERSION ] ; then
	hal_version=$(head -c -1 $ROOTACT/lora_gateway/VERSION)
elif [ -f $ROOTACT/lora_gateway_spi_x1/VERSION ] ; then
	hal_version=$(head -c -1 $ROOTACT/lora_gateway_spi_x1/VERSION)
else
	hal_version="N/C"
fi

hal_version=$(echo $hal_version | tr -d '\r\n')
echo "#ifndef _HALV_COMPAT"
echo "#define _HALV_COMPAT"
echo "	static char * hal_version=\"${hal_version}\";"
echo "#endif"



