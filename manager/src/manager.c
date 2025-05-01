#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "json.h"
#include "manager.h"

#define BUFLEN 1024

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
    node->worker.status = worker_not_busy;
    node->worker.job.id = -1;
    node->worker.job.status = job_not_assigned;
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

/* job_status_map: Maps the job status string returns its corresponding status
    code. Otherwise, returns -1. */
static job_status job_status_map(char *status) {
    if (strcmp(status, "running") == 0)
        return job_running;
    else if (strcmp(status, "complete") == 0)
        return job_completed;
    else if (strcmp(status, "incomplete") == 0)
        return job_incomplete;
    else
        return -1;
}

/* update_job_status: Updates the workers job status. */
static void update_job_status(Worker *worker, job_status status) {
    switch (status) {
        case job_running:
            worker->job.status = job_running;
            break;
        case job_completed:
            worker->job.status = job_completed;
            break;
        case job_incomplete:
            worker->job.status = job_incomplete;
            break;
        default:
            fprintf(stderr, "manager: Warning: Worker job status is unknown\n");
            break;
    }
    return;
}

/* worker_status_map: Maps the worker status string returns its corresponding
    status code. Otherwise, returns -1. */
static worker_status worker_status_map(char *status) {
    if (strcmp(status, "not_busy") == 0)
        return worker_not_busy;
    else if (strcmp(status, "busy") == 0)
        return worker_busy;
    else
        return -1;
}

/* update_status: Updates the workers status. */
static void update_status(Worker *worker, worker_status status) {
    switch (status) {
        case worker_busy:
            worker->status = worker_busy;
            break;
        case worker_not_busy:
            worker->status = worker_not_busy;
            break;
        default:
            fprintf(stderr, "manager: Warning: Worker status is unknown\n");
            break;
    }
    return;
}

/* worker_socket_handler: Closes the socket. */
static void *worker_socket_handler(void *arg) {
    if (close(*(int *)arg) == -1) {
        perror("manager: worker_socket_handler: close");
        exit(EXIT_FAILURE);
    }
    return NULL;
}

/* worker_thread: Updates the status of the worker and its job. */
static void *worker_thread(void *args) {
    Worker *worker = args;

    // Create unix socket
    int sockfd;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("manager: worker_thread: socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the worker
    struct sockaddr_un servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
#ifdef __APPLE__
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path),
        "/Users/leejahsprock/pyoneer/run/worker%d.socket", worker->id);
#elif __linux__
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path),
        "/run/pyoneer/worker%d.socket", worker->id);
#endif
    if (connect(sockfd, &servaddr, sizeof(servaddr)) == -1) {
        perror("manager: worker_thread: connect");
        exit(EXIT_FAILURE);
    }

    // Add close connection to cleanup handlers
    pthread_cleanup_push(worker_socket_handler, &sockfd);

    // Update worker status
    int nbyte, status;
    char buf[BUFLEN];
    json_value *res;
    char *get_status = "{\"command\":\"get_status\"}";
    char *get_job_status = "{\"command\":\"get_job_status\"}";
    while (sleep(1) == 0) {
        // Get worker status
        if (send(sockfd, get_status, strlen(get_status), 0) == -1) {
            perror("manager: worker_thread: send");
            exit(EXIT_FAILURE);
        }

        if ((nbyte = recv(sockfd, &buf, BUFLEN, 0)) == -1) {
            perror("manager: worker_thread: recv");
            exit(EXIT_FAILURE);
        }
        buf[nbyte > BUFLEN ? BUFLEN - 1 : nbyte] = '\0';

        if ((res = json_parse(buf, strlen(buf))) == NULL) {
            fprintf(stderr, "manager: worker_thread: json_parse: Unable to parse response\n");
            exit(EXIT_FAILURE);
        }
        
        for (int i = 0; i < res->u.object.length; i++) {
            if (strcmp(res->u.object.values[i].name, "status") == 0) {
                status = worker_status_map(res->u.object.values[i].value);
                update_status(worker, status);
            }
        }
        json_value_free(res);

        if (worker->job.status == job_not_assigned)
            continue;

        // Get worker job status
        if (send(sockfd, get_job_status, sizeof(get_job_status), 0) == -1) {
            perror("manager: worker_thread: send");
            exit(EXIT_FAILURE);
        }

        if ((nbyte = recv(sockfd, &buf, BUFLEN, 0)) == -1) {
            perror("manager: worker_thread: recv");
            exit(EXIT_FAILURE);
        }
        buf[nbyte > BUFLEN ? BUFLEN - 1 : nbyte] = '\0';

        if ((res = json_parse(buf, strlen(buf))) == NULL) {
            fprintf(stderr, "manager: worker_thread: json_parse: Unable to parse response\n");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < res->u.object.length; i++) {
            if (strcmp(res->u.object.values[i].name, "job_status") == 0) {
                status = job_status_map(res->u.object.values[i].value);
                update_job_status(worker, status);
            }
        }
        json_value_free(res);
    }

    pthread_cleanup_pop(0);
}

/* job_thread: Runs the job. */
static void *job_thread(void *args) {
    // Create unix socket

    // Connect to worker

    // Send command

    // Check response

    // Update
}


/* project_thread: Runs the project. */
static void *project_thread(void *args) {

}

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
