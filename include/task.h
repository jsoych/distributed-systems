#ifndef _TASK_H
#define _TASK_H

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
    char* name;
} Task;

Task* task_create(const char* name);
void task_destroy(Task* task);

int task_run(Task* task, const char* python);
json_value* task_encode(const Task* task);

Task* task_decode(const json_value* obj);

#endif
