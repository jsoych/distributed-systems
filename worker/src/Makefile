cflags = -Wall -Wextra -Wpedantic

src_dir = .
build_dir = .

objs = main.o worker.o job.o task.o cJSON.o

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

main.o: worker.h cjson.h main.c
	$(cc) $(cflags) -c main.c

worker.o: worker.h job.h task.h worker.c
	$(cc) $(cflags) -c worker.c

job.o: job.h task.h job.c
	$(cc) $(cflags) -c job.c

task.o: task.h task.c
	$(cc) $(cflags) -c task.c

cJSON.o: cJSON.h cJSON.c
	$(cc) $(cflags) -c cJSON.c

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