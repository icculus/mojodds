
CC?=gcc
CFLAGS?=-Wall -g -O -std=c99
LDFLAGS?=-g

.SUFFIXES: .o

PROGRAMS:=ddsinfo

.PHONY: all clean

all: $(PROGRAMS)

clean:
	-rm $(PROGRAMS) *.o


ddsinfo: ddsinfo.o mojodds.o
	$(CC) -o $@ $^


