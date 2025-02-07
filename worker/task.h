#ifndef _TASK_H
#define _TASK_H

typedef struct _task {
    char *name;
} Task;

Task *create_task(char *);
void free_task(Task *);
int run_task(Task *);

#endif