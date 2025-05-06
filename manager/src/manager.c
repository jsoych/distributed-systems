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
    man->status = _MANAGER_NOT_WORKING;
    man->crew.workers = NULL;
    man->crew.freelist = NULL;
    man->crew.len = 0;
    man->crew.freelen = 0;
    pthread_mutex_init(&man->crew.lock,NULL);
    man->running_project = NULL;
    return man;
}

/* free_manager: Frees the memory allocated to the manager. */
void free_manager(Manager *man) {
    int old_errno;
    // Free running project memory

    // Free workers
    crew_node *curr = man->crew.workers;
    for (int i = 1; i < man->crew.len; ++i) {
        if ((old_errno = pthread_cancel(curr->tid)) != 0)
            fprintf(strerror, "manager: free_manager: pthread_cancel: %s\n",
                strerror(old_errno));
        curr = curr->next;
        free(curr->prev);
    }
    free(curr);

    // Destroy lock
    if ((old_errno = pthread_mutex_destroy(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: free_manager: pthread_mutex_destroy: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    free(man);
    return;
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
    node->worker.status = worker_not_assigned;
    node->worker.job = NULL;

    if (man->crew.len == 0) {
        node->next = node;
        node->prev = node;
        node->next_free = node;
        node->prev_free = node;
        man->crew.workers = node;
        man->crew.freelist = node;
        man->crew.len++;
        man->crew.freelen++;
        goto unlock;
    }

    // Check id against all worker ids
    crew_node *curr = man->crew.workers;
    for (int i = 0; i < man->crew.len; i++) {
        if (curr->worker.id == id) {
            fprintf(stderr, "manager: Warning: Trying to add a worker with the same id\n");
            free(node);
            goto unlock;
        }
        curr = curr->next;
    }

    node->next = man->crew.workers;
    man->crew.workers->prev = node;
    node->next_free = man->crew.freelist;
    man->crew.freelist->prev_free = node;
    man->crew.len++;
    man->crew.freelen++;

    if ((old_errno = pthread_create(&node->tid, NULL,
            worker_thread, &node->worker)) != 0) {
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
    return man->running_project->project->status;
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
        case _JOB_RUNNING:
            worker->job->status = _JOB_RUNNING;
            break;
        case _JOB_COMPLETED:
            worker->job->status = _JOB_COMPLETED;
            break;
        case _JOB_INCOMPLETE:
            worker->job->status = _JOB_INCOMPLETE;
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

        if (worker->status == worker_not_assigned)
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

/* create_queue: Creates a new queue. */
static struct queue *create_queue() {
    queue *q;
    if ((q = malloc(sizeof(queue))) == NULL) {
        perror("manager: create_queue: malloc");
        exit(EXIT_FAILURE);
    }
    q->head = NULL;
    q->tail = NULL;
    q->len = 0;
    return q;
}

/* free_queue: Frees all memory allocated to the queue. */
static void free_queue(queue *q) {
    if (q->len == 0) {
        free(q);
        return;
    }
    RunningProjectNode *curr = q->head;
    while (curr) {
        curr = curr->next;
        free(curr->prev->deps);
        free(curr->prev);
    }
    free(q);
    return;
}

/* add_node: Adds a node to the tail of the queue. */
static void add_node(queue *q, RunningProjectNode *n) {
    n->next = NULL;
    if (q->len == 0) {
        n->prev = NULL;
        q->head = n;
        q->tail = n;
        q->len++;
        return;
    }
    n->prev = q->tail;
    q->tail = n;
    q->len++;
    return;
}

/* remove_node: Removes a node from the queue and returns it. */
static struct node *remove_node(queue *q, RunningProjectNode *n) {
    q->len--;
    if (q->len == 0) {
        q->head = NULL;
        q->tail = NULL;
        return n;
    }
    if (n->prev)
        n->prev->next = n->next;
    else
        q->head = n->next;
    if (n->next)
        n->next->prev = n->prev;
    else
        q->tail = n->prev;
    n->next = NULL;
    n->prev = NULL;
    return n;
}

/* create_running_project: Creates a new running project. */
static RunningProject *create_running_project(Manager *man, Project *proj) {
    RunningProject *rproj;
    if ((rproj = malloc(sizeof(RunningProject))) == NULL) {
        perror("manager: create_running_project: malloc");
        exit(EXIT_FAILURE);
    }
    rproj->manager = man;
    rproj->project = proj;
    rproj->not_ready_jobs = create_queue();
    rproj->ready_jobs = create_queue();
    rproj->running_jobs = create_queue();
    rproj->comleted_jobs = create_queue();
    rproj->incomplete_jobs = create_queue();
    return rproj;
}

/* free_running_project: Frees all resources allocated to the running
    project. */
static void free_running_project(RunningProject *rproj) {
    free_queue(rproj->not_ready_jobs);
    free_queue(rproj->ready_jobs);
    free_queue(rproj->running_jobs);
    free_queue(rproj->comleted_jobs);
    free_queue(rproj->incomplete_jobs);
    free(rproj);
    return;
}

/* is_ready: Checks the status of each dependency and returns job status
    ready, if all the jobs are complete. Otherwise, it returns job status
    not ready. */
static int is_ready(RunningProjectNode *node) {
    for (int i = 0; i < node->len; i++) {
        if (node->deps[i]->project_node->job->status != _JOB_COMPLETED)
            return _JOB_NOT_READY;
    }
    return _JOB_READY;
}

/* assign: Assigns jobs in the ready queue available workers. */
static int assign_job(Manager *man, Job *job) {
    int old_errno, sockfd, ret;
    if ((old_errno = pthread_mutex_lock(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: assign_job: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    if (man->crew.freelen == 0) {
        ret = -1;
        goto unlock;
    }

    // Create unix socket
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("manager: assign_job: socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the worker
    struct sockaddr_un servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
#ifdef __APPLE__
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path),
        "/Users/leejahsprock/pyoneer/run/worker%d.socket",
        man->crew.freelist->worker.id);
#elif __linux__
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path),
        "/run/pyoneer/worker%d.socket", man->crew.freelist->worker.id);
#endif
    if (connect(sockfd, &servaddr, sizeof(servaddr)) == -1) {
        perror("manager: worker_thread: connect");
        exit(EXIT_FAILURE);
    }

    char *cmd = "{\"command\":\"run_job\",\"job\":}"; // Stopping here to add encode_job method to Job class 

    unlock:
    if ((old_errno = pthread_mutex_unlock(&man->crew.lock)) != 0) {
        fprintf(stderr, "manager: assing_job: pthread_mutex_unlock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    
}

/* project_queue_handler: Frees all memory allocated to the queue. */
static void *running_project_handler(void *arg) {
    free_running_project((struct queue *) arg);
    return NULL;
}

/* project_thread: Runs the project. */
static void *project_thread(void *args) {
    // Unpack args
    RunningProject *rproj = (RunningProject *) args;
    Project *proj = rproj->project;
    Manager *man = rproj->manager;

    // Set manager and project status
    man->status = _MANAGER_WORKING;
    proj->status = _PROJECT_RUNNING;


    // Add queue handler
    pthread_cleanup_push(running_project_handler, rproj);

    RunningProjectNode *curr;
    while (sleep(1) != -1) {
        // Schedule
        schedule(rproj);
        for (curr = rproj->not_ready_jobs->head; curr; curr = curr->next) {
            if (is_ready(curr) == _JOB_READY)
                add_node(rproj->ready_jobs, remove_node(rproj->not_ready_jobs, curr));
        }
        
        // Assign


        // Finalize

    }

    pthread_cleanup_pop(0);
    return NULL;
}

/* get_pnode: Gets the node from the project and returns it. */
static Job *get_pnode(Project *proj, int id) {
    ProjectNode *ent;
    for (ent = proj->jobs_table[id % MAXLEN]; ent->job->id != id; ent = ent->next_ent)
        ;
    return ent;
}

/* run_project: Runs the project and returns 0. If the manager is working
    run_project returns -1. */
int run_project(Manager *man, Project *proj) {
    // Audit project

    // Create running project
    RunningProject *rproj = create_rproj();

    // Create job phase queues
    struct queue *not_ready_jobs = create_queue();
    struct queue *ready_jobs = create_queue();
    struct queue *running_jobs = create_queue();
    struct queue *completed_jobs = create_queue();
    struct queue *incomplete_jobs = create_queue();

    for (ProjectNode *pn = proj->jobs_list->head; pn; pn = pn->next) {
        RunningProjectNode *node;
        if ((node = malloc(sizeof(RunningProjectNode))) == NULL) {
            perror("manager: run_project: malloc");
            exit(EXIT_FAILURE);
        }
        node->project_node = pn;

        if ((node->deps = malloc(sizeof(RunningProjectNode *) * pn->len)) == NULL) {
            perror("manager: run_project: malloc");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < pn->len; i++)
            node->deps[i] = get_pnode(proj, pn->deps[i]);

        switch (pn->job->status) {
            case _JOB_NOT_READY:
                add_node(not_ready_jobs, node);
                break;
            case _JOB_READY:
                add_node(ready_jobs, node);
                break;
            case _JOB_RUNNING:
                add_node(running_jobs, node);
                break;
            case _JOB_COMPLETED:
                add_node(completed_jobs, node);
                break;
            case _JOB_INCOMPLETE:
                add_node(incomplete_jobs, node);
                break;
        }
    }
    
    // Create project thread

    return 0;
}
