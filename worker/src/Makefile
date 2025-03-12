CFLAGS := -Wall

SRC_DIR := .
BUILD_DIR := .

OBJS := main.o worker.o job.o task.o cJSON.o

ifeq ($(shell uname),Darwin)
	CC := clang
	BIN := /Users/leejahsprock/pyoneer/bin
endif

ifeq ($(shell uname),Linux)
	CC := gcc
	BIN := /usr/pyoneer/bin
endif

worker: $(OBJS)
	$(CC) $(CFLAGS) $^ -o worker
	mkdir -p $(BIN)
	mv worker $(BIN)

main.o: worker.h cJSON.h main.c
	$(CC) $(CFLAGS) -c main.c

worker.o: worker.h job.h task.h worker.c
	$(CC) $(CFLAGS) -c worker.c

job.o: job.h task.h job.c
	$(CC) $(CFLAGS) -c job.c

task.o: task.h task.c
	$(CC) $(CFLAGS) -c task.c

cJSON.o: cJSON.h cJSON.c
	$(CC) $(CFLAGS) -c cJSON.c

.PHONY: clean
clean:
	rm -f *.o worker