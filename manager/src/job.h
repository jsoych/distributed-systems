#ifndef _JOB_H
#define _JOB_H
#define _JOB_NOT_READY 0
#define _JOB_READY 1
#define _JOB_RUNNING 2
#define _JOB_COMPLETED 3
#define _JOB_INCOMPLETE 4

#include "json-builder.h"
#include "task.h"

typedef struct _job_node {
    int status;
    Task *task;
    struct _job_node *next;
} JobNode;

typedef struct _job {
    int id;
    int status;
    int len;
    JobNode *head;
} Job;

Job *create_job(int);
void free_job(Job *);
void add_task(Job *, char *);
json_value *encode_job(Job *);
Job *decode_job(json_value *);

#endif
