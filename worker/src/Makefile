cflags = -Wall -Wextra -Wpedantic

src_dir = .
build_dir = .

objs = main.o worker.o job.o task.o json.o json-builder.o

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
	$(cc) $(cflags) $^ -lm -o worker

main.o: worker.h json.h main.c
	$(cc) $(cflags) -c main.c

worker.o: worker.h job.h task.h worker.c
	$(cc) $(cflags) -c worker.c

job.o: job.h json.h json-builder.h task.h job.c
	$(cc) $(cflags) -c job.c

task.o: task.h task.c
	$(cc) $(cflags) -c task.c

json-builder: json-builder.h json.h json-builder.c
	$(cc) $(cflags) -c json-builder.c

json.o:	json.h json.c
	$(cc) $(flags) -c json.c

.phony: clean install uninstall

clean:
	rm -f *.o worker

install:
	mkdir -p $(bin)
	mkdir -p $(run)
	mkdir -p $(var)
	mv worker $(bin)

uninstall:
	rm -f $(bin)/worker $(run)/worker*.socket $(var)/worker*.log
