CC = clang
CFLAGS = -Wall

worker.o: worker.h job.h task.h worker.c
	$(CC) $(CFLAGS) -c worker.c

job.o: job.h task.h job.c
	$(CC) $(CFLAGS) -c job.c

task.o: task.h task.c
	$(CC) $(CFLAGS) -c task.c
