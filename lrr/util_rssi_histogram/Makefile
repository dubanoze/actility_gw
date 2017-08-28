### Application-specific constants

APP_NAME := util_rssi_histogram

### Environment constants 

#LGW_PATH := ../../libloragw
#LGW_PATH=../../lora_gateway_spi_x1/libloragw
#CROSS_COMPILE := arm-none-linux-gnueabi-

### External constant definitions
# must get library build option to know if mpsse must be linked or not

include $(LGW_PATH)/library.cfg

### Constant symbols

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

ifeq ($(SYSTEM),fcmlb)
CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I. -D FCMLB
else ifeq ($(SYSTEM),fcpico)
CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I. -D FCPICO
else ifeq ($(SYSTEM),fclamp)
CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I. -D FCLAMP
else
CFLAGS=-O2 -Wall -Wextra -std=c99 -Iinc -I.
endif

# Since HAL v4.1.0: new lgw_connect([..]) protoype
# This provides backward compatibility
HAL_VER=$(shell head -1 $(LGW_PATH)/../VERSION | cut -b1)
ifeq ($(shell test $(HAL_VER) -ge 4; echo $$?),0)
  HAL_VER=$(shell head -1 $(LGW_PATH)/../VERSION | cut -b3)
  ifeq ($(shell test $(HAL_VER) -lt 1; echo $$?),0)
      CFLAGS+= -DOLD_LGW_CONNECT_PROTOTYPE
  endif
else
  CFLAGS+= -DOLD_LGW_CONNECT_PROTOTYPE
endif

### Constants for LoRa concentrator HAL library
# List the library sub-modules that are used by the application

LGW_INC = $(LGW_PATH)/inc/config.h
LGW_INC += $(LGW_PATH)/inc/loragw_aux.h
LGW_INC += $(LGW_PATH)/inc/loragw_reg.h

### Linking options

ifeq ($(CFG_SPI),native)
  LIBS := -lloragw -lrt
else ifeq ($(CFG_SPI),ftdi)
  LIBS := -lloragw -lrt -lmpsse
else ifeq ($(SYSTEM),fcmlb)
  LIBS := -lloragw -lrt -lm
else ifeq ($(SYSTEM),fcpico)
  LIBS := -lloragw -lrt -lm
else ifeq ($(SYSTEM),fclamp)
  LIBS := -lloragw -lrt -lm
else ifeq ($(SYSTEM),mtac_usb_v1.0)
  LIBS := -lloragw -lrt -lmpsse
else ifeq ($(SYSTEM),mtac_v1.0)
  LIBS := -lloragw -lrt
endif

### General build targets

all: $(APP_NAME)

clean:
	rm -f *.o
	rm -f $(APP_NAME)

### HAL library (do no force multiple library rebuild even with 'make -B')

$(LGW_PATH)/inc/config.h:
	@if test ! -f $@; then \
	$(MAKE) all -C $(LGW_PATH); \
	fi

$(LGW_PATH)/libloragw.a: $(LGW_INC)
	@if test ! -f $@; then \
	$(MAKE) all -C $(LGW_PATH); \
	fi

### Main program compilation and assembly

$(APP_NAME).o: src/$(APP_NAME).c $(LGW_INC)
	$(CC) -c $(CFLAGS) -I$(LGW_PATH)/inc $< -o $@

$(APP_NAME): $(APP_NAME).o $(LGW_PATH)/libloragw.a
	$(CC) -L$(LGW_PATH) $< -o $@ $(LIBS)

### EOF
