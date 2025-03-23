#ifndef _MANAGER_H
#define _MANAGER_H

#include <pthread.h>
#include "project.h"

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
    int status;
    RunningProject *running_proj;
} Manager;

Manager *create_manager(void);
void free_manager(Manager *);
int run_project(Manager *);
int get_status(Manager, int *);
int start(Manager *);
void stop(Manager *);

#endif