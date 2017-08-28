################################################################################
include $(ROOTACT)/rtbase/base/makefile.def

################################################################################

# not an option but mandatory
CFLAGS += -Wall

INC=-I$(ROOTACT)/include

MYLDFLAGS=-L$(ROOTACT)/lib $(ROOTACT)/rtbase/lib/librtbase.a


################################################################################
EXE=moreini

SRC=\
	main.c\

OBJ=\
	main.o\



################################################################################
all	: $(EXE).x

$(EXE).x : $(OBJ)
	$(CC) $(STATIC) $(LDFLAGS) -o $@ $^ $(MYLDFLAGS)

.c.o	: $(HDEPS)
	$(CC) $(CCFLAGS) $(INC) $(DEFINC) -c $<

clean	:
	@(rm *.x *.o *.a core *.log 2>/dev/null || exit 0)
