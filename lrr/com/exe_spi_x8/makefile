################################################################################
include $(ROOTACT)/rtbase/base/makefile.def
include $(ROOTACT)/lrr/base/makefile.def

################################################################################

# Makefile for lrr with
#	ARCH_RADIO=SX1301_X8
#	ARCH_COMM=spi

# not an option but mandatory
CFLAGS += -DWITH_SPI -DWITH_SX1301_X8 -DLP_MODULE_LRR

SEMTECH=lora_gateway/libsx1301ar
LIBSEMTECH=$(ROOTACT)/$(SEMTECH)/libsx1301ar.a
INCSEMTECH=$(ROOTACT)/$(SEMTECH)/inc

LIB=\
	$(LIBSEMTECH)\
	$(LIBBASE)

INC=-I../include -I../../include -I$(ROOTACT) -I$(ROOTACT)/include -I$(INCSEMTECH)

MYLDFLAGS=-lm -lrt -lcrypto

ifeq ($(SYSTEM),wirmaar)
	CFLAGS += -DSPI_ATMEL -DWITH_GPS
endif

ifeq ($(SYSTEM),ciscoms)
	CFLAGS += -DWITH_GPS
endif

ifeq ($(SYSTEM),fcloc)
	CFLAGS += -DWITH_GPS
endif

ifeq ($(SYSTEM),tektelic)
	CFLAGS += -DWITH_GPS
	INC += -I$(SDK)/kona64/usr/include
endif

### Build mode can be either 'debug' or 'release'. Default is 'release'
BUILD_MODE ?=release
ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g -O0
endif


################################################################################
EXELRR=lrr

SRC=\
	../headerloramac.c\
	../crc-ccitt.c\
	../inivar.c\
	../lgw_gen.c\
	../gps.c\
	../avdv.c\
	../netitf.c\
	../rttping.c\
	../shellcmd.c\
	../keycrypt.c\
	../keybuild.c\
	../dutycycle.c\
	../main.c\
	../spi_linuxdev_ar.c

ifeq ($(SYSTEM),tektelic)
	SRC += ../lgw_x8_tektelic.c
else
	SRC += ../lgw_x8.c
endif

OBJ=\
	headerloramac.o\
	crc-ccitt.o\
	inivar.o\
	lgw_gen.o\
	gps.o\
	avdv.o\
	netitf.o\
	rttping.o\
	shellcmd.o\
	keycrypt.o\
	keybuild.o\
	dutycycle.o\
	main.o\
	spi_linuxdev_ar.o

ifeq ($(SYSTEM),tektelic)
	OBJ += lgw_x8_tektelic.o
else
	OBJ += lgw_x8.o
endif


HDEPS=\
	../../include/_whatstr.h\
	../spi_linuxdev_ar.h\
	../xlap.h\
	../define.h\
	../struct.h\
	../infrastruct.h


################################################################################
all	: $(EXELRR).x

$(EXELRR).x : $(OBJ) $(LIB)
	$(CC) $(STATIC) $(LDFLAGS) -o $@ $^ $(MYLDFLAGS) $(THREADLD)

%.o	: ../%.c $(HDEPS)
	$(CC) $(CCFLAGS) $(INC) $(DEFINC) -c $<

clean	:
	@(rm _cproto.h *.x *.o *.a core *.log 2>/dev/null || exit 0)
