#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "task.h"
#include "json-builder.h"
#include "json-helpers.h"

/* task_create: Creates a new task. */
Task* task_create(const char* name) {
    Task* task = malloc(sizeof(Task));
    if (task == NULL) {
        perror("task_create: malloc");
        return NULL;
    }

    int len = strlen(name);
    if (len == 0) {
        fprintf(stderr, "task_create: Error: Missing name\n");
        free(task);
        return NULL;
    }
    
    task->name = malloc((len + 1)*sizeof(char));
    if (task->name == NULL) {
        perror("task_create: malloc");
        free(task);
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
    
    if (execl(python, python, task_path, NULL) == -1) {
        perror("task_run: execl");
        return -1;
    }

    return 0;
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
