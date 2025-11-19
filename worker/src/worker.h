#ifndef pyoneer_worker_h
#define pyoneer_worker_h

#include <pthread.h>
#include <semaphore.h>
#include "job.h"

typedef enum {
    WORKER_NOT_ASSIGNED,
    WORKER_NOT_WORKING,
    WORKER_WORKING
} worker_status_t;

struct _worker;

typedef struct _running_job_node {
    job_node* task;
    pid_t pid;
    pthread_t tid;
    struct _running_job_node *next;
} running_job_node;

typedef struct _running_job {
    struct _worker *worker;
    Job *job;
    pthread_t tid;
    running_job_node *head;
} RunningJob;

typedef struct _worker {
    int id;
    worker_status_t status;
    RunningJob *running_job;
    sem_t lock;
} Worker;

Worker* worker_create(int id);
void worker_destroy(Worker* worker);

// Commands
int worker_run_job(Worker* worker);
int worker_get_status(Worker* worker);
int wokrer_get_job_status(Worker* worker);
int worker_assign(Worker* worker, Job* job);
int worker_unassign(Worker* worker);

// Signals
int worker_start(Worker* worker);
int worker_stop(Worker* worker);

#endif
