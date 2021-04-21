# -*- mode: makefile -*-

CC=gcc
CXX=g++
# DEBUGFLAGS=-save-temps -g
# OPTFLAGS=-O2
DEBUGFLAGS=-g
CFLAGS= $(DEBUGFLAGS)
LIBS=-pthread -lrt

BINS := \
	evtdemo

RM=rm -f

all: $(BINS)

evtdemo: evtdemo.o
	$(CC) $(CFLAGS) $(LIBS) $? -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

run_reg:
	@echo Run regression test

clean: 
	$(RM) *.o *.lst *.i
	$(RM) $(BINS)

.PHONY: clean run_reg
