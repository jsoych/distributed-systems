#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "task.h"
#include "json-builder.h"
#include "json-helpers.h"

/* task_create: Creates a new task. */
Task* task_create(const char* name) {
    int len = strlen(name);
    if (len == 0) return NULL;

    Task* task = malloc(sizeof(Task) + (len + 1)*sizeof(char));
    if (task == NULL) {
        perror("task_create: malloc");
        return NULL;
    }

    task->status = TASK_READY;
    task->pid = -1;
    task->exit_code = -1;
    strcpy(task->name, name);
    task->name[len] = '\0';
    return task;
}

/* task_destroy: Frees the memory allocated to the task. */
void task_destroy(Task *task) {
    if (task == NULL) return;
    if (task->status == TASK_RUNNING) task_stop(task);
    free(task);
}

/* task_run: Runs the task as a separated process and dedirects the task
    outputs to stdout and return its exit code. Otherwise, returns -1. */
int task_run(Task* task, const char* python) {
    const char* task_dir = getenv("PYONEER_TASK_DIR");
    if (task_dir == NULL) {
        fprintf(stderr, "task_run: Error: Missing environment variable PYONEER_TASK_DIR\n");
        return -1;
    }

    // Join task_dir and task name
    int len = strlen(task_dir) + strlen(task->name) + 2;
    char task_path[len];
    char* end = stpcpy(task_path, task_dir);
    end = stpcpy(end, "/");
    stpcpy(end, task->name);

    task->status = TASK_RUNNING;
    task->pid = fork();
    if (task->pid == -1) {
        perror("task_run: fork");
        task->status = TASK_READY;
        task->pid = -1;
        return -1;
    }

    if (task->pid == 0) {
        // Child process

        // task_run overlays the child process with a running python script
        if (execl(python, python, task_path, NULL) == -1) {
            perror("task_run: execl");
            _exit(TASK_EXIT_EXEC_FAILURE);
        }

        // Running python script
    }

    int status;
    if (wait(&status) == -1) {
        perror("task_run: wait");
        task->status = TASK_READY;
        task->pid = -1;
        return -1;
    }
    
    if (WIFEXITED(status)) {
        int code =  WEXITSTATUS(status);
        if (code == TASK_EXIT_EXEC_FAILURE) {
            fprintf(stderr, "task_run: Error: Child process exited with code (%d)\n", code);
            task->status = TASK_READY;
            task->pid = -1;
            task->exit_code = TASK_EXIT_EXEC_FAILURE;
            return -1;
        }
        
        // Update task status
        task->status = (code == 0) ? TASK_COMPLETED : TASK_NOT_READY;
        task->pid = -1;
        task->exit_code = code;
        return 0;
    } else if (WIFSIGNALED(status)) {
        // Task exited from a signal
        task->status = TASK_INCOMPLETE;
        task->pid = -1;
        task->exit_code = -1;
        return 0;
    }

    fprintf(stderr, "task_run: Error: Child process did not exit as expected\n");
    task->status = TASK_INCOMPLETE;
    task->pid = -1;
    task->exit_code = -1;
    return -1;
}

void task_stop(Task* task) {
    if (task != TASK_RUNNING) return;
    pid_t task_pid = task->pid;
    if (task_pid == -1) return;

    // Kill process
    if (kill(task_pid, SIGKILL) == -1) {
        perror("task_stop: kill");
        return;
    }

    // Reap zombie process
    if (waitpid(task_pid, NULL, 0) == -1) {
        perror("task_stop: waitpid");
        return;
    }
    return;
}

json_value* task_encode(const Task* task) {
    if (task == NULL) return json_null_new();
    json_value* obj = json_object_new(0);
    json_object_push_string(obj, "name", task->name);
    return obj;
}

Task* task_decode(const json_value* obj) {
    if (obj == NULL) return NULL;
    if (obj->type != json_object) return NULL;
    json_value* name = json_object_get_value(obj, "name");
    if (name == NULL) return NULL;
    if (name->type != json_string) return NULL;
    return task_create(name->u.string.ptr);
}
