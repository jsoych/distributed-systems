#ifndef _TASK_H
#define _TASK_H
#define _TASK_INCOMPLETE -1
#define _TASK_RUNNING 1
#define _TASK_COMPLETED 2

typedef struct _task {
    char *name;
} Task;

Task *create_task(char *);
void free_task(Task *);
int run_task(Task *);

#endif
