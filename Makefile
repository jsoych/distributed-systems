TARGET := bin/pyoneer

CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS := -lm

SRCS := $(shell find src -name '*.c')
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/* bin/*

.PHONY: all clean
