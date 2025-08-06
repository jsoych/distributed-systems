#ifndef _CREW_H
#define _CREW_H

#include <pthread.h>
#include "json.h"

#define _CREW_MAXLEN 512

// worker statuses
typedef enum {
    worker_not_assigned,
    worker_not_working,
    worker_working
} worker_status;

// job statuses
typedef enum {
    job_not_ready,
    job_ready,
    job_running,
    job_completed,
    job_incomplete
} job_status;

// job
typedef struct _crew_job {
    int id;
    job_status status;
} job;

// Worker object
typedef struct _crew_worker {
    int id;
    worker_status status;
    job job;
} Worker;

// crew node
typedef struct _crew_node {
    Worker *worker;
    struct _crew_node *next;
    struct _crew_node *prev;
    struct _crew_node *next_free;
    struct _crew_node *prev_free;
    pthread_t tid;
} crew_node;

// crew list
typedef struct _crew_list {
    crew_node *head;
    crew_node *tail;
    int len;
} crew_list;

// Crew object
typedef struct _crew {
    crew_list workers[_CREW_MAXLEN];
    crew_list freelist;
    int len;
    pthread_mutex_t lock;
    pthread_t tid;
} Crew;

Crew *create_crew();
void free_crew(Crew *);
int add_worker(Crew *, int);
int remove_worker(Crew *, int);
worker_status get_worker_status(Crew *, int);
job_status get_worker_job_status(Crew *, int);
int update_worker_status(Crew *, int, worker_status);
void freelist_append(Crew *, int);
void freelist_push(Crew *, int);
int freelist_pop(Crew *);
void broadcast_command(Crew *, json_value *);

#endif