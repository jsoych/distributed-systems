#ifndef _TASK_H
#define _TASK_H

#define TASK_EXIT_SUCCESS = 0
#define TASK_EXIT_SIGNAL 254
#define TASK_EXIT_FAILURE 255

#include <unistd.h>
#include "json.h"
#include "site.h"

enum {
    TASK_NOT_READY,
    TASK_READY,
    TASK_RUNNING,
    TASK_COMPLETED,
    TASK_INCOMPLETE
};

typedef struct _task {
    int status;
    char name[];
} Task;

typedef struct _task_runner {
    Task* task;
    int saved_status;
    pid_t task_pid;
    int exit_code;
} TaskRunner;

// Construtor and destructor
Task* task_create(const char* name);
void task_destroy(Task* task);

int task_get_status(Task* task);
json_value* task_encode(const Task* task);
json_value* task_encode_status(const Task* task);
Task* task_decode(const json_value* obj);

TaskRunner* task_run(Task* task, Site* site);

void task_runner_destroy(TaskRunner* runner);
void task_runner_stop(TaskRunner* runner);
void task_runner_wait(TaskRunner* runner);

// Helpers
json_value* task_status_map(const int status);

#endif
