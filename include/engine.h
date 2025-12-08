#ifndef _ENGINE_H
#define _ENGINE_H

#include <pthread.h>

#include "job.h"
#include "worker.h"

typedef struct {
    Job* job;
    int task_count;
    pthread_t task_threads[];
} engine_runtime;

typedef struct _engine {
    int job_id;
    pthread_t job_thread;
    engine_runtime* runtime;
} Engine;

Engine *engine_create();
void engine_destroy(Engine* engine);

int engine_run(Engine* engine, Job* job);
int engine_stop(Engine* engine);

#endif
