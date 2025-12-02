TARGET := pyoneer

SRC_DIR := src
BUILD_DIR := build

CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic
LDFLAGS := -lm

# Find all source files
SRCS := $(shell find $(SRC_DIR) -name '*.c')

OBJS := $(notdir $(SRCS))
OBJS := $(OBJS:.c=.o)
OBJS := $(addprefix $(BUILD_DIR)/,$(OBJS))

TEST_DIR := tests/src

# Debug build
debug: CFLAGS += -g -O0 -DDEBUG
debug: all

# Clean
.PHONY: all debug clean

clean:
	rm -rf $(BUILD_DIR)/*
