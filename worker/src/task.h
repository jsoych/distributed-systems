#ifndef pyoneer_task_h
#define pyoneer_task_h

typedef struct _task {
    char *name;
} Task;

Task *create_task(char *);
void free_task(Task *);
int run_task(Task *);

#endif
