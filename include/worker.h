#ifndef _WORKER_H
#define _WORKER_H

#include <pthread.h>
#include <semaphore.h>

#include "job.h"
#include "json.h"

enum {
    WORKER_NOT_ASSIGNED,
    WORKER_NOT_WORKING,
    WORKER_WORKING
};

struct _worker;

typedef struct _running_job_node {
    job_node* task;
    pid_t pid;
    pthread_t tid;
    struct _running_job_node *next;
} running_job_node;

typedef struct _running_job {
    struct _worker* worker;
    Job* job;
    pthread_t tid;
    running_job_node* head;
} RunningJob;

typedef struct _worker {
    int id;
    int status;
    RunningJob* running_job;
    sem_t lock;
} Worker;

Worker* worker_create(int id);
void worker_destroy(Worker* worker);

// Commands
int worker_get_status(Worker* worker);
int worker_get_job_status(Worker* worker);
int worker_run(Worker* worker, Job* job);
int worker_assign(Worker* worker, Job* job);
int worker_unassign(Worker* worker, Job* job);

// Signals
int worker_start(Worker* worker);
int worker_stop(Worker* worker);

// Helpers
json_value* worker_status_encode(int status);
int worker_status_decode(json_value* obj);

#endif
