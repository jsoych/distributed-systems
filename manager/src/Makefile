cflags = -Wall -Wextra -Wpedantic

src_dir = .
build_dir = .

objs = main.o manager.o project.o

ifeq ($(shell uname),Darwin)
	cc = clang
	bin = ~/pyoneer/bin
	run = ~/pyoneer/run
	var = ~/pyoneer/var/log
endif

ifeq ($(shell uname),Linux)
	cc = gcc
	bin = /usr/bin/pyoneer
	run = /run/pyoneer
	var = /var/log/pyoneer
endif

worker: $(objs)
	$(cc) $(cflags) $^ -o worker

main.o: worker.h cJSON.h main.c
	$(cc) $(cflags) -c main.c

manager.o: manager.h project.h manager.c
	$(cc) $(cflags) -c manager.cc

project.o: project.h job.h project.c
	$(cc) $(cflags) -c project.c

cJSON.o: cJSON.h cJSON.c
	$(cc) $(cflags) -c cJSON.c

.phony: clean install uninstall

clean:
	rm -f *.o manager

install:
	mkdir -p $(bin)
	mkdir -p $(run)
	mkdir -p $(var)
	mv worker $(bin)

uninstall:
	rm -i $(bin)/manager $(run)/manager*.socket $(var)/manager*.log