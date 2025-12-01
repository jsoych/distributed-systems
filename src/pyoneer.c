#include "pyoneer.h"

/* pyoneer worker wrappers */
static int pyoneer_get_worker_status(Pyoneer* pyoneer) {
    return worker_get_status(pyoneer->as.worker);
}

static int pyoneer_get_job_status(Pyoneer* pyoneer) {
    return worker_get_job_status(pyoneer->as.worker);
}

static int pyoneer_run_job(Pyoneer* pyoneer, Blueprint* blueprint) {
    return worker_run(pyoneer->as.worker, blueprint->as.job);
}

static int pyoneer_assign_job(Pyoneer* pyoneer, Blueprint* blueprint) {
    return worker_assign(pyoneer->as.worker, blueprint->as.job);
}

static int pyoneer_unassign_job(Pyoneer* pyoneer, Blueprint* blueprint) {
    return worker_unassign(pyoneer->as.worker, blueprint->as.job);
}

static int pyoneer_worker_start(Pyoneer* pyoneer) {
    return worker_start(pyoneer->as.worker);
}

static int pyoneer_worker_stop(Pyoneer* pyoneer) {
    return worker_stop(pyoneer->as.worker);
}

/* pyoneer manager wrappers */

/* pyoneer_get_blueprint_kind: Gets the kind of blueprint and returns it. 
    Otherwise, returns -1. */
static int pyoneer_get_blueprint_kind(Pyoneer* pyoneer) {
    switch (pyoneer->role) {
        case PYONEER_WORKER:
            return BLUEPRINT_JOB;
        case PYONEER_MANAGER:
            return BLUEPRINT_PROJECT;
    }
    return -1;
}

/* pyoneer_create: Creates a new pyoneer. */
Pyoneer* pyoneer_create(int id, int role) {
    Pyoneer *pyoneer = malloc(sizeof(Pyoneer));
    if (pyoneer == NULL) {
        perror("pyoneer: pyoneer_create: malloc");
        return NULL;
    }
    switch (role) {
        case PYONEER_WORKER:
            pyoneer->role = PYONEER_WORKER;
            pyoneer->as.worker = worker_create(id);
            pyoneer->run = pyoneer_run_job;
            pyoneer->get_status = pyoneer_get_worker_status;
            pyoneer->get_blueprint_kind = pyoneer_get_blueprint_kind;
            pyoneer->get_blueprint_status = pyoneer_get_job_status;
            pyoneer->assign = pyoneer_assign_job;
            pyoneer->unassign = pyoneer_unassign_job;
            pyoneer->start = pyoneer_worker_start;
            pyoneer->stop = pyoneer_worker_stop;
            break;
        case PYONEER_MANAGER:
            pyoneer->role = PYONEER_MANAGER;
            pyoneer->as.manager = manager_create(id);
            pyoneer->get_blueprint_kind = pyoneer_get_blueprint_kind;
            // Add manager methods
            break;
    }
    return pyoneer;
}

/* pyoneer_destroy: Destroys the pyoneer. */
void pyoneer_destroy(Pyoneer* pyoneer) {
    switch (pyoneer->role) {
        case PYONEER_WORKER:
            worker_destroy(pyoneer->as.worker);
            break;
        case PYONEER_MANAGER:
            manager_destroy(pyoneer->as.manager);
            break;
    }
    free(pyoneer);
}

/* pyoneer_status_encode: Encodes the status of the pioneer. */
json_value* pyoneer_status_encode(Pyoneer* pyoneer) {
    switch (pyoneer->role) {
        case PYONEER_WORKER:
            return worker_status_encode(pyoneer->get_status(pyoneer));
        case PYONEER_MANAGER:
            return manager_status_encode(pyoneer->get_status(pyoneer));
    }
    return NULL;
}

/* pyoneer_status_decode: Decodes the object into the pyoneer status code. */
int pyoneer_status_decode(Pyoneer* pyoneer, json_value* obj) {
    switch (pyoneer->role) {
        case PYONEER_WORKER:
            return worker_status_decode(obj);
        case PYONEER_MANAGER:
            return manager_status_decode(obj);
    }
    return -1;
}
