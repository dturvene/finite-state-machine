# -*- mode: makefile -*-

CC=gcc
# OPTFLAGS=-O2
# DEBUGFLAGS=-save-temps -g
DEBUGFLAGS=-g
LIBS=-pthread -lrt
CFLAGS= $(DEBUGFLAGS) $(OPTFLAGS) $(LIBS)


BINS := \
	evtdemo

RM=rm -f

all: $(BINS)

# 
evtdemo: evtdemo.o
	$(CC) $(CFLAGS) $? -o $@ $(LIBS) 

%.o: %.c
	$(CC) $(CFLAGS) -c $<

run_reg:
	@echo Run regression test
	./evtdemo -n

# Generate markdown->html
# read: firefox README.html
README.html: README.md
	pandoc -f markdown -t html $? -o $@

clean: 
	$(RM) *.o 
	$(RM) $(BINS)

.PHONY: clean run_reg
