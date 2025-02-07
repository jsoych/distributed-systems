#include <stdio.h>
#include <stdlib.h>
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
    // kill task_thread
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
    // job->job_thread = need default value

    return job;
}

/* free_job: Frees memory allocated to the job. */
void free_job(Job *job) {
    // Free each job node
    JobNode *curr, *prev;
    curr = job->head;
    while (curr) {
        prev = curr;
        curr = curr->next;
        free_job_node(prev);
    }

    // kill job thread
    free(job);
}

/* add_task: Adds a task to the job. */
void add_task(Job *job, char *name) {
    Task *task = create_task(name);

    JobNode *new_node = create_job_node(task);

    // Add new node to the job
    new_node->next = job->head;
    job->head = new_node;
}