#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "crew.h"

#define BUFLEN 1024

/* mutex_lock: Locks the mutex lock. If there is a system failure, mutex_lock
    prints a error message and exits the process. */
static void mutex_lock(pthread_mutex_t *lock, char *name) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(lock)) != 0) {
        fprintf(stderr, "crew: %s: %s\n", name, strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* mutex_unlock: Unlocks the mutex lock. If there is a system failue, 
    mutex_unlock prints a error and exits the process. */
static void mutex_unlock(pthread_mutex_t *lock, char *name) {
    int old_errno;
    if ((old_errno = pthread_mutex_unlock(lock)) != 0) {
        fprintf(stderr, "crew: %s: %s\n", name, strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
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
    if (strcmp(status, "not_assigned") == 0)
        return worker_not_assigned;
    else if (strcmp(status, "not_working") == 0)
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
        "/home/jsoychak/pyoneer/run/worker%d.socket", worker->id);
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

/* get_crew_node: Gets the crew node by its worker id from the list and
    returns it. Otherwise, returns NULL. */
static crew_node *get_crew_node(crew_list *l, int id) {
    if (l->len == 0)
        return NULL;
    
    crew_node *curr = l->head;
    while (curr) {
        if (curr->worker->id == id)
            return curr;
        curr = curr->next;
    }
    return curr;
}

/* add_crew_worker: Adds the worker to the end of the crew list. */
static void add_crew_worker(crew_list *l, Worker *w) {
    crew_node *n;
    if ((n = malloc(sizeof(crew_node))) == NULL) {
        perror("crew: add_crew_worker: malloc");
        exit(EXIT_FAILURE);
    }
    n->worker = w;
    n->next = NULL;
    n->prev = l->tail;
    n->next_free = NULL;
    n->prev_free = NULL;
    if (l->len++ == 0) {
        l->head = n;
        l->tail = n;
        return;
    }
    l->tail->next = n;
    l->tail = n;
    return;
}

/* create_crew: Creates a new crew. */
Crew *create_crew(void) {
    Crew *crew;
    if ((crew = malloc(sizeof(Crew))) == NULL) {
        perror("manager: create_crew: malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < _CREW_MAXLEN; ++i)
        init_crew_list(&crew->workers[i]);
    crew->len = 0;
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

/* free_crew: Frees all the resources allocated to the crew. */
static void free_crew(Crew *crew) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "manager: free_crew: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    
    if (crew->len == 0)
        goto unlock;

    crew->len = 0;
    for (int i = 0; i < _CREW_MAXLEN; i++)
        free_crew_list(&crew->workers[i]);

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

/* in_crew: Checks if the id is in the crew and returns 1, if the id matches
    a worker's id. Otherwise, returns 0. */

/* freelist_push: Adds the node to the head of the crew freelist. */
void freelist_push(Crew *crew, int id) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: freelist_push: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    if (in_crew_list(list, id) == false) {
        fprintf(stderr, "crew: freelist_push: warning: trying to push invalid worker id to the freelist\n");
        goto unlock;
    }

    crew_node *node = get_crew_node(list, id);
    node->prev_free = NULL;
    if (crew->freelist.len++ == 0) {
        node->next_free = NULL;
        crew->freelist.head = node;
        crew->freelist.tail = node;
        goto unlock;
    }
    node->next_free = crew->freelist.head;
    crew->freelist.head->prev_free = node;
    crew->freelist.head = node;

    unlock:
    if ((old_errno = pthread_mutex_unlock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: freelist_push: pthread_mutex_unlock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* freelist_pop: Pops the first worker from the crew freelist and returns its id.
    Otherwise, returns -1. */
int freelist_pop(Crew *crew) {
    int id = -1;
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: freelist_pop: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    if (crew->freelist.len == 0)
        goto unlock;

    crew_node *node = crew->freelist.head;
    id = node->worker->id;
    crew->freelist.head = node->next_free;
    node->next_free = NULL;
    node->prev_free = NULL;
    if (crew->freelist.len-- == 1) {
        crew->freelist.tail = NULL;
        goto unlock;
    }
    crew->freelist.head->prev_free = NULL;
    
    unlock:
    if ((old_errno = pthread_mutex_unlock(&crew->lock)) != 0) {
        fprintf(old_errno, "crew: freelist_pop: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return id;
}

/* freelist_append: Adds the worker to the tail of the crew freelist. */
static void freelist_append(Crew *crew, int id) {
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: freelist_append: pthread_mutex_lock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    if (in_crew_list(list, id) == false) {
        fprintf(stderr, "crew: freelist_append: warning: trying to add invalid worker id to the freelist\n");
        goto unlock;
    }

    crew_node *node = get_crew_node(list, id);
    node->next_free = NULL;
    if (crew->freelist.len++ == 0) {
        node->prev_free = NULL;
        crew->freelist.head = node;
        crew->freelist.tail = node;
        goto unlock;
    }
    node->prev_free = crew->freelist.tail;
    crew->freelist.tail->next_free = node;
    crew->freelist.tail = node;

    unlock:
    if ((old_errno = pthread_mutext_unlock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: freelist_append: pthread_mutex_unlock: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
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
int add_worker(Crew *crew, int id) {
    int old_errno, ret_val = 0;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "manager: add_worker: pthread_mutex_lock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    if (in_list(list, id) == true) {
        ret_val = -1;
        goto unlock;
    }

    add_crew_worker(list, create_worker(id));
    crew_node *node = get_crew_node(list, id);

    // Create worker thread
    if ((old_errno = pthread_create(&node->tid, NULL, worker_thread, &node->worker)) != 0) {
            fprintf(stderr, "crew: add_worker: pthread_create: %s\n", strerror(old_errno));
            exit(EXIT_FAILURE);
    }

    unlock:
    if ((old_errno = pthread_mutex_unlock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: add_worker: pthread_mutex_unlock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return ret_val;
}

/* remove_worker: Removes a worker from the crew with its id. */
int remove_worker(Crew *crew, int id) {
    int old_errno, ret_val = 0;
    if ((old_errno = pthread_mutex_lock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: remove_worker: pthread_mutex_unlock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }

    crew_list *list = &crew->workers[id % _CREW_MAXLEN];
    if (in_crew_list(list, id) == false) {
        
    }

    if ((old_errno = pthread_mutex_unlock(&crew->lock)) != 0) {
        fprintf(stderr, "crew: remove_worker: pthread_mutex_unlock: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }
}

/* broadcast_command: Sends a command to all workers in the crew. */
void broadcast_command(Crew *crew, json_value *cmd) {
    mutex_lock(&crew->lock, "broadcast_command");

    if (crew->len == 0) {
        mutex_unlock(&crew->lock, "broadcast_command");
        return -1;
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
    return 0;
}
