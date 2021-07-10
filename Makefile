
CC?=gcc
CFLAGS?=-Wall -g -O -std=c99
LDFLAGS?=-g

CFLAGS+=$(shell sdl2-config --cflags)
CFLAGS+=$(shell pkg-config glew --cflags)

LDLIBS+=$(shell sdl2-config --libs)
LDLIBS+=$(shell pkg-config glew --libs)


.SUFFIXES: .o

PROGRAMS:=ddsinfo glddstest afl-mojodds

.PHONY: all clean

all: $(PROGRAMS)

clean:
	-rm $(PROGRAMS) *.o


ddsinfo: ddsinfo.o mojodds.o
	$(CC) $(LDFLAGS) -o $@ $^


glddstest: glddstest.o mojodds.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)


afl-mojodds: afl-mojodds.o mojodds.o
	$(CC) $(LDFLAGS) -o $@ $^
