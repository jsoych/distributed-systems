#ifndef pyoneer_job_h
#define pyoneer_job_h

#include "json.h"
#include "json-builder.h"
#include "task.h"

typedef enum {
    TASK_NOT_READY,
    TASK_READY,
    TASK_RUNNING,
    TASK_COMPLETED,
    TASK_INCOMPLETE
} task_status_t;

typedef enum {
    JOB_NOT_READY,
    JOB_READY,
    JOB_RUNNING,
    JOB_COMPLETED,
    JOB_INCOMPLETE
} job_status_t;

typedef struct {
    task_status_t status;
    Task* task;
    struct _job_node *next;
} job_node;

typedef struct {
    int id;
    job_status_t status;
    job_node *head;
} Job;

Job* create_job(int);
void free_job(Job *);
void add_task(Job *, char *);
json_value* encode_job(Job *);
Job* decode_job(json_value *);

#endif
