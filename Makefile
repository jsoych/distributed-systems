TARGET := pyoneer

SRC_DIR := src
INCLUDE_DIR := include
BUILD_DIR := build

CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -I$(INCLUDE_DIR)
LDFLAGS := -lm

# Find all source files
SRCS := src/json.c src/json-builder.c src/json-helpers.c
SRCS += src/task.c
OBJS := $(subst $(SRC_DIR),$(BUILD_DIR),$(SRCS))
OBJS := $(subst .c,.o,$(OBJS))


all: $(OBJS)

$(OBJS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

TEST_SRCS := tests/shared/unittest.c
TEST_SRCS += tests/blueprints/test_task.c

CFLAGS += -Itests/shared

TEST_OBJS := $(BUILD_DIR)/unittest.o # $(BUILD_DIR)/test_unittest.o
TEST_OBJS += $(BUILD_DIR)/test_task.o

test: $(TEST_OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(TEST_OBJS) -o $(BUILD_DIR)/$@

$(BUILD_DIR)/test_unittest: tests/shared/test_unittest.c
	$(CC) $(CFLAGS) -c $< -o $@

build/unittest.o: tests/shared/unittest.c
	$(CC) $(CFLAGS) -c $< -o $@

build/test_task.o: tests/blueprints/test_task.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/*

.phony: all test clean install

