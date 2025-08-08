#ifndef _MANAGER_H
#define _MANAGER_H
#define _MANAGER_NOT_ASSIGNED 2
#define _MANAGER_ASSIGNED 3
#define _MANAGER_NOT_WORKING 0
#define _MANAGER_WORKING 1

#include <pthread.h>
#include <semaphore.h>
#include "project.h"
#include "crew.h"

// report types
typedef enum {
    missing_job_dependencies,
    cyclic_job_dependencies
} report_t;

// Report object
typedef struct _report {
    report_t type;
    union {
        int integer
    } u;
} Report;

// running project node
typedef struct _running_project_node {
    int worker_id;
    Job *job;
    Job **deps;
    int len;
    struct _running_project_node *next;
    struct _running_project_node *prev;
} running_project_node;

// queue
typedef struct _running_project_queue {
    running_project_node *head;
    running_project_node *tail;
    int len;
} queue;

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
    sem_t lock;
} Manager;

Manager *create_manager(int);
void free_manager(Manager *);

// Commands
Report *check_project(Manager *, Project *);
int get_status(Manager *);
int get_project_status(Manager *);
int run_project(Manager *, Project *);
int start(Manager *);
int stop(Manager *);

#endif
