#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
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

/* run_task: Runs the task and returns its status. */
int run_task(Task *task) {
    // Create task process
    pid_t id;
    if ((id = fork()) == -1) {
        perror("task: run_task: fork");
        exit(EXIT_FAILURE);
    }

    // Run Task
    if (id == 0) {
        // Execute task
        if (execl(python, "python", task->name) == -1) {
            perror("task: run_task: execl");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for task to complete
    int status;
    if (waitpid(id, &status, 0) == -1) {
        perror("task: run_task: waitpid");
        exit(EXIT_FAILURE);
    }

    return WEXITSTATUS(status);
}