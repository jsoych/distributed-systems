#ifndef _JOB_H
#define _JOB_H
#define _JOB_INCOMPLETE -2
#define _JOB_NOT_READY -1
#define _JOB_READY 0
#define _JOB_RUNNING 1
#define _JOB_COMPLETED 2

#define _TASK_INCOMPLETE -1
#define _TASK_RUNNING 1
#define _TASK_COMPLETED 2

#include <pthread.h>
#include "task.h"

typedef struct _job_node {
    int status;
    Task *task;
    int task_rv;
    pthread_t task_thread;
    struct _job_node *next;
} JobNode;

typedef struct _job {
    int id;
    int status;
    JobNode *head;
    pthread_t job_thread;
} Job;

Job *create_job(int,int);
void free_job(Job *);
void add_task(Job *);

#endif