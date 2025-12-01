#ifndef _TASK_H
#define _TASK_H

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

Task* task_create(const char* name);
void task_destroy(Task* task);
int task_run(Task* task, const char* python);

#endif
