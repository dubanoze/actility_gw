################################################################################
include $(ROOTACT)/rtbase/base/makefile.def
include $(ROOTACT)/lrr/base/makefile.def

################################################################################

# Makefile for lrr with
#	ARCH_RADIO=SX1301_X1
#	ARCH_COMM=tty

# not an option but mandatory
# SEMTECH libloragw >= 3.2.0
CFLAGS += -Wno-unused-result -DWITH_TTY -DWITH_SX1301_X1 -DLP_MODULE_LRR -DUSELIBLGW3

SEMTECH=lora_gateway_tty_x1/libloragw
LIBSEMTECH=$(ROOTACT)/$(SEMTECH)/libloragw.a
INCSEMTECH=$(ROOTACT)/$(SEMTECH)/inc

LIB=\
	$(LIBSEMTECH)\
	$(LIBBASE)

INC=-I../include -I../../include -I$(ROOTACT) -I$(ROOTACT)/include -I$(INCSEMTECH)

MYLDFLAGS=-lm -lrt -lcrypto

ifeq ($(SYSTEM),linux-x86)
#	STATIC=-static
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
	gps.o\
	lgw_gen.o\
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
