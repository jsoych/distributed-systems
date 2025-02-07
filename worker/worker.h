#ifndef _WORKER_H
#define _WORKER_H
#define _WORKER_NOT_WORKING -1
#define _WORKER_WORKING 0

#include <pthread.h>
#include "job.h"

typedef struct _worker {
    int id;
    int status;
    pthread_mutex_t lock;
} Worker;

Worker *create_worker(int,int);
void free_worker(Worker *);
int run_job(Worker *, Job *);

#endif