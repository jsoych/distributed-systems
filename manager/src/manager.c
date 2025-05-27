#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "manager.h"

#define BUFLEN 1024


/* json_get_value: Gets the value from the JSON object with its name and
    returns it. Otherwise, returns NULL. */
static json_value *json_get_value(json_value *obj, const char *name) {
    if (obj->type != json_object)
        return NULL;
    for (int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, name) == 0)
            return obj->u.object.values[i].value;
    }
    return NULL;
}

/* job_status_map: Maps the job status string returns its corresponding status
    code. Otherwise, returns -1. */
static job_status job_status_map(char *status) {
    if (strcmp(status, "running") == 0)
        return job_running;
    else if (strcmp(status, "completed") == 0)
        return job_completed;
    else if (strcmp(status, "incomplete") == 0)
        return job_incomplete;
    else
        return -1;
}

/* worker_status_map: Maps the worker status string returns its corresponding
    status code. Otherwise, returns -1. */
static worker_status worker_status_map(char *status) {
    if (strcmp(status, "not_working") == 0)
        return worker_not_working;
    else if (strcmp(status, "working") == 0)
        return worker_working;
    else
        return -1;
}

/* create_worker: Creates a new worker. */
static Worker *create_worker(int id) {
    Worker *worker;
    if ((worker = malloc(sizeof(Worker))) == NULL) {
        perror("manager: create_worker: malloc");
        exit(EXIT_FAILURE);
    }
    worker->id = id;
    worker->status = worker_not_assigned;
    worker->job.id = -1;
    worker->job.status = -1;
    return worker;
}

/* send_command: Sends a command to the worker and returns its response. */
static json_value *send_command(Worker *worker, json_value *cmd) {
    int sockfd, nbytes;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("manager: send_command: socket");
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
        perror("manager: send_command: connect");
        exit(EXIT_FAILURE);
    }

    // Send command
    char buf[BUFLEN];
    if (json_measure(cmd) > BUFLEN) {
        fprintf(stderr, "manager: send_command: Error: Serialized command to large for buffer\n");
        close(sockfd);
        return NULL;
    }
    json_serialize(buf, cmd);

    if (send(sockfd, buf, strlen(buf), 0) == -1) {
        perror("manager: send_command: send");
        exit(EXIT_FAILURE);
    }

    // Return response
    if ((nbytes = recv(sockfd, buf, BUFLEN, 0)) == -1) {
        perror("manager: send_command: recv");
        exit(EXIT_FAILURE);
    }
    buf[nbytes >= BUFLEN ? BUFLEN - 1 : nbytes] = '\0'; 

    json_value *res;
    if ((res = json_parse(buf, strlen(buf))) == NULL) {
        fprintf(stderr, "manager: send_comand: Error: Unable to parse response\n");
    }
    close(sockfd);
    return res;
}

/* free_worker: Frees all resources allocated to the worker. */
static void free_worker(Worker *worker) {
    // Stop worker
    json_value *cmd, *res;
    char *stop = "{\"command\":\"stop\"}";
    if (worker->status == worker_working) {
        cmd = json_parse(stop, strlen(stop));
        res = send_command(worker, cmd);
        json_value_free(cmd);
        json_value_free(res);
    }
    free(worker);
    return;
}

/* create_crew: Creates a new crew. */
static Crew *create_crew(void) {
    Crew *crew;
    if ((crew = malloc(sizeof(Crew))) == NULL) {
        perror("manager: create_crew: malloc");
        exit(EXIT_FAILURE);
    }
    crew->workers.head = NULL;
    crew->workers.tail = NULL;
    crew->workers.len = 0;
    crew->freelist.head = NULL;
    crew->freelist.tail = NULL;
    crew->freelist.len = 0;
    int old_errno;
    if ((old_errno = pthread_mutex_init(&crew->lock, NULL)) != 0) {
        fprintf(stderr, "manager: create_crew: pthread_mutex_init: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return crew;
}

/* freelist_push: Adds the node to the head of the crew freelist. */
static void freelist_push(Crew *crew, crew_node *node) {
    node->prev_free = NULL;
    if (crew->freelist.len == 0) {
        node->next_free = NULL;
        crew->freelist.head = node;
        crew->freelist.tail = node;
        crew->freelist.len++;
        return;
    }
    node->next_free = crew->freelist.head;
    crew->freelist.head->prev_free = node;
    crew->freelist.head = node;
    crew->freelist.len++;
    return;
}

/* freelist_pop: Pops the first node from the crew freelist and returns it.
    Otherwise, returns NULL. */
static crew_node *freelist_pop(Crew *crew) {
    if (crew->freelist.len == 0) {
        return NULL;
    }

    crew_node *node = crew->freelist.head;
    crew->freelist.len--;
    crew->freelist.head = node->next_free;
    node->next_free = NULL;
    node->prev_free = NULL;
    if (crew->freelist.len == 0) {
        crew->freelist.tail = NULL;
        return node;
    }
    crew->freelist.head->next_free = NULL;
    return node;
}

/* freelist_append: Adds the node to the tail of the crew freelist. */
static void freelist_append(Crew *crew, crew_node *node) {
    node->next_free = NULL;
    if (crew->freelist.len == 0) {
        node->prev_free = NULL;
        crew->freelist.head = node;
        crew->freelist.tail = node;
        crew->freelist.len++;
        return;
    }
    node->prev_free = crew->freelist.tail;
    crew->freelist.tail->next_free = node;
    crew->freelist.tail = node;
    crew->freelist.len++;
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

    // Update worker status and job status
    int nbyte, status;
    char buf[BUFLEN];
    char *get_status = "{\"command\":\"get_status\"}";
    char *get_job_status = "{\"command\":\"get_job_status\"}";
    json_value *res, *val;
    pthread_cleanup_push(worker_socket_handler, &sockfd);
    while (sleep(1) == 0) {
        if (worker->status == worker_not_assigned)
            continue;

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
            continue;
        }
        
        if ((val = json_get_value(res, "status")) == NULL) {
            fprintf(stderr, "manager: worker_thread: json_get_value: Error: Missing JSON value\n");
            json_value_free(val);
            goto get_job_status;
        }

        // Update worker status
        if ((status = worker_status_map(val->u.string.ptr)) == -1) {
            fprintf(stderr, "manager: worker_thread: worker_status_map: Error: Unknown worker status\n");
            json_value_free(val);
            goto get_job_status;
        }
        worker->status = status;
        json_value_free(res);

        // Get worker job status
        get_job_status:
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
            continue;
        }

        if ((val = json_get_value(res, "job_status")) == NULL) {
            fprintf(stderr, "manager: worker_thread: json_get_value: Error: Missing JSON value\n");
            json_value_free(res);
            continue;
        }

        if ((status = job_status_map(val->u.string.ptr)) == -1) {
            fprintf(stderr, "manager: worker_thread: job_status_map: Error: Unknown worker status\n");
            json_value_free(res);
            continue;
        }
        worker->job.status = status;
        json_value_free(res);
    }

    pthread_cleanup_pop(0);
    return NULL;
}

/* add_worker: Creates a workers and adds it to the crew. */
void add_worker(Crew *crew, int id) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "manager: add_worker: pthread_mutex_lock: %s\n",
            strerror(old_errno));
        return;
    }
    
    crew_node *node;
    if ((node = malloc(sizeof(crew_node))) == NULL) {
        perror("manager: add_worker: malloc");
        exit(EXIT_FAILURE);
    }
    node->worker = create_worker(id);
    node->next = NULL;
    node->next_free = NULL;

    if (crew->workers.len == 0) {
        node->prev = NULL;
        node->prev_free = NULL;
        crew->workers.head = node;
        crew->workers.tail = node;
        crew->workers.len++;
        crew->freelist.head = node;
        crew->freelist.tail = node;
        crew->freelist.len++;
        goto unlock;
    }

    // Check id against all worker ids
    crew_node *curr = crew->workers.head;
    while (curr) {
        if (curr->worker->id == id) {
            fprintf(stderr, "manager: add_worker: Warning: Trying to add a worker with the same id\n");
            free_worker(node->worker);
            free(node);
            goto unlock;
        }
    }

    // Add the new worker to the tail of the workers list
    node->prev = crew->workers.tail;
    crew->workers.tail->next = node;
    crew->workers.tail = node;
    crew->workers.len++;

    // Add the new worker to the tail of the freelist
    node->prev_free = crew->freelist.tail;
    crew->freelist.tail->next_free = node;
    crew->freelist.tail = node;
    crew->freelist.len++;

    // Create worker thread
    if ((old_errno = pthread_create(&node->tid, NULL, worker_thread, &node->worker)) != 0) {
            fprintf(stderr, "manager: add_worker: pthread_create: %s\n", strerror(old_errno));
            exit(EXIT_FAILURE);
    }

    unlock:
    if ((old_errno = pthread_mutex_unlock(&crew->lock)) != 0) {
        fprintf(stderr, "manager: add_worker: pthread_mutex_unlock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* free_crew: Frees all the resources allocated to the crew. */
static void free_crew(Crew *crew) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "manager: free_crew: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    
    if (crew->workers.len == 0)
        goto unlock;

    crew_node *curr = crew->workers.head;
    while (curr->next) {
        curr = curr->next;
        free_worker(curr->prev->worker);
        free(curr->prev);
    }
    free_worker(curr->worker);
    free(curr);

    unlock:
    if ((old_errno = pthread_mutex_unlock(&crew->lock)) != 0) {
        fprintf(stderr, "manager: free_crew: pthread_mutex_unlock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    if ((old_errno = pthread_mutex_destroy(&crew->lock)) != 0) {
        fprintf(stderr, "manager: free_crew: pthread_mutex_destroy: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    free(crew);
    return;
}

/* init_queue: Initializes the queue. */
static void init_queue(queue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->len = 0;
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
static RunningProjectNode *remove_node(queue *q, RunningProjectNode *n) {
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

/* destroy_queue: Frees all queue resources. */
static void destroy_queue(queue *q) {
    if (q->len == 0) {
        return;
    }
    RunningProjectNode *curr = q->head;
    while (curr->next) {
        curr = curr->next;
        free(curr->prev->deps);
        free(curr->prev);
    }
    free(curr->deps);
    free(curr);
    return;
}

/* get_job: Gets the job from the project and returns it. */
static Job *get_job(Project *proj, int id) {
    ProjectNode *ent =  proj->jobs_table[id % MAXLEN]; 
    while (ent->job->id != id)
        ent = ent->next_ent;
    return ent->job;
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
    init_queue(&rproj->not_ready_jobs);
    init_queue(&rproj->ready_jobs);
    init_queue(&rproj->running_jobs);
    init_queue(&rproj->completed_jobs);
    init_queue(&rproj->incomplete_jobs);

    int i;
    RunningProjectNode *node;
    for (ProjectNode *pn = proj->jobs.head; pn; pn = pn->next) {
        if ((node = malloc(sizeof(RunningProjectNode))) == NULL) {
            perror("manager: run_project: malloc");
            exit(EXIT_FAILURE);
        }
        node->job = pn->job;

        if ((node->deps = malloc(sizeof(RunningProjectNode *) * pn->len)) == NULL) {
            perror("manager: run_project: malloc");
            exit(EXIT_FAILURE);
        }
        for (i = 0; i < pn->len; i++) {
            node->deps[i] = get_job(proj, pn->deps[i]);
        }
        node->len = pn->len;

        switch (pn->job->status) {
            case _JOB_NOT_READY:
                add_node(&rproj->not_ready_jobs, node);
                break;
            case _JOB_READY:
                add_node(&rproj->ready_jobs, node);
                break;
            case _JOB_RUNNING:
                add_node(&rproj->running_jobs, node);
                break;
            case _JOB_COMPLETED:
                add_node(&rproj->completed_jobs, node);
                break;
            case _JOB_INCOMPLETE:
                add_node(&rproj->incomplete_jobs, node);
                break;
        }
    }
    return rproj;
}

/* free_running_project: Frees all resources allocated to the running
    project. */
static void free_running_project(RunningProject *rproj) {
    destroy_queue(&rproj->not_ready_jobs);
    destroy_queue(&rproj->ready_jobs);
    destroy_queue(&rproj->running_jobs);
    destroy_queue(&rproj->completed_jobs);
    destroy_queue(&rproj->incomplete_jobs);
    free(rproj);
    return;
}

/* create_manager: Creates a new manager. */
Manager *create_manager(int id) {
    Manager *man;
    if ((man = malloc(sizeof(Manager))) == NULL) {
        perror("manager: malloc");
        exit(EXIT_SUCCESS);
    }
    man->id = id;
    man->status = _MANAGER_NOT_WORKING;
    man->crew = create_crew();
    man->running_project = create_running_project(man, NULL);
    return man;
}

/* get_status: Gets the manager status. */
int get_status(Manager *man) {
    return man->status;
}

/* get_project_status: Gets the status of the running project. */
int get_project_status(Manager *man) {
    if (man->running_project->project == NULL)
        return -1;
    return man->running_project->project->status;
}

/* assign: Assigns a job to a worker in the crew and returns 0. Otherwise,
    returns -1. */
static int assign_job(Manager *man, Job *job) {
    int old_errno, nbytes, sockfd, ret;
    if ((old_errno = pthread_mutex_lock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: assign_job: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    if (man->crew->freelist.len == 0) {
        ret = -1;
        goto unlock;
    }

    crew_node *node = freelist_pop(man->crew);

    // Create json object
    json_settings settings;
    settings.value_extra = json_builder_extra;
    
    char error[128];
    json_value *res, *val;
    if ((val = json_parse_ex(&settings, "{\"command\":\"run_job\"}", 22, error)) == NULL) {
        fprintf(stderr, "manager: assign_job: json_parse: %s\n", error);
        freelist_push(man->crew, node);
        ret = -1;
        goto unlock;
    }
    json_object_push(val, "job", encode_job(job));
    
    if ((res = send_command(node->worker, val)) == NULL) {
        fprintf(stderr, "manager: assign_jobL send_command: Missing response\n");
        freelist_push(man->crew, node);
        json_builder_free(val);
        ret = -1;
        goto unlock;
    }
    json_builder_free(val);

    // Update worker status
    if ((val = json_get_value(res, "status")) == NULL) {
        fprintf(stderr, "manager: assign_job: json_get_value: Error: Missing status value\n");
        freelist_push(&man->crew, node);
        json_builder_free(res);
        ret = -1;
        goto unlock;
    }
    node->worker->status = worker_status_map(val->u.string.ptr);

    // Update job status
    if ((val = json_get_value(res, "job_status")) == NULL) {
        fprintf(stderr, "manager: assign_job: json_get_value: Error: Missing job_status value\n");
        freelist_push(&man->crew, node);
        json_builder_free(res);
        ret = -1;
        goto unlock;
    }
    node->worker->job.status = job_status_map(val->u.string.ptr);
    json_builder_free(res);
    ret = 0;

    unlock:
    if ((old_errno = pthread_mutex_unlock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: assing_job: pthread_mutex_unlock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return ret;
}

/* unassign_worker: Unassigns the worker. */
static void unassign_worker(Manager *man, int worker_id) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: unassign_worker: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    if (man->crew->workers.len == 0) {
        fprintf(stderr, "manager: unassign_worker: Warning: No workers to unassign\n");
        goto unlock;
    }

    crew_node *curr = man->crew->workers.head;
    while (curr) {
        if (curr->worker->id == worker_id)
            break;
        curr = curr->next;
    }
    if (curr == NULL) {
        fprintf(stderr, "manager: unassign_worker: Warning: Trying to unassign worker with id %d\n", worker_id);
        goto unlock;
    }


    if (curr->worker->status == worker_not_assigned) {
        fprintf(stderr, "manager: unassign_worker: Warning: Trying to unassign a worker that is not assigned yet\n");
        goto unlock;
    }

    char *stop = "{\"command\":\"stop\"}";
    json_value *cmd = json_parse(stop, strlen(stop));
    if (curr->worker->status == worker_working) {
        cmd = json_parse(stop, strlen(stop));
        json_value_free(send_command(curr->worker, cmd));
    }
    json_value_free(cmd);
    freelist_append(man->crew, curr);

    unlock:
    if ((old_errno = pthread_mutex_unlock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: unassign_worker: pthread_mutex_unlock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* sync_project: Synchronizes the project job statuses with the crew job
    job statuses. */
static void sync_project(Manager *man) {
    Project *proj = man->running_project->project;
    
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: update_project_status: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    
    Job *job;
    crew_node *curr = man->crew->workers.head;
    while (curr) {
        if (curr->worker->status == worker_not_assigned) {
            curr = curr->next;
            continue;
        }

        job = get_job(proj, curr->worker->job.id);
        switch (curr->worker->job.status) {
            case job_running:
                job->status = _JOB_RUNNING;
                break;
            case job_completed:
                job->status = _JOB_COMPLETED;
                break;
            case job_incomplete:
                job->status = _JOB_INCOMPLETE;
                break;
        }
        curr = curr->next;
    }

    if((old_errno = pthread_mutex_unlock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: update_project_status: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* status_handler: */
static void *status_handler(void *arg) {
    Manager *man = (Manager *) arg;
    if (man->status == _MANAGER_NOT_WORKING)
        return NULL;
    if (man->running_project->project->status == _PROJECT_RUNNING)
        man->running_project->project->status = _PROJECT_INCOMPLETE;
    man->status = _MANAGER_NOT_WORKING;
    return NULL;
}

/* stop_workers_handler: Stops all working workers. */
static void *stop_workers_handler(void *arg) {
    Manager *man = (Manager *) arg;

    // For each working worker send a command to the worker to stop
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: stop_workers_handler: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    char *stop = "{\"command\":\"stop\"}";
    json_value *cmd = json_parse(stop, strlen(stop));
    crew_node *curr = man->crew->workers.head;
    while (curr) {
        if ((curr->worker->status == worker_working))
            json_value_free(send_command(curr->worker, cmd));
        curr = curr->next;
    }
    json_value_free(cmd);

    if ((old_errno = pthread_mutex_unlock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: stop_workers_handler: pthread_mutex_unlock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return NULL;
}

/* get_job_status: Get the status of the job. */
static int get_job_status(RunningProjectNode *node) {
    if (node->job->status != _JOB_NOT_READY) {
        return node->job->status;
    }

    // Check the status of each dependency
    for (int i = 0; i < node->len; i++) {
        if (node->deps[i]->status != _JOB_COMPLETED)
            return _JOB_NOT_READY;
    }
    return (node->job->status = _JOB_READY);
}

/* project_thread: Runs the project. */
static void *project_thread(void *args) {
    // Unpack args
    RunningProject *rproj = (RunningProject *) args;
    Manager *man = rproj->manager;
    Project *proj = rproj->project;
    proj->status = _PROJECT_RUNNING;

    // Add cleanup handlers
    pthread_cleanup_push(status_handler, man);
    pthread_cleanup_push(stop_workers_handler, man);

    // Run project
    RunningProjectNode *curr;
    while (sleep(1) != -1) {
        // Synchronzie project with crew
        sync_project(man);

        // Schedule not ready jobs
        for (curr = rproj->not_ready_jobs.head; curr; curr = curr->next) {
            switch (get_job_status(curr)) {
                case _JOB_READY:
                    add_node(&rproj->ready_jobs, remove_node(&rproj->not_ready_jobs, curr));
                    break;
                default:
                    fprintf(stderr, "manager: project_thread: Warning: Schedule is in inconsitent state\n");
                    break;
            }
        }
        
        // Assign ready jobs
        for (curr = rproj->ready_jobs.head; curr; curr = curr->next) {
            switch (get_job_status(curr)) {
                case _JOB_READY:
                    assign_job(man, curr->job);
                    break;
                case _JOB_RUNNING:
                    add_node(&rproj->running_jobs, remove_node(&rproj->ready_jobs, curr));
                    break;
                default:
                    fprintf(stderr, "manager: project_thread: Warning: Schedule is in inconsitent state\n");
                    break;
            }
        }

        // Check running jobs
        for (curr = rproj->running_jobs.head; curr; curr = curr->next) {
            switch (get_job_status(curr)) {
                case _JOB_COMPLETED:
                    add_node(&rproj->completed_jobs, remove_node(&rproj->incomplete_jobs, curr));
                    unassign_worker(man, curr->worker_id);
                    break;
                case _JOB_INCOMPLETE:
                    add_node(&rproj->incomplete_jobs, remove_node(&rproj->running_jobs, curr));
                    unassign_worker(man, curr->worker_id);
                    break;
                default:
                    fprintf(stderr, "manager: project_thread: Warning: Schedule is in inconsitent state\n");
                    break;             
            }
        }

        // Update project status
        if (rproj->incomplete_jobs.len > 0) {
            proj->status = _PROJECT_INCOMPLETE;
            break;
        } else if (rproj->completed_jobs.len = proj->len) {
            proj->status = _PROJECT_COMPLETED;
            break;
        }
        
        // Sanity check
        if ((rproj->not_ready_jobs.len + rproj->ready_jobs.len + rproj->running_jobs.len + rproj->completed_jobs.len + rproj->incomplete_jobs.len) == proj->len) {
            fprintf(stderr, "manager: project_thread: Error: Missing node\n");
            break;
        }
    }
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    return NULL;
}

/* run_project: Runs the project and returns 0. If the manager is working
    run_project returns -1. */
int run_project(Manager *man, Project *proj) {
    if (proj->status != _PROJECT_READY)
        return -1;

    // Audit project
    if (audit_project(proj) != -1) {
        fprintf(stderr, "manager: run_project: audit_project: Project has circular dependencies\n");
        proj->status = _PROJECT_NOT_READY;
        return -1;
    }

    // Create project thread
    RunningProject *rproj = create_running_project(man, proj);    
    int old_errno;
    if ((old_errno = pthread_create(&rproj->tid, NULL, project_thread, rproj)) != 0) {
        fprintf(stderr, "manager: run_project: pthread_create: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    man->status = _MANAGER_WORKING;
    return 0;
}

/* start: Starts the running project and returns the project status.
    If the running project is NULL, start returns -1. */
int start(Manager *man) {
    if (man->running_project == NULL)
        return -1;
    
    if (man->status == _MANAGER_WORKING)
        return man->running_project->project->status;
    
    int old_errno;
    if (man->running_project->project->status == _PROJECT_READY) {
        if ((old_errno = pthread_create(&man->running_project->tid, NULL, project_thread, man->running_project)) != 0) {
            fprintf(stderr, "manager: start: %s\n", strerror(old_errno));
            exit(EXIT_FAILURE);
        }
    }
    return man->running_project->project->status;
}

/* stop: Stops the running project and returns the project status. If
    the running project is NULL, stop returns -1. */
int stop(Manager *man) {
    if (man->running_project == NULL)
        return -1;
    
    if (man->status == _MANAGER_NOT_WORKING)
        return man->running_project->project->status;
    
    int old_errno;
    if ((old_errno = pthread_cancel(man->running_project->tid)) != 0) {
        fprintf(stderr, "manager: stop: %s\n", strerror(old_errno));
    }
    return man->running_project->project->status;
}


/* free_manager: Frees the memory allocated to the manager. */
void free_manager(Manager *man) {
    if (man->status == _MANAGER_WORKING)
        stop(man);
    free_crew(man->crew);
    free_running_project(man->running_project);
    free(man);
    return;
}
