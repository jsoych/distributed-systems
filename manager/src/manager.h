#ifndef _MANAGER_H
#define _MANAGER_H
#define _MANAGER_NOT_BUSY 0
#define _MANAGER_BUSY 1

#include <pthread.h>
#include "cJSON.h"
#include "project.h"

// worker status
typedef enum {
    worker_not_busy,
    worker_busy
} worker_status;

// job status
typedef enum {
    job_not_assigned,
    job_assigned,
    job_running,
    job_completed,
    job_incomplete
} job_status;

// job structure
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

// CrewNode
typedef struct _crew_node {
    Worker worker;
    struct _crew_node *next;
    struct _crew_node *prev;
    struct _crew_node *next_free;
    struct _crew_node *prev_free;
    pthread_t tid;
} crew_node;

// Crew
typedef struct _crew {
    crew_node *workers;
    crew_node *freelist;
    int len;
    pthread_mutex_t lock;
    pthread_t tid;
} Crew;

// RunningProject
typedef struct _running_project {
    struct _manager *man;
    Project *proj;
    List *not_ready_jobs;
    List *ready_jobs;
    List *running_jobs;
} RunningProject;

// Manager
typedef struct _manager {
    int id;
    int status;
    Crew crew;
    RunningProject *running_proj;
} Manager;

Manager *create_manager(int);
void free_manager(Manager *);
void add_worker(Manager *, int);

// Comands and signals
int run_project(Manager *, Project *);
int get_status(Manager *);
int get_project_status(Manager *);
int get_worker_status(Manager *, int);
int start(Manager *);
int stop(Manager *);
cJSON *call_command(Manager *, cJSON *);

#endif