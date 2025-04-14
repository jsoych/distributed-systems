#ifndef _MANAGER_H
#define _MANAGER_H

#include <pthread.h>
#include "cJSON.h"
#include "project.h"

enum { _MANAGER_NOT_BUSY, _MANAGER_BUSY };

// Supervisor
typedef struct _supervisor {
   int id;
   // endpoint structure
} Supervisor;

struct _manager;

// RunningProject
typedef struct _running_project {
    struct _manager *man;
    Project *proj;
    List *ready_jobs;
    List *running_jobs;
} RunningProject;

// Manager
typedef struct _manager {
    int id;
    int status;
    RunningProject *running_proj;
} Manager;

Manager *create_manager(int);
void free_manager(Manager *);
int add_supervisor(int /*, endpoint structure */);

// Comands and signals
int run_project(Manager *, Project *);
int get_status(Manager *, int *);
int get_project_status(Manager *);
int start(Manager *);
int stop(Manager *);
cJSON *call_command(Manager *, cJSON *);

#endif