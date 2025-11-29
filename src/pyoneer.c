#include "common.h"
#include "pyoneer.h"

typedef int (*command)(Pyoneer*);
typedef int (*command_arg)(Pyoneer*, void*);
typedef int (*signal)(Pyoneer*);

struct _pyoneer {
    pyoneer_t type;
    union {
        Worker* worker;
        Manager* manager;
    } as;
    command get_status;
    command get_job_status;
    command get_project_status;
    command_arg run;
    command_arg assign;
    command_arg unassign;
    signal start;
    signal stop;
};

/* pyoneer worker wrappers */
static int pyoneer_get_worker_status(Pyoneer* pyoneer) {
    return worker_get_status(pyoneer->as.worker);
}

static int pyoneer_get_job_status(Pyoneer* pyoneer) {
    return worker_get_job_status(pyoneer->as.worker);
}

static int pyoneer_run_job(Pyoneer* pyoneer, void* job) {
    return worker_run(pyoneer->as.worker, job);
}

static int pyoneer_assign_job(Pyoneer* pyoneer, void* job) {
    return worker_assign(pyoneer->as.worker, job);
}

static int pyoneer_unassign_job(Pyoneer* pyoneer, void* job) {
    return worker_unassign(pyoneer->as.worker, job);
}

static int pyoneer_worker_start(Pyoneer* pyoneer) {
    return worker_start(pyoneer->as.worker);
}

static int pyoneer_worker_stop(Pyoneer* pyoneer) {
    return worker_stop(pyoneer->as.worker);
}

/* pyoneer manager wrappers */

/* pyoneer_create: Creates a new pyoneer. */
Pyoneer* pyoneer_create(int id, pyoneer_t type) {
    Pyoneer *pyoneer = malloc(sizeof(Pyoneer));
    if (pyoneer == NULL) {
        perror("pyoneer: pyoneer_create: malloc");
        exit(EXIT_FAILURE);
    }
    switch (type) {
        case TYPE_WORKER:
            pyoneer->type = TYPE_WORKER;
            pyoneer->as.worker = worker_create(id);
            pyoneer->run = pyoneer_run_job;
            pyoneer->get_status = pyoneer_get_worker_status;
            pyoneer->get_job_status = pyoneer_get_job_status;
            pyoneer->get_project_status = NULL;
            pyoneer->assign = pyoneer_assign_job;
            pyoneer->unassign = pyoneer_unassign_job;
            pyoneer->start = pyoneer_worker_start;
            pyoneer->stop = pyoneer_worker_stop;
            break;
        case TYPE_MANAGER:
            pyoneer->type = TYPE_MANAGER;
            pyoneer->as.manager = manager_create(id);
            // Add manager methods
            break;
    }
    return pyoneer;
}

/* pyoneer_destroy: Destroys the pyoneer. */
void pyoneer_destroy(Pyoneer* pyoneer) {
    switch (pyoneer->type) {
        case TYPE_WORKER:
            worker_destroy(pyoneer->as.worker);
            break;
        case TYPE_MANAGER:
            manager_destroy(pyoneer->as.manager);
            break;
    }
    free(pyoneer);
}
