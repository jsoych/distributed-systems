#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "job.h"
#include "json-builder.h"
#include "json-helpers.h"

/* job_create: Creates a new job and returns it, if the id is positive
    (id > 0). Otherwise, returns NULL. */
Job* job_create(int id) {
    if (id <= 0) return NULL;

    // Create new job
    Job* job = malloc(sizeof(Job));
    if (job == NULL) {
        perror("job_create: malloc");
        return NULL;
    }

    job->id = id;
    job->status = JOB_READY;
    job->size = 0;
    job->head = NULL;
    return job;
}

/* job_destroy: Frees the memory allocated to the job. */
void job_destroy(Job *job) {
    if (job == NULL) return;

    job_node* curr = job->head;
    while (curr) {
        job_node* prev = curr;
        curr = curr->next;
        task_destroy(prev->task);
        free(curr);
    }
    free(job);
}

/* update_status: Updates the job status. The job is running, not ready or 
    incomplete, if one its tasks is running, not ready or incomplete, and
    is completed if all of its tasks are completed. Lastly, a job is ready
    when all of its tasks completed and there is at one ready task. */
static void update_status(Job* job) {
    if (job->size == 0) return;
    
    // Count each task state
    int status[5] = {0};
    job_node* curr = job->head;
    while (curr) {
        switch (curr->task->status) {
            case TASK_NOT_READY:
                status[TASK_NOT_READY]++;
                break;
            case TASK_READY:
                status[TASK_READY]++;
                break;
            case TASK_RUNNING:
                status[TASK_RUNNING]++;
                break;
            case TASK_COMPLETED:
                status[TASK_COMPLETED]++;
                break;
            case TASK_INCOMPLETE:
                status[TASK_INCOMPLETE]++;
                break;
        }
        curr = curr->next;
    }

    job->status =   (status[TASK_RUNNING] > 0)      ? JOB_RUNNING :
                    (status[TASK_NOT_READY] > 0)    ? JOB_NOT_READY :
                    (status[TASK_INCOMPLETE] > 0)   ? JOB_INCOMPLETE :
                    (status[TASK_READY] > 0)        ? JOB_READY : 
                    /* all tasks are completed */     JOB_COMPLETED;
}

/* job_get_status: Gets the updated status of the job. */
int job_get_status(Job* job) {
    update_status(job);
    return job->status;
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
    if (obj == NULL) return NULL;
    json_object_push_integer(obj, "id", job->id);

    // Add tasks
    json_value* arr = json_array_new(0);
    job_node *curr = job->head;
    while (curr) {
        json_value* val = task_encode(curr->task);
        json_array_push(arr, val);
        curr = curr->next;
    }

    json_object_push(obj, "tasks", arr);
    return obj;
}

/* job_decode: Decodes the JSON object into a new job. */
Job* job_decode(const json_value* obj) {
    json_value* id = json_object_get_value(obj, "id");
    json_value* tasks = json_object_get_value(obj, "tasks");
    if (id == NULL || tasks == NULL) return NULL;
    if (id->type != json_integer || tasks->type != json_array) return NULL;

    Job* job = job_create(id->u.integer);
    for (unsigned int i = 0; i < tasks->u.array.length; i++) {
        Task* task = task_decode(tasks->u.array.values[i]);
        if (task == NULL) {
            job_destroy(job);
            return NULL;
        }

        if (job_add_task(job, task) == -1) {
            job_destroy(job);
            return NULL;
        }
    }

    return job;
}

/* job_status_map: Maps job status codes to its corresponding
    JSON value. */
json_value* job_status_map(int status) {
    switch (status) {
        case JOB_NOT_READY:     return json_string_new("not_ready");
        case JOB_READY:         return json_string_new("ready");
        case JOB_RUNNING:       return json_string_new("running");
        case JOB_COMPLETED:     return json_string_new("completed");
        case JOB_INCOMPLETE:    return json_string_new("incomplete");
        default:                return json_null_new();
    }
}
