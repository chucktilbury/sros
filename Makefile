###############################################################################
#
#       Tasker make file
#
###############################################################################

SYSTEM  	=	anyos
BINDIR		=	bin
TESTDIR 	=	tests

LIBOBJS 	=       task.o \
			memory.o \
			util.o \
                        event.o \
                	$(SYSTEM)/system.o 
# removed for user mode program	
#$(SYSTEM)/setjmp.o

LIBTARGET	=	$(BINDIR)/libtask.a

TSTOBJS		=	simple_test.o

TESTS		=	$(BINDIR)/simple_test \
                        $(BINDIR)/event_test 

DEBUG		=	-g
OPTIMIZE	=	-Os
WARN		=	-Wall
DEFINES 	=	-DANYOS

OPTIONS 	= 	$(DEFINES) $(DEBUG) $(WARN)
#OPTIONS 	= 	$(DEFINES) $(DEBUG) $(WARN) $(OPTIMIZE)
		

%.o:%.c
	gcc $(OPTIONS) -c $< -o $@
        
%.o:%.S
	gcc $(OPTIONS) -c $< -o $@        

all: lib $(TESTS)

lib: $(LIBOBJS)
	ar -rucsv $(LIBTARGET) $(LIBOBJS)

$(BINDIR)/simple_test: $(TESTDIR)/simple_test.c $(LIBTARGET)
	gcc $(OPTIONS) $< -o $@ $(LIBTARGET)

$(BINDIR)/event_test: $(TESTDIR)/event_test.c $(LIBTARGET)
	gcc $(OPTIONS) $< -o $@ $(LIBTARGET)

$(LIBOBJS): kern.h

clean:
	rm -f $(LIBOBJS) $(LIBTARGET) $(TSTOBJS) $(TESTS) 
