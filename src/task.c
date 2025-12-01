#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "task.h"

/* task_create: Creates a new task. */
Task* task_create(const char* name) {
    int len = strlen(name);
    Task* task = malloc(sizeof(Task) + (len + 1)*sizeof(char));
    if (task == NULL) {
        perror("task_create: malloc");
        return NULL;
    }
    task->status = TASK_READY;
    strcpy(task->name, name);
    task->name[len] = '\0'; 
    return task;
}

/* task_destroy: Frees the memory allocated to the task. */
void task_destroy(Task *task) {
    free(task);
}

/* task_run: Executes the task by overlaying the running process with the
    python interpreter. */
int task_run(Task* task, const char* python) {
    char* task_dir = getenv("PYONEER_TASK_DIR");
    int len = strlen(task_dir) + strlen(task->name) + 2;
    char task_path[len];
    char* end = strcpy(task_path, task_dir);
    end = strcpy(end, "/");
    strcpy(end, task->name);
    if (execl(python, task_path, NULL) == -1) {
        perror("task_run: execl");
        return -1;
    }
    return 0;
}
