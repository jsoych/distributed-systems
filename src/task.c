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
    strcpy(task->name, name);
    task->name[len] = '\0';
    return task;
}

/* task_destroy: Frees the memory allocated to the task. */
void task_destroy(Task *task) {
    if (task == NULL) return;
    free(task);
}

/* task_get_status: Get the task status and returns it. */
int task_get_status(Task* task) {
    return task->status;
}

/* task_run: Runs the task as a separated process and dedirects the task
    outputs to stdout and return its exit code. Otherwise, returns -1. */
TaskRunner* task_run(Task* task, Site* site) {
    if (!task || !site) return NULL;

    // Create runner
    TaskRunner* runner = malloc(sizeof(TaskRunner));
    if (runner == NULL) {
        perror("task_run: malloc");
        return NULL;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("task_run: fork");
        free(runner);
        return NULL;
    }

    runner->task = task;
    runner->saved_status = task->status;
    runner->task_pid = pid;
    runner->exit_code = -1;

    // Run task
    if (pid == 0) {
        // ----- Child process -----   
        if (setpgid(pid, pid) == -1) {
            exit(TASK_EXIT_FAILURE);
        }
        
        if (chdir(site->working_dir) == -1) {
            exit(TASK_EXIT_FAILURE);
        }

        int len = strlen(site->task_dir) + strlen(task->name) + 1;
        char task_path[len + 1];
        if (sprintf(task_path, "%s/%s", site->task_dir, task->name) != len) {
            fprintf(stderr, "task_run: sprintf: Error (%s)\n", task_path);
            exit(TASK_EXIT_FAILURE);
        }

        // Execute the task
        if (execl(site->python, site->python, task_path, NULL) == -1) {
            perror("task_run: execl");
            exit(TASK_EXIT_FAILURE);
        }

        // Running python script
    }

    // ----- Parent process -----
    task->status = TASK_RUNNING;
    return runner;
}

/* task_runner_wait: Waits for a task runner to finish executing, cleans up, and
    sets its to exit code to the code of the exiting process. */
void task_runner_wait(TaskRunner* runner) {
    if (!runner) return;
    int status;
    if (waitpid(runner->task_pid, &status, 0) == -1) {
        perror("task_wait: waitpid");
        return;
    }

    if (WIFEXITED(status)) {
        int code =  WEXITSTATUS(status);
        if (code == TASK_EXIT_FAILURE) {
            fprintf(stderr, "task_run: Error: Child process failed to run\n");
            runner->task->status = runner->saved_status;
            runner->exit_code = TASK_EXIT_FAILURE;
            return;
        }
        runner->task->status = (code == 0) ? TASK_COMPLETED : TASK_NOT_READY;
        runner->exit_code = code;
        return;
    } else if (WIFSIGNALED(status)) {
        runner->task->status = TASK_INCOMPLETE;
        runner->exit_code = TASK_EXIT_SIGNAL;
        return;
    }

    fprintf(stderr, "task_run: Error: Child process exited unexpectedly\n");
    runner->task->status = runner->saved_status;
    runner->exit_code = -1;
    return;
}

/* task_runner_stop: Stops the task runner. */
void task_runner_stop(TaskRunner* runner) {
    if (!runner) return;
    if (kill(runner->task_pid, SIGKILL) == -1) return;
    task_runner_wait(runner);
    return;
}

/* task_runner_destroy: Frees the task resources. */
void task_runner_destroy(TaskRunner* runner) {
    if (!runner) return;
    task_runner_stop(runner);
    free(runner);
}

/* task_encode: Encodes the task into a JSON object. */
json_value* task_encode(const Task* task) {
    json_value* obj = json_object_new(0);
    if (obj == NULL) return NULL;
    json_object_push_string(obj, "name", task->name);
    return obj;
}

/* task_status_map: Maps the task status code into a JSON value. */
json_value* task_status_map(const int status) {
    switch (status) {
        case TASK_NOT_READY:    return json_string_new("not_ready");
        case TASK_READY:        return json_string_new("ready");
        case TASK_RUNNING:      return json_string_new("running");
        case TASK_COMPLETED:    return json_string_new("completed");
        case TASK_INCOMPLETE:   return json_string_new("incomplete");
        default:                return json_null_new();
    }
}

/* task_encode_status: Encode the task status and returns it. If the task
    is not ready, it includes its exit code in addition.  */
json_value* task_encode_status(const Task* task) {
    json_value* obj = json_object_new(0);
    if (obj == NULL) return NULL;
    json_value* status = task_status_map(task->status);
    json_object_push(obj, "status", status);
    return obj;
}

/* task_decode: Decodes the object into a new task. */
Task* task_decode(const json_value* obj) {
    if (obj == NULL) return NULL;
    if (obj->type != json_object) return NULL;
    json_value* name = json_object_get_value(obj, "name");
    if (name == NULL) return NULL;
    if (name->type != json_string) return NULL;
    return task_create(name->u.string.ptr);
}
