################################################################################
include $(ROOTACT)/rtbase/base/makefile.def
include $(ROOTACT)/lrr/base/makefile.def

################################################################################

# Makefile for lrr with
#	ARCH_RADIO=SX1301_X1
#	ARCH_COMM=spi

# not an option but mandatory
# SEMTECH libloragw >= 3.2.0
CFLAGS += -DWITH_SPI -DWITH_SX1301_X1 -DLP_MODULE_LRR -DUSELIBLGW3

SEMTECH=lora_gateway_spi_x1/libloragw
LIBSEMTECH=$(ROOTACT)/$(SEMTECH)/libloragw.a
INCSEMTECH=$(ROOTACT)/$(SEMTECH)/inc

LIB=\
	$(LIBSEMTECH)\
	$(LIBBASE)

INC=-I../include -I../../include -I$(ROOTACT) -I$(ROOTACT)/include -I$(INCSEMTECH)

MYLDFLAGS=-lm -lrt -lcrypto

ifeq ($(SYSTEM),wirmav2)
	CFLAGS += -DWITH_GPS
endif

ifeq ($(SYSTEM),fcmlb)
	CFLAGS += -DWITH_GPS
endif

ifeq ($(SYSTEM),gemtek)
	CFLAGS += -DWITH_GPS
endif

################################################################################
EXELRR=lrr

SRC=\
	../headerloramac.c\
	../crc-ccitt.c\
	../inivar.c\
	../leds.c\
	../gps.c\
	../lgw_gen.c\
	../avdv.c\
	../netitf.c\
	../rttping.c\
	../shellcmd.c\
	../semtech.c\
	../keycrypt.c\
	../keybuild.c\
	../dutycycle.c\
	../main.c\
	../lgw_x1.c

OBJ=\
	headerloramac.o\
	crc-ccitt.o\
	inivar.o\
	leds.o\
	lgw_gen.o\
	gps.o\
	avdv.o\
	netitf.o\
	rttping.o\
	shellcmd.o\
	semtech.o\
	keycrypt.o\
	keybuild.o\
	dutycycle.o\
	main.o\
	lgw_x1.o

HDEPS=\
	../../include/_whatstr.h\
	../spi_linuxdev.h\
	../xlap.h\
	../define.h\
	../struct.h\
	../infrastruct.h


################################################################################
all	: $(EXELRR).x

$(EXELRR).x : $(OBJ) $(LIB)
	$(CC) $(STATIC) $(LDFLAGS) -s -o $@ $^ $(MYLDFLAGS) $(THREADLD)

%.o	: ../%.c $(HDEPS)
	$(CC) $(CCFLAGS) $(INC) $(DEFINC) -c $<

clean	:
	@(rm _cproto.h *.x *.o *.a core *.log 2>/dev/null || exit 0)
