CC = gcc
CFLAGS = -std=c99 -pedantic -Werror -Wall -Wextra -Wvla

SRC_FILES = $(wildcard src/*.c)
OBJ_FILES = $(patsubst %.c, %.o, $(SRC_FILES))

TARGET = a.out

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ_FILES)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

debug: CFLAGS += -g -fsanitize=address
debug: $(TARGET)

clean:
	${RM} -f $(TARGET) $(OBJ_FILES) *.snapshot main

.PHONY: all debug clean
