################################################################################
include $(ROOTACT)/rtbase/base/makefile.def
LIBBASE=$(ROOTACT)/rtbase/lib/librtbase.a
################################################################################

# not an option but mandatory
CFLAGS += -Wall

INC=-I../include -I../../include -I$(ROOTACT) -I$(ROOTACT)/include
ifeq ($(SYSTEM),tektelic)
	INC += -I$(SDK)/kona64/usr/include
endif

MYLDFLAGS=-L$(ROOTACT)/lib $(ROOTACT)/lib/libnewt.a $(ROOTACT)/lib/libslang.a $(LIBBASE) -lcrypto

################################################################################
EXE=suplog

SRC=\
	main.c\
	htbl_suplog.c\
	keybuild.c\
	keycrypt.c\

OBJ=\
	main.o\
	htbl_suplog.o\
	keybuild.o\
	keycrypt.o\

HDEPS=\
	_suplogver.h


################################################################################
all	: $(EXE).x

_suplogver.h: ../Version
	echo "#define SUPLOGVER \"`cat ../Version`\"" >_suplogver.h

$(EXE).x : $(OBJ)
	$(CC) $(STATIC) $(LDFLAGS) -o $@ $^ $(MYLDFLAGS)

.c.o	: $(HDEPS)
	@echo "#define SUPLOGVER \"`cat ../Version`\"" >_suplogver.h
	$(CC) $(CCFLAGS) $(INC) $(DEFINC) -c $<

clean	:
	@(rm *.x *.o *.a core *.log _suplogver.h 2>/dev/null || exit 0)
