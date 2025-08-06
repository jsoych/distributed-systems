#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "task.h"

#ifdef __APPLE__
static char python[] = "/Users/leejahsprock/miniconda3/envs/pyoneer/bin/python";
static char tasks_dir[] = "/Users/leejahsprock/cs/distributed-systems/worker/tasks/";
#elif __linux__
static char python[] = "/home/jsoychak/miniconda3/bin/python";
static char tasks_dir[] = "/home/jsoychak/pyoneer/var/lib/tasks";
#endif

/* create_task: Creates a new task. */
Task *create_task(char *name) {
    Task *task;
    if ((task = malloc(sizeof(Task))) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }

    char *s;
    if ((s = malloc(strlen(name) + 1)) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(s, name);
    task->name = s;
    return task;
}

/* free_task: Frees memory allocated to the task. */
void free_task(Task *task) {
    free(task->name);
    free(task);
}

/* run_task: Runs the task in place. */
int run_task(Task *task) {
    char *t;
    if ((t = malloc(strlen(tasks_dir) + strlen(task->name) + 1)) == NULL) {
        perror("task: run_task: malloc");
        exit(EXIT_FAILURE);
    }
    sprintf(t, "%s/%s", tasks_dir, task->name);
    if (execl(python, "python", t, NULL) == -1) {
        perror("task: run_task: execl");
        exit(EXIT_FAILURE);
    }
}
