#ifndef _JOB_H
#define _JOB_H
#define _JOB_INCOMPLETE -2
#define _JOB_NOT_READY -1
#define _JOB_READY 0
#define _JOB_RUNNING 1
#define _JOB_COMPLETED 2

#include "cJSON.h"
#include "task.h"

typedef struct _job_node {
    int status;
    Task *task;
    struct _job_node *next;
} JobNode;

typedef struct _job {
    int id;
    int status;
    JobNode *head;
} Job;

Job *create_job(int,int);
void free_job(Job *);
void add_task(Job *, char *);
size_t marshal_job(Job *, void *buf, size_t nbytes);
Job *unmarshal_job(Job *, void *buf, size_t nbytes);
cJSON *encode_job(Job *);
Job* *decode_job(cJSON *);

#endif