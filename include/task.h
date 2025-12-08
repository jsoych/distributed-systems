#ifndef _TASK_H
#define _TASK_H

#define TASK_EXIT_SUCCESS = 0
#define TASK_EXIT_EXEC_FAILURE 255

#include <unistd.h>
#include "json.h"

enum {
    TASK_NOT_READY,
    TASK_READY,
    TASK_RUNNING,
    TASK_COMPLETED,
    TASK_INCOMPLETE
};

typedef struct _task {
    int status;
    pid_t pid;
    int exit_code;
    char name[];
} Task;

// Construtor and destructor
Task* task_create(const char* name);
void task_destroy(Task* task);

// Methods
int task_run(Task* task, const char* python);
json_value* task_encode(const Task* task);

// Signals
void task_stop(Task* task);

// Helpers
Task* task_decode(const json_value* obj);

#endif
