PROGNAME = repeater
SRC = repeater.c parseconfig.c json.c

OBJS = $(patsubst %.c,%.o,$(SRC))

CC = gcc
CFLAGS = -Wall -Werror -std=c99 -pedantic -I../include -lm -D_POSIX_SOURCE

build-release: $(OBJS)
	$(CC) $(CFLAGS) -o ../bin/$(PROGNAME) $(OBJS)

build-debug: CFLAGS += -g -DDEBUG
build-debug: $(OBJS)
	$(CC) $(CFLAGS) -o ../bin/$(PROGNAME) $(OBJS)

%.o: %.c ../include/%.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f ./*.o
