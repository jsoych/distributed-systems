CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic
CFLAGS += -I../include -Ishared
LDFLAGS := -lm
TESTS := test_task

BLUEPRINTS_SRCS := $(shell find blueprints -name '*.c')
BLUEPRINTS_OBJS := $(BLUEPRINTS_SRCS:%.c=build/%.o)

SHARED_SRCS := $(shell find shared -name '*.c')
SHARED_OBJS := $(SHARED_SRCS:%.c=build/%.o)

PYONEER_OBJS := $(shell find ../build -name '*.o')

all: bin/$(TESTS)

run: $(TESTS)
	@echo "----- Running task test ------"
	@./bin/test_task

bin/test_task: $(BLUEPRINTS_OBJS) $(SHARED_OBJS) $(PYONEER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

build/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/*

.phony: all clean test
