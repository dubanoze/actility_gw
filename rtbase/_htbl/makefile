################################################################################
include $(ROOTACT)/rtbase/base/makefile.def

################################################################################
#CCFLAGS +=-g
#LDFLAGS +=$(THREADLD)
LIB=$(DEFLIB)
INC=-I../include

################################################################################

################################################################################
all	: h.x mh.x th.x update.x lockitself.x

%.x : %.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $< $(LIB) $(THREADLD)

%.o	: %.c ../include/_whatstr.h
	$(CC) $(CCFLAGS) $(INC) $(DEFINC) -c $<

clean	:
	@(rm *.x *.o *.a core *.log 2>/dev/null || exit 0)
