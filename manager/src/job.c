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
Job *create_job(int id) {
    Job *job;
    if ((job = malloc(sizeof(Job))) == NULL) {
        perror("job: create_job: malloc");
        exit(EXIT_FAILURE);
    }
    job->id = id;
    job->status = _JOB_READY;
    job->len = 0;
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
    ++job->len;
    return;
}

/* encode_job: Encodes the job as a JSON object. */
json_value *encode_job(Job *job) {
    json_value *obj, *val, *arr;
    obj = json_object_new(0);

    // add id
    val = json_integer_new(job->id);
    json_object_push(obj, "id", val);

    // add tasks
    arr = json_array_new(0);
    for (JobNode *curr = job->head; curr; curr = curr->next) {
        val = json_string_new(curr->task->name);
        json_array_push(arr, val);
    }
    json_object_push(obj, "tasks", arr);
    return obj;
}

/* decode_job: Decodes the JSON object into a new job. */
Job *decode_job(json_value *obj) {
    json_value *id, *tasks;
    id = json_get_value(obj, "id");
    tasks = json_get_value(obj, "tasks");

    Job *job = create_job(id->u.integer);
    for (unsigned int i = 0; i < tasks->u.array.length; i++)
        add_task(job, tasks->u.array.values[i]->u.string.ptr);
    return job;
}
