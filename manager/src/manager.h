#ifndef _MANAGER_H
#define _MANAGER_H

#include <pthread.h>
#include "cJSON.h"
#include "project.h"

enum { _MANAGER_NOT_BUSY, _MANAGER_BUSY };
enum { _WORKER_NOT_BUSY, _WORKER_BUSY };

struct _manager;
typedef struct _worker {
    int id;
    int status;
} Worker;

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