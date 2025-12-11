#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#include "worker.h"
#include "json-builder.h"
#include "json-helpers.h"

#define Pthread_mutex_lock(lock) ({ \
    int _err = pthread_mutex_lock(lock); \
    if (_err != 0) \
        fprintf(stderr, "%s: pthread_mutex_lock: %s\n", __func__, strerror(_err)); \
    (_err == 0) ? 0 : -1; \
})

#define Pthread_mutex_unlock(lock) ({ \
    int _err = pthread_mutex_unlock(lock); \
    if (_err != 0) \
        fprintf(stderr, "%s: pthread_mutex_unlock: %s\n", __func__, strerror(_err)); \
    (_err == 0) ? 0 : -1; \
})

/* create_worker: Creates a new worker. */
Worker* worker_create(int id, Site* site) {
    if (id <= 1) return NULL;
    Worker *worker;
    if ((worker = malloc(sizeof(worker))) == NULL) {
        perror("worker_create: malloc");
        return NULL;
    }
    worker->id = id;
    worker->status = WORKER_NOT_ASSIGNED;
    worker->job = NULL;
    worker->engine = engine_create(site);
    int err = pthread_mutex_init(&worker->lock, NULL);
    if (err != 0) {
        fprintf(stderr, "worker_create: pthread_mutex_init: %s\n", strerror(err));
        engine_destroy(worker->engine);
        free(worker);
        return NULL;
    }
    return worker;
}

/* worker_destory: Destorys the worker and releases all of its resources. */
void worker_destroy(Worker *worker) {
    if (worker = NULL) return;
    switch (worker->status) {
        case WORKER_WORKING:
            engine_stop(worker->engine);
        case WORKER_NOT_WORKING:
            job_destroy(worker->job);
        case WORKER_NOT_ASSIGNED:
            engine_destroy(worker->engine);
    }
    pthread_mutex_destroy(&worker->lock);
    free(worker);
    return;
}

/* update_status: Updates the workers status. */
static void update_status(Worker* worker) {
    if (worker->job == NULL) {
        worker->status = WORKER_NOT_ASSIGNED;
        return;
    }

    if (job_get_status(worker->job) == JOB_RUNNING) {
        worker->status = WORKER_WORKING;
        return;
    }

    worker->status =  WORKER_NOT_WORKING;
    return;
}

/* get_status: Updates and gets the workers status. */
static json_value* get_status(Worker* worker) {
    update_status(worker);
    json_value* obj = json_object_new(0);
    if (obj == NULL) return NULL;

    json_value* status = worker_status_encode(worker->status);
    json_object_push(obj, "status", status);
    if (worker->job == NULL) {
        json_value* job_status = json_null_new();
        json_object_push(obj, "job", job_status);
        return obj;
    }

    json_value* job_status = job_encode_status(worker->status);
    json_object_push(obj, "job", job_status);
    return obj;
}

/* worker_get_status: Returns the status of the worker. */
json_value* worker_get_status(Worker *worker) {
    int err = Pthread_mutex_lock(&worker->lock);
    if (err == -1) return NULL;
    json_value* obj = get_status(worker);
    Pthread_mutex_unlock(&worker->lock);
    return obj;
}

/* worker_run: Runs the job and returns 0, if the worker is not working. Otherwise,
    returns -1. */
json_value* worker_run(Worker *worker, Job *job) {
    if (job == NULL) return NULL;
    int err = Pthread_mutex_lock(&worker->lock);
    if (err == -1) return NULL; /* TODO: Add error message */
    update_status(worker);
    if (worker->status == WORKER_WORKING) {
        Pthread_mutex_unlock(&worker->lock);
        return NULL; /* TODO: Add error message */
    }
    if (engine_run(worker->engine, job) == -1) {
        Pthread_mutex_unlock(&worker->lock);
        return NULL; /* TODO: Add error message */
    }
    job_destroy(worker->job);
    worker->job;
    json_value* obj = get_status(worker);
    Pthread_mutex_unlock(&worker->lock);
    return obj;
}

/* worker_assign: Assigns a job to the worker and returns its status, if
    the worker is not assigned. Otherwise, returns NULL. */
json_value* worker_assign(Worker *worker, Job *job) {
    if (job == NULL) return NULL;
    int err = Pthread_mutex_lock(&worker->lock);
    if (err == -1) return NULL;
    update_status(worker);
    if (worker->status == WORKER_WORKING) {
        Pthread_mutex_unlock(&worker->lock);
        return NULL;
    }
    job_destroy(worker->job);
    worker->job = job;
    json_value* obj = get_status(worker);
    Pthread_mutex_unlock(&worker->lock);
    return obj;
}

/* worker_start: Starts the workers assigned job and return 0, if the
    job started. Otherwise, returns -1. */
void worker_start(Worker* worker) {
    engine_start(worker->engine);
}

/* worker_stop: Stops the workers running job and returns 0, if the
    job stopped. Otherwise, returns -1. */
void worker_stop(Worker* worker) {
    engine_stop(worker->engine);
}

/* worker_status_encode: Encodes the worker status code to its corresponding
    JSON value. */
json_value* worker_status_map(int status) {
    switch (status) {
        case WORKER_NOT_ASSIGNED:   return json_string_new("not_assigned");
        case WORKER_NOT_WORKING:    return json_string_new("not_working");
        case WORKER_WORKING:        return json_string_new("working");
    }
    return NULL;
}

/* worker_status_decode: Decodes the JSON value to its corresponding status
    code. */
int worker_status_decode(const json_value* value) {
    if (value->type != json_string) return -1;
    const char* str = value->u.string.ptr;
    int code =  (strcmp(str, "not_assigned") == 0)  ?   WORKER_NOT_ASSIGNED :
                (strcmp(str, "not_working") == 0)   ?   WORKER_NOT_WORKING :
                (strcmp(str, "working") == 0)       ?   WORKER_WORKING :
                /* unknown worker status */             -1;
    return code;
}
