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
    // Free each job node
    JobNode *curr, *prev;
    curr = job->head;
    while (curr) {
        prev = curr;
        curr = curr->next;
        free_job_node(prev);
    }

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

/* encode_job: Encodes the job into a new cJSON object. */
cJSON *encode_job(Job *job) {
    cJSON *obj, *item, *arr;
    if ((obj = cJSON_CreateObject()) == NULL) {
        fprintf(stderr, "job: encode_job: cJSON_CreateObject\n");
        exit(EXIT_FAILURE);
    }

    cJSON_AddNumberToObject(obj,"id",job->id);
    cJSON_AddNumberToObject(obj,"status",job->status);
    cJSON_AddArrayToObject(obj,"jobs");
    arr = cJSON_GetObjectItem(obj,"jobs");
    for (JobNode *curr = job->head; curr; curr = curr->next) {
        item = cJSON_CreateString(curr->task->name);
        cJSON_AddItemToArray(arr,item);
    }

    return obj;
}

/* decode_job: Decodes the cJSON object into a new job. */
Job *decode_job(cJSON *obj) {
    cJSON *item = cJSON_GetObjectItem(obj,"id");
    Job *job = create_job(item->valuedouble,_JOB_INCOMPLETE);
    for (item = cJSON_GetObjectItem(obj,"tasks")->child; item; item = item->next) {
        add_task(job,item->valuestring);
    }
    return job;
}

/* marshal_job: Serializes the job into the buffer. */
size_t marshal_job(Job *job, void *buf, size_t len) {
    cJSON *obj = encode_job(job);
    char *str = cJSON_PrintUnformatted(obj);
    size_t nbytes = strlen(str);
    if (nbytes > len) {
        fprintf(stderr, "job: marshal_job: the buffer length is too small\n");
        exit(EXIT_FAILURE);
    }
    strcpy(buf,str);
    cJSON_Delete(obj);
    free(str);
    return nbytes;
}

/* unmarshal_job: Deserializes the buffer into a new job. */
Job *unmarshal_job(void *buf, size_t len) {
    cJSON *obj = cJSON_ParseWithLength(buf,len);
    Job *job = decode_job(obj);
    cJSON_Delete(obj);
    return job;
}
