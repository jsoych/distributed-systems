target_exec := pyoneer

build_dir := ./build
src_dir := ./src

src := $(shell find $(src_dir) -name '*.c')

objs := $(src:%=$(build_dir)/%.o)

cc := gcc
cflags := -Wall -Wextra -Wpedantic
ldflags := -lmath

$(build_dir)/$(target_exec): $(objs)
	$(cc) $(objs) -o $@ $(ldflags)

$(build_dir)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(cc) $(cflags) -c $< -o $@

debug: cflags += -g -O0 -DDEBUG
debug: $(build_dir)/$(target_exec)

.PHONY: clean debug
clean:
	rm -r $(build_dir)
