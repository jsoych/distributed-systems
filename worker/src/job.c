#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "job.h"
#include "task.h"

static char python[] = "/home/jsoychak/miniconda3/bin/python";
static char tasks_dir[] = "/home/jsoychak/pyoneer/var/lib/tasks";

/* create_task: Creates a new task. */
Task *create_task(char *name) {
    Task *task;
    if ((task = malloc(sizeof(Task))) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }
    task->status = _TASK_READY;
    char *s;
    if ((s = malloc(sizeof(char) * (strlen(name) + 1))) == NULL) {
        perror("task: create_task: malloc");
        exit(EXIT_FAILURE);
    }
    strcpy(s, name);
    task->name = s;
    return task;
}

/* free_task: Frees memory allocated to the task. */
void free_task(Task *task) {
    free(task->name);
    free(task);
    return;
}

/* run_task: Runs the task in place. */
int run_task(Task *task) {
    char *t;
    if ((t = malloc(strlen(tasks_dir) + strlen(task->name) + 1)) == NULL) {
        perror("task: run_task: malloc");
        exit(EXIT_FAILURE);
    }
    sprintf(t, "%s/%s", tasks_dir, task->name);
    if (execl(python, "python", t, NULL) == -1) {
        perror("task: run_task: execl");
        exit(EXIT_FAILURE);
    }
}

/* json_get_value: Gets the json value by its name. */
static json_value *json_get_value(json_value *, char *);

/* create_job: Creates a new job. */
Job *create_job(int id) {
    Job *job;
    if ((job = malloc(sizeof(Job))) == NULL) {
        perror("job: create_job: malloc");
        exit(EXIT_FAILURE);
    }
    job->id = id;
    job->status = _JOB_READY;
    job->head = NULL;
    return job;
}

/* free_job: Frees the memory allocated to the job. */
void free_job(Job *job) {
    if (job->head == NULL) {
        free(job);
        return;
    }

    job_node *prev, *curr = job->head;
    while (curr->next) {
        prev = curr;
        curr = curr->next;
        free_task(prev->task);
        free(prev);
    }
    free_task(curr->task);
    free(curr);
    return;
}

/* add_task: Adds a task to the job. */
void add_task(Job *job, char *name) {
    Task *task = create_task(name);
    job_node *node;
    if ((node = malloc(sizeof(job_node))) == NULL) {
        perror("job: add_task: malloc");
        exit(EXIT_FAILURE);
    }
    node->task = task;
    node->next = job->head;
    job->head = node;
    return;
}

/* encode_job: Encodes the job as a JSON object. */
json_value *encode_job(Job *job) {
    json_value *arr, *obj;
    obj = json_object_new(0);
    json_object_push(obj, "id", json_integer_new(job->id));
    arr = json_array_new(0);
    job_node *curr = job->head;
    while (curr) {
        json_array_push(arr, json_string_new(curr->task->name));
        curr = curr->next;
    }
    json_object_push(obj, "tasks", arr);
    return obj;
}

/* decode_job: Decodes the JSON object into a new job. */
Job *decode_job(json_value *obj) {
    json_value *val;
    val = json_get_value(obj, "id");
    Job *job = create_job(val->u.integer);
    val = json_get_value(obj, "tasks");
    for (unsigned int i = 0; i < val->u.array.length; i++)
        add_task(job, val->u.array.values[i]->u.string.ptr);
    return job;
}
