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

static void pyoneer_worker_stop(Pyoneer* pyoneer) {
    worker_stop(pyoneer->as.worker);
}

static void pyoneer_worker_start(Pyoneer* pyoneer) {
    worker_start(pyoneer->as.worker);
}

/* pyoneer manager wrappers */

/* pyoneer_create: Creates a new pyoneer. */
Pyoneer* pyoneer_create(int id, int role, Site* site) {
    Pyoneer *pyoneer = malloc(sizeof(Pyoneer));
    if (pyoneer == NULL) {
        perror("pyoneer: pyoneer_create: malloc");
        return NULL;
    }
    switch (role) {
        case PYONEER_WORKER:
            pyoneer->role = PYONEER_WORKER;
            pyoneer->as.worker = worker_create(id, site);
            pyoneer->run = pyoneer_run_job;
            pyoneer->get_status = pyoneer_get_worker_status;
            pyoneer->assign = pyoneer_assign_job;
            pyoneer->start = pyoneer_worker_start;
            pyoneer->stop = pyoneer_worker_stop;
            break;
        case PYONEER_MANAGER:
            pyoneer->role = PYONEER_MANAGER;
            pyoneer->as.manager = manager_create(id);
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

Blueprint* pyoneer_blueprint_decode(Pyoneer* pyoneer, const json_value* val) {
    if (val->type != json_object) return NULL;
    switch (pyoneer->role) {
        case PYONEER_WORKER:
            return blueprint_decode(val, BLUEPRINT_JOB);
        case PYONEER_MANAGER:
            return blueprint_decode(val, BLUEPRINT_PROJECT);
    }
    return NULL;
}
