cflags = -g -O0 -Wall -Wextra -Wpedantic

src_dir = .
build_dir = .

objs = main.o manager.o project.o job.o task.o json-builder.o json.o

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

manager: $(objs)
	$(cc) $(cflags) $^ -lm -o manager

main.o: manager.h project.h json.h json-builder.h main.c
	$(cc) $(cflags) -c main.c

manager.o: manager.h project.h manager.c
	$(cc) $(cflags) -c manager.c

project.o: project.h job.h json.h json-builder.h project.c
	$(cc) $(cflags) -c project.c

job.o: job.h task.h job.c
	$(cc) $(cflags) -c job.c

task.o: task.h task.c
	$(cc) $(cflags) -c task.c

json-builder.o: json-builder.h json.h json-builder.c
	$(cc) $(cflags) -c json-builder.c

json.o: json.h json.c
	$(cc) $(cflags) -c json.c

.phony: clean install uninstall

clean:
	rm -f *.o manager

install:
	mkdir -p $(bin)
	mkdir -p $(run)
	mkdir -p $(var)
	mv manager $(bin)

uninstall:
	rm -i $(bin)/manager $(run)/manager*.socket $(var)/manager*.log

test: test-project test-job test-task
	./test-task
	./test-job
	./test-project

test-manager: test-manager.c unittest.o json.o json-builder.o manager.o project.o job.o task.o
	$(cc) $(cflags) -g test-manager.c unittest.o json.o json-builder.o manager.o project.o job.o task.o -lm -o test-manager

test-project: test-project.c json.o json-builder.o project.o job.o task.o
	$(cc) $(cflags) -g test-project.c json.o json-builder.o project.o job.o task.o -lm -o test-project

test-job: test-job.c json.o json-builder.o job.o task.o
	$(cc) $(cflags) -g test-job.c json.o json-builder.o job.o task.o -lm -o test-job

test-task: test-task.c task.o
	$(cc) $(cflags) -g test-task.c task.o -o test-task

unnittest.o: unittest.h unittest.c
	$(cc) $(cflags) -c unnittest.c
