CC = gcc
CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS = -Wall -Wextra -Werror -std=c99 -Wvla
LDFLAGS = -shared
VPATH = src

TARGET_LIB = libmalloc.so
OBJS = malloc.o allocator.o convert.o

all: library

library: $(TARGET_LIB)
$(TARGET_LIB): CFLAGS += -pedantic -fvisibility=hidden -fPIC
$(TARGET_LIB): LDFLAGS += -Wl,--no-undefined
$(TARGET_LIB): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

debug: CFLAGS += -g
debug: clean $(TARGET_LIB)

check: library
	cp $(TARGET_LIB) tests && tests/testsuite.sh

main:
	gcc -o main src/main.c src/malloc.c src/allocator.c src/convert.c src/utilities.c

clean:
	$(RM) -f $(TARGET_LIB) $(OBJS) tests/libmalloc.so main *.snapshot

.PHONY: all library $(TARGET_LIB) clean
