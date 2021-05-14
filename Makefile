# -*- mode: makefile -*-

CC=gcc

# OPTFLAGS=-O2
DEBUGFLAGS=-g
LIBS=-pthread -lrt
CFLAGS= -I. $(DEBUGFLAGS) $(OPTFLAGS)

# from http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
DEPDIR := .deps

# -MT file: file is target
# -MMD: only user header files (not system)
# -MP: add a phony target for each dep into dependency file
# -MF file: write to dependencies to file
# DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d
DEPFLAGS = -MMD -MF $(DEPDIR)/$*.d

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

evtdemo: evtdemo.o evtq.o timer.o cli.o 
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

fsmdemo: fsmdemo.o evtq.o timer.o cli.o fsm.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

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
	@echo Run regression tests, speeding up timers
	./evtdemo -n
	./fsmdemo -n -t 200

# Generate markdown->html
# read: firefox README.html
README.html: README.md
	pandoc -f markdown -t html $? -o $@

clean:
	$(RM) -r $(DEPDIR)
	$(RM) *.o 
	$(RM) $(BINS)

.PHONY: clean run
