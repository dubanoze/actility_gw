
################################################################################
include $(ROOTACT)/rtbase/base/makefile.def

################################################################################
LIB=$(DEFLIB)
INC=-I../include

################################################################################

################################################################################
all	: srv.x cli.x seccli.x bothsrvcli.x msrv.x

%.x : %.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $< $(LIB) $(THREADLD) -lrt

%.o	: %.c ../include/_whatstr.h
	$(CC) $(CCFLAGS) $(INC) $(DEFINC) -c $<

clean	:
	@(rm *.x *.o *.a core *.log 2>/dev/null || exit 0)
