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

Worker* create_worker(int id);
void free_worker(Worker* worker);

// Commands
int run_job(Worker* worker);
int get_status(Worker* worker);
int get_job_status(Worker* worker);
int assign(Worker* worker, Job* job);
int unassign(Worker* worker);

// Signals
int start(Worker* worker);
int stop(Worker* worker);

#endif
