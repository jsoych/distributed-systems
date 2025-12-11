#ifndef _ENGINE_H
#define _ENGINE_H

#include <pthread.h>

#include "job.h"
#include "worker.h"

typedef struct _engine {
    Site* job_site;
    JobRunner* job_runner;
} Engine;

Engine *engine_create(Site* site);
void engine_destroy(Engine* engine);

int engine_run(Engine* engine, Job* job);
void engine_stop(Engine* engine);
void engine_start(Engine* engine);

#endif
