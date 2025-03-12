#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "task.h"

#ifdef __APPLE__
static char python[] = "/Users/leejahsprock/miniconda3/envs/pyoneer/bin/python";
static char tasks_dir[] = "/Users/leejahsprock/cs/distributed-systems/worker/tasks/";
#endif

#ifdef __linux__
static char python[] = "/usr/bin/python3";
static char tasks_dir[] = "/root/distributed-systems/worker/tasks/"
#endif

/* create_task: Creates a new task. */
Task *create_task(char *name) {
    Task *task;
    if ((task = malloc(sizeof(Task))) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }

    char *s;
    if ((s = malloc(strlen(tasks_dir) + strlen(name) + 1)) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }
    task->name = s;
    s = stpcpy(s,tasks_dir);
    stpcpy(s, name);
    
    return task;
}

/* free_task: Frees memory allocated to the task. */
void free_task(Task *task) {
    free(task->name);
    free(task);
}

/* run_task: Runs the task in place. */
int run_task(Task *task) {
    
    if (execl(python, "python", task->name, NULL) == -1) {
        perror("task: run_task: execl");
        exit(EXIT_FAILURE);
    }
}