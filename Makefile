# -*- mode: makefile -*-

CC=gcc

# OPTFLAGS =-O2
DEBUGFLAGS =-g
# -I : directories to search for includes
# -L : directories to search for libraries
# -fPIC: compile to PIC for shared library
CFLAGS =-I. -L. -fPIC $(DEBUGFLAGS) $(OPTFLAGS)

# fsm local so, system pthread
LIBS =-lfsm -pthread

# from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR := .deps

# -MT file: file is target
# -MMD: only user header files (not system)
# -MP: add a phony target for each dep into dependency file
# -MF file: write dependencies to file
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

BINS := \
	evtdemo \
	fsmdemo

# source files from which dependency files are created
SRCS := \
	evtq.c \
	timer.c \
	cli.c \
	evtdemo.c \
	fsm.c \
	fsmdemo.c

RM=rm -f

all: $(BINS)

evtdemo: evtdemo.o libfsm.so
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

fsmdemo: fsmdemo.o libfsm.so
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# create a local shared object containing common functions
libfsm.so: evtq.o timer.o cli.o fsm.o
	$(CC) -shared $^ -o $@

# recompile if .c or .d is newer OR need to run $(DEPDIR) rule
%.o: %.c $(DEPDIR)/%.d | $(DEPDIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $<

# create DEPDIR if doesn't exist
$(DEPDIR):
	@mkdir -p $@

# match all dep files from current source
DEPFILES := $(SRCS:%.c=$(DEPDIR)/%.d)

# need this for dependency targets, otherwise the default %o:%.c rule is
# invoked
$(DEPFILES):

# include the dependency rules
include $(wildcard $(DEPFILES))

run:
	@echo 'check $$LD_LOAD_LIBRARY=.:$$LD_LOAD_LIBRARY'
	@echo Run regression tests, speeding up timers
	./evtdemo -n -t 200
	./fsmdemo -n -t 100

# Generate markdown->html
# read: firefox README.html
README.html: README.md
	pandoc -f markdown -t html $? -o $@

clean:
	$(RM) -r $(DEPDIR)
	$(RM) *.o *.so
	$(RM) $(BINS)

.PHONY: clean run
