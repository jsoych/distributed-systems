TARGET := pyoneer

SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build

CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -I$(INCLUDE_DIR)
LDFLAGS := -lm

# Find all source files
SRCS := src/json.c src/json-builder.c src/json-helpers.c
SRCS += src/task.c src/job.c
OBJS := $(subst $(SRC_DIR),$(BUILD_DIR),$(SRCS))
OBJS := $(subst .c,.o,$(OBJS))


all: $(OBJS)

$(OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test:

clean:
	rm -rf build/*

.phony: all test clean

