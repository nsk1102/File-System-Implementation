#
# file:        Makefile - programming assignment 3
#

HW = homework.c
CFLAGS = -g -Wall
SOFLAGS = -shared -fPIC 
ifdef COVERAGE
CFLAGS += -fprofile-arcs -ftest-coverage
LD_LIBS = --coverage
endif

# note that implicit make rules work fine for compiling x.c -> x
# (e.g. for mktest). Also, the first target defined in the file gets
# compiled if you run without an argument.
#
TOOLS = mktest read-img mkfs-hw3 libhw3.so
all: homework $(TOOLS)

# '$^' expands to all the dependencies (i.e. misc.o homework.o image.o)
# and $@ expands to 'homework' (i.e. the target)
#
libhw3.so: libhw3.c $(HW) misc.c
	gcc libhw3.c $(HW) misc.c -o libhw3.so $(CFLAGS) $(SOFLAGS) $(LD_LIBS)

misc.o: CFLAGS += -DREAL_FS

homework: misc.o $(HW:.c=.o)
	gcc -g $^ -o $@ -lfuse $(LD_LIBS)

clean: 
	rm -f *.o homework $(TOOLS) *.gcda *.gcno *.gcov
