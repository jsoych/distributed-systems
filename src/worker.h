#ifndef _WORKER_H
#define _WORKER_H

#include <pthread.h>
#include <semaphore.h>
#include "job.h"

typedef struct _worker Worker;

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

#endif
