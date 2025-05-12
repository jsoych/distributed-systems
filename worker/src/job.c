#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "job.h"

/* create_job_node: Creates a new job node. */
static JobNode *create_job_node(Task *task) {
    JobNode *job_node;
    if ((job_node = malloc(sizeof(JobNode))) == NULL) {
        perror("job: create_job_node: malloc");
        exit(EXIT_FAILURE);
    }
    job_node->status = _TASK_INCOMPLETE;
    job_node->task = task;
    return job_node;
}

/* free_job_node: Frees memory allocated to the job node. */
static void free_job_node(JobNode *job_node) {
    free_task(job_node->task);
    free(job_node);
}

/* create_job: Creates a new job. */
Job *create_job(int id,int status) {
    Job *job;
    if ((job = malloc(sizeof(Job))) == NULL) {
        perror("job: create_job: malloc");
        exit(EXIT_FAILURE);
    }
    job->id = id;
    job->status = status;
    job->head = NULL;
    return job;
}

/* free_job: Frees the memory allocated to the job. */
void free_job(Job *job) {
    JobNode *curr, *prev;
    curr = job->head;
    while (curr) {
        prev = curr;
        curr = curr->next;
        free_job_node(prev);
    }
    free(job);
    return;
}

/* add_task: Adds a task to the job. */
void add_task(Job *job, char *name) {
    Task *task = create_task(name);
    JobNode *new_node = create_job_node(task);
    new_node->next = job->head;
    job->head = new_node;
    return;
}

/* encode_job: Encodes the job as a JSON object. */
json_value *encode_job(Job *job) {
    json_value *obj, *val, *arr;
    if ((obj = json_object_new(0)) == NULL) {
        fprintf(stderr, "job: encode_job: json_object_new: Unable to create new object\n");
        exit(EXIT_FAILURE);
    }
    val = json_integer_new(job->id);
    json_object_push(obj, "id", val);

    if ((arr = json_array_new(0)) == NULL) {
        fprintf(stderr, "job: encode_job: json_array_new: Unable to create new array\n");
        exit(EXIT_FAILURE);
    }
    for (JobNode *curr = job->head; curr; curr = curr->next) {
        val = json_string_new(curr->task->name);
        json_array_push(arr, val);
    }
    json_object_push(obj, "tasks", arr);
    return obj;
}

/* job_status_map: Maps strings to job status codes. */
static int job_status_map(char *status) {
    if (strcmp(status, "not_ready") == 0)
        return _JOB_NOT_READY;
    else if (strcmp(status, "ready") == 0)
        return _JOB_READY;
    else if (strcmp(status, "running") == 0)
        return _JOB_RUNNING;
    else if (strcmp(status, "completed") == 0)
        return _JOB_COMPLETED;
    else if (strcmp(status, "incomplete") == 0)
        return _JOB_INCOMPLETE;
    else
        return -1;
}

/* decode_job: Decodes the JSON object into a new job. */
Job *decode_job(json_value *obj) {
    if (obj == NULL) {
        fprintf(stderr, "job: decode_job: Warning: JSON value is null\n");
        return NULL;
    }
    if (obj->type != json_object) {
        fprintf(stderr, "job: decode_job: Error: incorrect JSON type\n");
        return NULL;
    }
    int id, len;
    json_char *status;
    json_value **tasks;
    // Get id, status, and tasks
    for (int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, "id") == 0)
            id = obj->u.object.values[i].value->u.integer;
        else if (strcmp(obj->u.object.values[i].name, "status") == 0)
            status = obj->u.object.values[i].value->u.string.ptr;
        else if (strcmp(obj->u.object.values[i].name, "tasks") == 0) {
            tasks = obj->u.object.values[i].value->u.array.values;
            len = obj->u.object.values[i].value->u.array.length;
        }
    }
    Job *job = create_job(id, job_status_map(status));
    for (int i = 0; i < len; i++) {
        add_task(job, tasks[i]->u.string.ptr);
    }
    return job;
}
