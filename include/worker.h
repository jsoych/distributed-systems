#ifndef _WORKER_H
#define _WORKER_H

#include <pthread.h>

#include "engine.h"
#include "job.h"
#include "json.h"

enum {
    WORKER_NOT_ASSIGNED,
    WORKER_NOT_WORKING,
    WORKER_WORKING
};

typedef struct _worker {
    int id;
    int status;
    Job* job;
    Engine* engine;
    pthread_mutex_t lock;
} Worker;

Worker* worker_create(int id, Site* site);
void worker_destroy(Worker* worker);

// Commands
json_value* worker_get_status(Worker* worker);
json_value* worker_run(Worker* worker, Job* job);
json_value* worker_assign(Worker* worker, Job* job);

// Signals
void worker_start(Worker* worker);
void worker_stop(Worker* worker);

// Helpers
json_value* worker_status_map(int status);
int worker_status_decode(const json_value* value);

#endif
