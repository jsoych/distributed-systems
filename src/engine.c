#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "engine.h"

/* engine_create: Creates a new engine. */
Engine* engine_create(Site* site) {
    if (!site) return NULL;
    Engine* eng = malloc(sizeof(Engine));
    if (eng == NULL) {
        perror("engine_create: malloc");
        return NULL;
    }  
    eng->job_site = site;
    eng->job_runner = NULL;
    return eng;
}

/* engine_destroy: Destroys the engine and releases all of its resources. */
void engine_destroy(Engine* eng) {
    engine_stop(eng);
    site_destroy(eng->job_site);
    free(eng);
}

/* engine_run: Runs the job at the engine job site. */
int engine_run(Engine* engine, Job* job) {
    JobRunner* runner = job_run(job, engine->job_site);
    if (!runner) return -1;
    job_runner_destroy(engine->job_runner);
    engine->job_runner = runner;
    return 0;
}

/* engine_stop: Stops the engine. */
void engine_stop(Engine* engine) {
    job_runner_stop(engine->job_runner);
}

/* engine_start: Starts the engine. */
void engine_start(Engine* engine) {
    job_runner_start(engine->job_runner);
}
