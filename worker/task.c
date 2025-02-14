#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "task.h"

static char python[] = "/Users/leejahsprock/miniconda3/envs/DSI/bin/python";

/* create_task: Creates a new task. */
Task *create_task(char *name) {
    Task *task;
    if ((task = malloc(sizeof(Task))) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }

    char *s;
    if ((s = malloc(strlen(name)+1)) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }
    task->name = strcpy(s,name);

    return task;
}

/* free_task: Frees memory allocated to the task. */
void free_task(Task *task) {
    free(task->name);
    free(task);
}

/* run_task: Runs the task in place. */
int run_task(Task *task) {
    if (execl(python, "python", task->name) == -1) {
        perror("task: run_task: execl");
        exit(EXIT_FAILURE);
    }
}