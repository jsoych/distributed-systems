CC = clang
CFLAGS = -Wall
objects = main.o worker.o job.o task.o cJSON.o

worker: $(objects)
	$(CC) $(CFLAGS) $^ -o worker

main.o: worker.h cJSON.h main.c
	$(CC) $(CFLAGS) -c main.c

worker.o: worker.h job.h task.h worker.c
	$(CC) $(CFLAGS) -c worker.c

job.o: job.h task.h job.c
	$(CC) $(CFLAGS) -c job.c

task.o: task.h task.c
	$(CC) $(CFLAGS) -c task.c

cJSON: cJSON.h cJSON.c
	$(CC) $(CFLAGS) -c cJSON.c

.PHONY: clean
clean:
	rm -f *.o worker