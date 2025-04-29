#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "manager.h"

/* create_manager: Creates a new manager. */
Manager *create_manager(int id) {
    Manager *man;
    if ((man = malloc(sizeof(Manager))) == NULL) {
        perror("manager: malloc");
        exit(EXIT_SUCCESS);
    }
    man->id = id;
    man->status = _MANAGER_NOT_BUSY;
    man->crew.workers = NULL;
    man->crew.freelist = NULL;
    pthread_mutex_init(&man->crew.lock,NULL);
    man->running_proj = NULL;
    return man;
}

/* free_manager: Frees the memory allocated to the manager. */
void free_manager(Manager *man) {
    int old_errno;
    // Free running project memory

    // Free workers
    crew_node *curr = man->crew.workers;
    while (curr) {
        if ((old_errno = pthread_cancel(curr->tid)) != 0)
            fprintf(strerror, "manager: free_manager: pthread_cancel: %s\n",
                strerror(old_errno));
        curr = curr->next;
        free(curr->prev);
    }
    
    // Destroy lock
    if ((old_errno = pthread_mutex_destroy(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: free_manager: pthread_mutex_destroy: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    free(man);
}

/* add_worker: Creates a new worker with id and add it to the crew. */
void add_worker(Manager *man, int id) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: add_worker: pthread_mutex_lock: %s\n",
            strerror(old_errno));
        return;
    }
    
    crew_node *node;
    if ((node = malloc(sizeof(crew_node))) == NULL) {
        perror("manager: add_worker: malloc");
        exit(EXIT_FAILURE);
    }
    node->worker.id = id;
    node->worker.status = _WORKER_NOT_BUSY;
    node->next = NULL;
    node->prev_free = NULL;

    if (man->crew.len == 0) {
        node->prev = NULL;
        man->crew.workers = node;
        node->next_free = NULL;
        man->crew.freelist = node;
        man->crew.len++;
        goto unlock;
    }

    // Check id against all worker ids
    crew_node *curr = man->crew.workers;
    while (curr->next) {
        if (curr->worker.id == id) {
            fprintf(stderr, "manager: warning: trying to add a worker with the same id\n");
            free(node);
            goto unlock;
        }
        curr = curr->next;
    }
    if (curr->worker.id == id) {
        fprintf(stderr, "manager: warning: try to add a worker with the same id\n");
        free(node);
        goto unlock;
    }

    // Add worker
    curr->next = node;
    node->prev = curr;

    // Add worker to the start of the freelist
    man->crew.freelist->prev_free = node;
    node->next_free = man->crew.freelist;
    man->crew.freelist = node;
    man->crew.len++;

    if ((old_errno = pthread_create(&man->crew.freelist->tid, NULL,
        worker_thread, &man->crew.freelist->worker)) != 0) {
            fprintf(stderr, "manager: add_worker: pthread_create: %s\n",
                strerror(old_errno));
            exit(EXIT_FAILURE);
    }

    unlock:
    if ((old_errno = pthread_mutex_unlock(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: add_worker: pthread_mutex_unlock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* get_status: Gets the manager status. */
int get_status(Manager *man) {
    return man->status;
}

/* get_project_status: Gets the status of the running project. */
int get_project_status(Manager *man) {
    return man->running_proj->proj->status;
}

/* get_worker_status: Gets the status of the worker by its id. Otherwise,
    returns -1. */
int get_worker_status(Manager *man, int id) {
    int ret = -1;
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: get_worker_status: pthread_mutex_lock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    crew_node *curr = man->crew.workers;
    while (curr) {
        if (curr->worker.id = id)
            ret = curr->worker.id;
        curr = curr->next;
    }

    if ((old_errno = pthread_mutex_unlock(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: get_worker_status: pthread_mutex_unlock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    return ret;
}

/* worker_thread: Updates the status of the worker. */
static void *worker_thread(void *args) {
    Worker *worker = args;
    // Create local socket
    int len, sockfd;
    struct sockaddr_un servaddr;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("manager: worker_thread: socket");
        exit(EXIT_FAILURE);
    }
    
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    char id[4];
    char *s, *path;
    path = servaddr.sun_path;
#ifdef __APPLE__
    s = stpncpy(s, "/Users/leejahsprock/pyoneer/run/", sizeof(path) - 1);
#elif __linux__
    s = stpcpy(s, "/run/pyoneer/", sizeof(path) - 1);
#endif
    if ((strlen(path) + 18) > sizeof(servaddr.sun_path)) {
        fprintf(stderr, "manager: worker_thread: path name is too long\n");
        exit(EXIT_FAILURE);
    }
    s = stpcpy(s, "worker");
    // Convert id to string
    sprintf(id, "%d", worker->id);
    s = stpcpy(s, id);
    s = stpcpy(s, ".socket");

    // Connect to the worker

    // Add close connection to cleanup handlers

    while (sleep(1) == 0) {
        // Update worker status
    }

    // return something
}

/* project_thread: Runs the project. */
static void *project_thread(void *args) {

}

/* job_thread: Runs the job. */


/* run_project: Runs the project and returns 0. If the manager is working
    run_project returns -1. */
int run_project(Manager *man, Project *proj) {
    // create jobs schedule

    // create project thread

    return 0;
}

static struct commmand {
    char *cmd;
    void *args;
};

/* call_command: Calls one of the manager's commands and returns a
    response JSON object. */
cJSON *call_command(Manager *man, cJSON *cmd) {
    cJSON *obj, *item, *res;
    res = cJSON_CreateObject();
    
    return res;
}
