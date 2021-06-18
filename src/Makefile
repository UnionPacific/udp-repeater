PROGNAME = repeater
SRC = repeater.c parseconfig.c json.c

OBJS = $(patsubst %.c,%.o,$(SRC))

CC = gcc
CFLAGS = -Wall -Werror -std=c99 -pedantic -I../include -D_POSIX_SOURCE

build-release: $(OBJS)
	mkdir -p ../bin
	$(CC) $(CFLAGS) -o ../bin/$(PROGNAME) $(OBJS) -lm

build-debug: CFLAGS += -g -DDEBUG
build-debug: $(OBJS)
	mkdir -p ../bin
	$(CC) $(CFLAGS) -o ../bin/$(PROGNAME) $(OBJS) -lm

%.o: %.c ../include/%.h
	$(CC) $(CFLAGS) -c $< -lm

clean:
	rm -f ./*.o
	rm -rf ../bin
