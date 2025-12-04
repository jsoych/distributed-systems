#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "crew.h"

#define BUFLEN 1024

// Api commands
static enum {
    CMD_RUN = 0,
    CMD_GET_STATUS,
    CMD_GET_BLUEPRINT_STATUS,
    CMD_ASSIGN,
    CMD_UNASSIGN
};

static const char* const CMD_API_JSON[] = {
    [CMD_RUN]                   = "{\"command\":\"run\"}",
    [CMD_GET_STATUS]            = "{\"command\":\"get_status\"}",
    [CMD_GET_BLUEPRINT_STATUS]  = "{\"command\":\"get_blueprint\"}",
    [CMD_UNASSIGN]              = "{\"command\":\"unassign\"}"
};

/* send_command: Sends a command to the worker and returns its response. */
static json_value* send_command(Worker *worker, json_value *cmd) {
    int sockfd, nbytes;
    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("crew: send_command: socket");
        return NULL;
    }

    // Connect to the worker
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_LOCAL;

    // TODO: do something better please (':
    char* dir = getenv("PYONEER_DIR");
    int size = sizeof(addr.sun_path);
    char* fmt[size];
    char* end = stpncpy(fmt, dir, size);
    stpncpy(end, "/worker%d.sock", size);
    snprintf(addr.sun_path, size, fmt, worker->id);

    if (connect(sockfd, &addr, sizeof(addr)) == -1) {
        perror("crew: send_command: connect");
        return NULL;
    }

    // Send command
    char buf[BUFLEN];
    if (json_measure(cmd) > BUFLEN) {
        fprintf(stderr, "crew: send_command: Error: Serialized command to large for buffer\n");
        close(sockfd);
        return NULL;
    }
    json_serialize(buf, cmd);

    if (send(sockfd, buf, strlen(buf), 0) == -1) {
        perror("crew: send_command: send");
        close(sockfd);
        return NULL;
    }

    // Return response
    if ((nbytes = recv(sockfd, buf, BUFLEN, 0)) == -1) {
        perror("crew: send_command: recv");
        exit(EXIT_FAILURE);
    }
    buf[nbytes >= BUFLEN ? BUFLEN - 1 : nbytes] = '\0'; 

    json_value *res;
    if ((res = json_parse(buf, strlen(buf))) == NULL) {
        fprintf(stderr, "crew: send_comand: Error: Unable to parse response\n");
    }
    close(sockfd);
    return res;
}

/* mutex_lock: Locks the mutex lock. If there is a system failure, mutex_lock
    prints a error message and exits the process. */
static void mutex_lock(pthread_mutex_t *lock, char *name) {
    int err = pthread_mutex_lock(lock);
    if (err != 0) {
        fprintf(stderr, "crew: %s: %s\n", name, strerror(err));
        exit(EXIT_FAILURE);
    }
    return;
}

/* mutex_unlock: Unlocks the mutex lock. If there is a system failue, 
    mutex_unlock prints a error and exits the process. */
static void mutex_unlock(pthread_mutex_t *lock, char *name) {
    int err = pthread_mutex_unlock(lock);
    if (err != 0) {
        fprintf(stderr, "crew: %s: %s\n", name, strerror(err));
        exit(EXIT_FAILURE);
    }
    return;
}

/* crew_worker_create: Creates a new worker. */
static crew_worker* crew_worker_create(int id) {
    crew_worker* worker = malloc(sizeof(crew_worker));
    if (worker == NULL) {
        perror("crew_worker_create: malloc");
        return NULL;
    }
    worker->id = id;
    worker->status = WORKER_NOT_ASSIGNED;
    worker->job.id = -1;
    worker->job.status = -1;
    return worker;
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

/* init_crew_list: Initializes the list. */
static void init_crew_list(crew_list *l) {
    l->head = NULL;
    l->tail = NULL;
    l->len = 0;
    return;
}

/* free_crew_list: Frees the memory allocated to the list. */
static void free_crew_list(crew_list *l) {
    if (l->len == 0)
        return;
    
    l->len = 0;
    crew_node *curr = l->head;
    while (curr->next) {
        curr = curr->next;
        free(curr->prev);
        free_worker(curr->prev->worker);
    }
    free_worker(curr->worker);
    free(curr);
    l->head = NULL;
    l->tail = NULL;
    return;
}

/* in_crew_list: Checks if the id is in the crew_list and returns true, if the
    the id equals one of the worker's id. Otherwise, returns false. */
static bool in_crew_list(crew_list *l, int id) {
    crew_node *curr = l->head;
    while (curr) {
        if (curr->worker->id == id)
            return true;
        curr = curr->next;
    }
    return false;
}

/* get_crew_node: Gets the node from the list by its worker id and returns it.
    Otherwise, returns NULL. */
static crew_node *get_crew_node(crew_list *l, int id) {
    if (l->len == 0)
        return NULL;
    
    crew_node *curr = l->head;
    while (curr) {
        if (curr->worker->id == id)
            return curr;
        curr = curr->next;
    }
    return NULL;
}

/* create_crew: Creates a new crew. */
Crew *create_crew(void) {
    Crew *crew;
    if ((crew = malloc(sizeof(Crew))) == NULL) {
        perror("crew: create_crew: malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < _CREW_MAXLEN; ++i)
        init_crew_list(&crew->workers[i]);
    crew->len = 0;
    crew->freelist.head = NULL;
    crew->freelist.tail = NULL;
    crew->freelist.len = 0;
    int err;
    if ((err = pthread_mutex_init(&crew->lock, NULL)) != 0) {
        fprintf(stderr, "crew: create_crew: pthread_mutex_init: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    return crew;
}

/* free_crew: Frees all the resources allocated to the crew. */
void free_crew(Crew *crew) {
    int err;
    if ((err = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: free_crew: pthread_mutex_lock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    
    if (crew->len == 0)
        goto unlock;

    crew->len = 0;
    for (int i = 0; i < _CREW_MAXLEN; i++)
        free_crew_list(&crew->workers[i]);

    unlock:
    if ((err = pthread_mutex_unlock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: free_crew: pthread_mutex_unlock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = pthread_mutex_destroy(&crew->lock)) != 0) {
        fprintf(stderr, "crew: free_crew: pthread_mutex_destroy: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    free(crew);
    return;
}

/* in_crew: Checks if the id is in the crew and returns true, if the id
    equals one crew worker's id. Otherwise, returns false. */
static bool in_crew(Crew *crew, int id) {
    return in_crew_list(&crew->workers[id % _CREW_MAXLEN], id);
}

/* get_worker: Gets the worker by its id from the crew and returns it.
    Otherwise, returns NULL. */
static Worker *get_worker(Crew *crew, int id) {
    if (in_crew(crew, id) == false)
        return NULL;
    return get_crew_node(&crew->workers[id % _CREW_MAXLEN], id)->worker;
}

/* freelist_push: Adds the node to the head of the crew freelist. */
static void freelist_push(Crew *crew, int id) {
    if (in_crew(crew, id) == false) {
        fprintf(stderr, "crew: freelist_push: warning: trying to push invalid worker id to the freelist\n");
        return;
    }

    crew_node *node = get_crew_node(&crew->workers[id % _CREW_MAXLEN], id);
    node->prev_free = NULL;
    if (crew->freelist.len++ == 0) {
        node->next_free = NULL;
        crew->freelist.head = node;
        crew->freelist.tail = node;
        return;
    }
    node->next_free = crew->freelist.head;
    crew->freelist.head->prev_free = node;
    crew->freelist.head = node;
    return;
}

/* freelist_pop: Pops the first worker from the crew freelist and returns its id.
    Otherwise, returns -1. Remark: Not thread safe. */
static int freelist_pop(Crew *crew) {
    if (crew->freelist.len == 0)
        return -1;

    crew_node *node = crew->freelist.head;
    if (crew->freelist.len-- == 1) {
        crew->freelist.head = NULL;
        crew->freelist.tail = NULL;
        node->next_free = NULL;
        return node->worker->id;
    }
    crew->freelist.head = node->next_free;
    crew->freelist.head->prev_free = NULL;
    node->next_free = NULL;
    return node->worker->id;
}

/* freelist_append: Adds the worker to the tail of the crew freelist. */
static void freelist_append(Crew *crew, int id) {
    if (in_crew(crew, id) == false) {
        fprintf(stderr, "crew: freelist_append: warning: trying to add invalid worker id to the freelist\n");
        return;
    }

    crew_node *node = get_crew_node(&crew->workers[id % _CREW_MAXLEN], id);
    node->next_free = NULL;
    if (crew->freelist.len++ == 0) {
        node->prev_free = NULL;
        crew->freelist.head = node;
        crew->freelist.tail = node;
        return;
    }
    node->prev_free = crew->freelist.tail;
    crew->freelist.tail->next_free = node;
    crew->freelist.tail = node;
    return;
}

/* worker_socket_handler: Closes the socket. */
static void *worker_socket_handler(void *arg) {
    if (close(*(int *)arg) == -1) {
        perror("crew: worker_socket_handler: close");
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
        perror("crew: worker_thread: socket");
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
        "/home/jsoychak/pyoneer/run/worker%d.socket", worker->id);
#endif

    if (connect(sockfd, &servaddr, sizeof(servaddr)) == -1) {
        perror("crew: worker_thread: connect");
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
        // Get worker status
        if (send(sockfd, get_status, strlen(get_status), 0) == -1) {
            perror("crew: worker_thread: send");
            exit(EXIT_FAILURE);
        }

        if ((nbyte = recv(sockfd, &buf, BUFLEN, 0)) == -1) {
            perror("crew: worker_thread: recv");
            exit(EXIT_FAILURE);
        }
        buf[nbyte > BUFLEN ? BUFLEN - 1 : nbyte] = '\0';

        if ((res = json_parse(buf, strlen(buf))) == NULL) {
            fprintf(stderr, "crew: worker_thread: json_parse: Unable to parse response\n");
            continue;
        }
        
        if ((val = json_get_value(res, "status")) == NULL) {
            fprintf(stderr, "crew: worker_thread: json_get_value: Error: Missing JSON value\n");
            json_value_free(res);
            continue;
        }

        // Update worker status
        if ((status = worker_status_map(val->u.string.ptr)) == -1) {
            fprintf(stderr, "crew: worker_thread: worker_status_map: Error: Unknown worker status\n");
            json_value_free(res);
            continue;
        }
        worker->status = status;
        json_value_free(res);

        if (worker->status == worker_not_assigned)
            continue;

        // Get worker job status
        if (send(sockfd, get_job_status, strlen(get_job_status), 0) == -1) {
            perror("crew: worker_thread: send");
            exit(EXIT_FAILURE);
        }

        if ((nbyte = recv(sockfd, &buf, BUFLEN, 0)) == -1) {
            perror("crew: worker_thread: recv");
            exit(EXIT_FAILURE);
        }
        buf[nbyte > BUFLEN ? BUFLEN - 1 : nbyte] = '\0';

        if ((res = json_parse(buf, strlen(buf))) == NULL) {
            fprintf(stderr, "crew: worker_thread: json_parse: Unable to parse response\n");
            continue;
        }

        if ((val = json_get_value(res, "job_status")) == NULL) {
            fprintf(stderr, "crew: worker_thread: json_get_value: Error: Missing JSON value\n");
            json_value_free(res);
            continue;
        }

        switch (val->type) {
            case json_string:
                status = job_status_map(val->u.string.ptr);
                break;
            case json_null:
                status = -1;
                break;
            default:
                status = -1;
                break;
        }
        worker->job.status = status;
        json_value_free(res);
    }

    pthread_cleanup_pop(0);
    return NULL;
}

/* add_worker: Creates a workers and adds it to the crew. */
int add_worker(Crew *crew, int id) {
    mutex_lock(&crew->lock, "add_worker");

    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    if (in_crew_list(list, id) == true) {
        mutex_unlock(&crew->lock, "add_worker");
        return -1;
    }

    crew_node *node;
    if ((node = malloc(sizeof(crew_node))) == NULL) {
        perror("crew: add_crew_worker: malloc");
        exit(EXIT_FAILURE);
    }
    node->worker = crew_worker_create(id);

    // create worker thread
    int err;
    if ((err = pthread_create(&node->tid, NULL, worker_thread, node->worker)) != 0) {
        fprintf(stderr, "crew: add_worker: pthread_create: %s\n", strerror(err));
        mutex_unlock(&crew->lock, "add_worker");
        return -1;
    }

    crew->len++;
    node->next = NULL;
    node->next_free = NULL;

    // add the node to the end of the list
    if (list->len++ == 0) {
        node->prev = NULL;
        list->head = node;
        list->tail = node;
    } else {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }

    // add the node to the end of the freelist
    if (crew->freelist.len++ == 0) {
        node->prev_free = NULL;
        crew->freelist.head = node;
        crew->freelist.tail = node;
    } else {
        node->prev_free = crew->freelist.tail;
        crew->freelist.tail->next_free = node;
        crew->freelist.tail = node;
    }

    mutex_unlock(&crew->lock, "add_worker");
    return 0;
}

/* remove_worker: Removes a worker from the crew with its id. */
int remove_worker(Crew *crew, int id) {
    mutex_lock(&crew->lock, "remove_worker");
    if (in_crew(crew, id) == false) {
        mutex_unlock(&crew->lock, "remove_worker");
        return -1;
    }
    crew->len--;
    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    crew_node *node = get_crew_node(list, id);

    // stop worker
    json_value *cmd = json_parse(STOP, strlen(STOP));
    json_value_free(send_command(node->worker, cmd));
    json_value_free(cmd);

    // remove node from freelist
    if (node->prev_free == NULL)
        crew->freelist.head = node->next_free;
    else
        node->prev_free->next_free = node->next_free;
    if (node->next_free == NULL)
        crew->freelist.tail = node->prev_free;
    else
        node->next_free->prev_free = node->prev_free;
    crew->freelist.len--;

    // remove node from workers
    if (node->prev == NULL)
        list->head = node->next;
    else
        node->prev->next = node->next;
    if (node->next == NULL)
        list->tail = node->prev;
    else
        node->next->prev = node->prev;
    list->len--;

    mutex_unlock(&crew->lock, "remove_worker");
    return 0;
}

/* get_worker_status: Gets the status of the worker by its id and returns it.
    Otherwise, returns -1. */
worker_status get_worker_status(Crew *crew, int id) {
    mutex_lock(&crew->lock, "get_worker_status");

    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    if (in_crew_list(list, id) == false) {
        mutex_unlock(&crew->lock, "get_worker_status");
        return -1;
    }
    
    worker_status status = get_crew_node(list, id)->worker->status;
    mutex_unlock(&crew->lock, "get_worker_status");
    return status;
}

/* get_worker_job_status: Gets the status of the worker's job by its id and
    returns it. Otherwise, returns -1. */
job_status get_worker_job_status(Crew *crew, int id) {
    mutex_lock(&crew->lock, "get_worker_job_status");

    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    if (in_crew_list(list, id) == false) {
        mutex_unlock(&crew->lock, "get_worker_job_status");
        return -1;
    }

    job_status status = get_crew_node(list, id)->worker->job.status;
    mutex_unlock(&crew->lock, "get_worker_job_status");
    return status;
}

/* assign_job: Assigns a job to a worker in the crew and returns its id. 
    Otherwise, returns -1. */
int assign_job(Crew *crew, json_value *job) {
    mutex_lock(&crew->lock, "assign_job");

    int id = freelist_pop(crew);
    if (id == -1) {
        mutex_unlock(&crew->lock, "assign_job");
        return -1;
    }

    // send command
    crew_node *node = get_crew_node(&crew->workers[id % _CREW_MAXLEN], id);
    json_value *cmd;
    cmd = json_object_new(0);
    json_object_push(cmd, "command", json_string_new("run_job"));
    json_object_push(cmd, "job", job);
    json_value_free(send_command(node->worker, cmd));
    json_builder_free(cmd);

    mutex_unlock(&crew->lock, "assign_job");
    return 0;
}

/* unassign_worker: Unassigns the worker from its current job and adds it
    to the freelist. */
int unassign_worker(Crew *crew, int id) {
    mutex_lock(&crew->lock, "unassign_worker");

    if (in_crew(crew, id) == false) {
        mutex_unlock(&crew->lock, "unassign_worker");
        return -1;
    }

    Worker *worker = get_worker(crew, id);
    json_value *cmd;
    char *stop = "{\"command\":\"stop\"}";
    switch (worker->status) {
        /* intentionally falling through :) */
        case worker_working:
            cmd = json_parse(stop, strlen(stop));
            json_value_free(send_command(worker, cmd));
            json_value_free(cmd);
        case worker_not_working:
            freelist_append(crew, id);
            worker->status = worker_not_assigned;
            worker->job.id = -1;
            worker->job.status = -1;
        case worker_not_assigned:
            break;
    }

    mutex_unlock(&crew->lock, "unassign_worker");
    return 0;
}

/* broadcast_command: Sends a command to all workers in the crew. */
void broadcast_command(Crew *crew, json_value *cmd) {
    mutex_lock(&crew->lock, "broadcast_command");

    if (crew->len == 0) {
        mutex_unlock(&crew->lock, "broadcast_command");
        return;
    }

    crew_list *list;
    crew_node *curr;
    for (int i = 0; i < _CREW_MAXLEN; i++) {
        list = &crew->workers[i];
        if (list->len == 0)
            continue;
        curr = list->head;
        while (curr) {
            json_value_free(send_command(curr->worker, cmd));
            curr = curr->next;
        }
    }

    mutex_unlock(&crew->lock, "broadcast_command");
    return;
}
