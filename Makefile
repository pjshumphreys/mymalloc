
## Created by Anjuta

CC = gcc
CFLAGS = -g -Wall
SOURCES = mymalloc.c
OBJECTS = $(SOURCES:%.c=%.o)
INCFLAGS = 
LDFLAGS = -Wl,-rpath,/usr/local/lib
LIBS =

all: mymalloc

mymalloc: $(OBJECTS)
	$(CC) -o mymalloc $(OBJECTS) $(LDFLAGS) $(LIBS)

.SUFFIXES:
.SUFFIXES: .c .cc .C .cpp .o

.c.o :
	$(CC) -o $@ -c $(CFLAGS) $< $(INCFLAGS)

count:
	wc *.c *.cc *.C *.cpp *.h *.hpp

clean:
	rm -f $(OBJECTS) mymalloc

.PHONY: all
.PHONY: count
.PHONY: clean
