CFLAGS := -Wall

SRC_DIR := .
BUILD_DIR := .

OBJS := main.o worker.o job.o task.o cJSON.o

ifeq ($(uname),Darwin)
	CC := clang
	PYTHON := conda run -n pyoneer python
endif

ifeq ($(uname),Linux)
	CC := gcc
endif

worker: $(OBJS)
	$(CC) $(CFLAGS) $^ -o worker

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

test: test.py
	./worker 3 debug
	$(PYTHON) test.py

.PHONY: clean
clean:
	rm -f *.o worker