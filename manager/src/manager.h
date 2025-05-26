#ifndef _MANAGER_H
#define _MANAGER_H
#define _MANAGER_NOT_WORKING 0
#define _MANAGER_WORKING 1

#include <pthread.h>
#include "project.h"

// worker statuses
typedef enum {
    worker_not_assigned,
    worker_assigned,
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

// Job structure
typedef struct _job {
    int id;
    job_status status;
} job;

// Worker object
typedef struct _worker {
    int id;
    worker_status status;
    job job;
} Worker;

struct _manager;

// Crew node structure
typedef struct _crew_node {
    Worker *worker;
    struct _crew_node *next;
    struct _crew_node *prev;
    struct _crew_node *next_free;
    struct _crew_node *prev_free;
    pthread_t tid;
} crew_node;

// list structure
typedef struct _list {
    crew_node *head;
    crew_node *tail;
    int len;
} list;

// Crew object
typedef struct _crew {
    list workers;
    list freelist;
    pthread_mutex_t lock;
    pthread_t tid;
} Crew;

// queue structure
typedef struct _queue {
    RunningProjectNode *head;
    RunningProjectNode *tail;
    int len;
} queue;

// RunningProjectNode
typedef struct _running_project_node {
    int worker_id;
    Job *job;
    Job **deps;
    int len;
    struct _running_project_node *next;
    struct _running_project_node *prev;
} RunningProjectNode;

// RunningProject object
typedef struct _running_project {
    struct _manager *manager;
    Project *project;
    queue not_ready_jobs;
    queue ready_jobs;
    queue running_jobs;
    queue completed_jobs;
    queue incomplete_jobs;
    pthread_t tid;
} RunningProject;

// Manager object
typedef struct _manager {
    int id;
    int status;
    Crew *crew;
    RunningProject *running_project;
} Manager;

Manager *create_manager(int);
void free_manager(Manager *);

// Comands and signals
int run_project(Manager *, Project *);
int get_status(Manager *);
int get_project_status(Manager *);
int start(Manager *);
int stop(Manager *);

#endif