#ifndef _JOB_H
#define _JOB_H

#include "json.h"
#include "task.h"

enum {
    JOB_NOT_READY,
    JOB_READY,
    JOB_RUNNING,
    JOB_COMPLETED,
    JOB_INCOMPLETE
};

typedef struct _job_node {
    Task* task;
    struct _job_node *next;
} job_node;

typedef struct {
    int id;
    int status;
    int size;
    job_node *head;
} Job;

// Constructor and Destructor
Job* job_create(int id);
void job_destroy(Job* job);

// Methods
int job_get_status(Job* job);
int job_add_task(Job* job, Task* task);

// Helpers
json_value* job_encode_status(Job* job);
json_value* job_encode(Job* job);
Job* job_decode(const json_value* obj);

#endif
