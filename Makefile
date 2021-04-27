# -*- mode: makefile -*-

CC=gcc
# OPTFLAGS=-O2
DEBUGFLAGS=-g
LIBS=-pthread -lrt
CFLAGS= -I. $(DEBUGFLAGS) $(OPTFLAGS)

BINS := \
	evtdemo

RM=rm -f

all: $(BINS)

# 
evtdemo: evtdemo.o evtq.o timer.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

evtdemo.o: evtdemo.c workers.h utils.h
	$(CC) $(CFLAGS) -c $<

evtq.o: evtq.c evtq.h utils.h
	$(CC) $(CFLAGS) -c $<

timer.o: timer.c timer.h utils.h
	$(CC) $(CFLAGS) -c $<

fsmdemo: fsmdemo.o evtq.o timer.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

run:
	@echo Run regression test
	./evtdemo -n

# Generate markdown->html
# read: firefox README.html
README.html: README.md
	pandoc -f markdown -t html $? -o $@

clean: 
	$(RM) *.o 
	$(RM) $(BINS)

.PHONY: clean run
