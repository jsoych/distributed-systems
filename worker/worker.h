#ifndef _WORKER_H
#define _WORKER_H
#define _WORKER_NOT_WORKING -1
#define _WORKER_WORKING 0

#include <pthread.h>
#include "job.h"

struct _worker;

typedef struct _running_job_node {
    JobNode *job_node;
    pthread_t task_tid;
    struct _running_job_node *next;
} RunningJobNode;

typedef struct _running_job {
    struct _worker *worker;
    Job *job;
    pthread_t job_tid;
    RunningJobNode *head;
} RunningJob;

typedef struct _worker {
    int id;
    int status;
    RunningJob *running_job;
} Worker;

Worker *create_worker(int);
void free_worker(Worker *);
int run_job(Worker *, Job *);
int start(Worker *);
void stop(Worker *);

#endif