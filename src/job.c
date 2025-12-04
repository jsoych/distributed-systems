#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "job.h"
#include "json-builder.h"
#include "json-helpers.h"

/* job_create: Creates a new job. */
Job* job_create(int id) {
    Job* job = malloc(sizeof(Job));
    if (job == NULL) {
        perror("job_create: malloc");
        exit(EXIT_FAILURE);
    }
    job->id = id;
    job->status = JOB_READY;
    job->size = 0;
    job->head = NULL;
    return job;
}

/* job_destroy: Frees the memory allocated to the job. */
void job_destroy(Job *job) {
    job_node* curr = job->head;
    while (curr->next) {
        job_node* prev = curr;
        curr = curr->next;
        task_destroy(prev->task);
        free(prev);
    }
    free(job);
    return;
}

/* job_update_status: Updates and returns the job status. */
int job_update_status(Job* job) {
    if (job->size == 0) return JOB_NOT_READY;

    job_node* curr = job->head;
    while (curr) {
        switch (curr->task->status) {
            case TASK_NOT_READY: return (job->status = JOB_NOT_READY);
            case TASK_RUNNING: return (job->status = JOB_RUNNING);
            case TASK_INCOMPLETE: return (job->status = JOB_INCOMPLETE);
        }
    }
    return (job->status = JOB_COMPLETED);
}

/* job_add_task: Adds the task to the job and returns 0, if the task is
    successfully added to the job. Otherwise, returns -1. */
int job_add_task(Job *job, Task* task) {
    job_node* node = malloc(sizeof(job_node));
    if (node == NULL) {
        perror("job_add_task: malloc");
        return -1;
    }
    node->task = task;
    node->next = job->head;
    job->head = node;
    job->size++;
    return 0;
}

/* job_encode: Encodes the job as a JSON object. */
json_value* job_encode(Job *job) {
    json_value *obj = json_object_new(0);
    json_object_push(obj, "id", json_integer_new(job->id));

    // Add tasks
    json_value* arr = json_array_new(0);
    job_node *curr = job->head;
    while (curr) {
        json_array_push(arr, json_string_new(curr->task->name));
        curr = curr->next;
    }
    json_object_push(obj, "tasks", arr);

    return obj;
}

/* job_decode: Decodes the JSON object into a new job. */
Job* job_decode(const json_value* obj) {
    json_value* val = json_object_get_value(obj, "id");
    if (val == NULL) return NULL;
    Job* job = job_create(val->u.integer);

    // Add tasks
    val = json_object_get_value(obj, "tasks");
    if (val == NULL) {
        job_destroy(job);
        return NULL;
    }

    for (unsigned int i = 0; i < val->u.array.length; i++) {
        Task* task = task_create(val->u.array.values[i]->u.string.ptr);
        job_add_task(job, task);
    }

    return job;
}

/* job_status_map: Maps job status codes to its corresponding
    JSON value. */
json_value* job_status_map(int status) {
    json_value *val;
    switch (status) {
        case JOB_RUNNING:
            val = json_string_new("running");
            break;
        case JOB_COMPLETED:
            val = json_string_new("completed");
            break;
        case JOB_INCOMPLETE:
            val = json_string_new("incomplete");
            break;
        default:
            val = json_null_new();
            break;
    }
    return val;
}
