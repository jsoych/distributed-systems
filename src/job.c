#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "job.h"
#include "json-builder.h"
#include "json-helpers.h"

/* job_create: Creates a new job and returns it, if the id is positive
    (id > 0). Otherwise, returns NULL. */
Job* job_create(int id) {
    if (id <= 0) return NULL;

    // Create new job
    Job* job = malloc(sizeof(Job));
    if (job == NULL) {
        perror("job_create: malloc");
        return NULL;
    }

    job->id = id;
    job->status = JOB_READY;
    job->size = 0;
    job->head = NULL;
    job->tail = NULL;
    return job;
}

/* job_destroy: Frees the memory allocated to the job. */
void job_destroy(Job *job) {
    if (job == NULL) return;

    job_node* curr = job->head;
    while (curr) {
        job_node* prev = curr;
        curr = curr->next;
        task_destroy(prev->task);
        free(curr);
    }
    free(job);
}

/* update_status: Updates the job status. The job is running, not ready or 
    incomplete, if one its tasks is running, not ready or incomplete, and
    is completed if all of its tasks are completed. Lastly, a job is ready
    when all of its tasks completed and there is at one ready task. */
static void update_status(Job* job) {
    if (job->size == 0) return;
    
    // Count each task state
    int status[5] = {0};
    job_node* curr = job->head;
    while (curr) {
        switch (curr->task->status) {
            case TASK_NOT_READY:
                status[TASK_NOT_READY]++;
                break;
            case TASK_READY:
                status[TASK_READY]++;
                break;
            case TASK_RUNNING:
                status[TASK_RUNNING]++;
                break;
            case TASK_COMPLETED:
                status[TASK_COMPLETED]++;
                break;
            case TASK_INCOMPLETE:
                status[TASK_INCOMPLETE]++;
                break;
        }
        curr = curr->next;
    }

    job->status =   (status[TASK_RUNNING] > 0)      ? JOB_RUNNING :
                    (status[TASK_NOT_READY] > 0)    ? JOB_NOT_READY :
                    (status[TASK_INCOMPLETE] > 0)   ? JOB_INCOMPLETE :
                    (status[TASK_READY] > 0)        ? JOB_READY : 
                    /* all tasks are completed */     JOB_COMPLETED;
}

/* job_get_status: Gets the updated status of the job. */
int job_get_status(Job* job) {
    update_status(job);
    return job->status;
}

/* job_add_task: Adds the task to the job and returns 0, if the task is
    successfully added to the job. Otherwise, returns -1. */
int job_add_task(Job *job, Task* task) {
    job_node* node = malloc(sizeof(job_node));
    if (node == NULL) {
        perror("job_add_task: malloc");
        return -1;
    }

    node->task = task;
    node->next = NULL;
    if (job->size++ == 0) {
        job->head = node;
        job->tail = node;
        return 0;
    }
    job->tail->next = node;
    job->tail = node;
    return 0;
}

/* job_status_map: Maps job status codes to its corresponding
    JSON value. */
json_value* job_status_map(int status) {
    switch (status) {
        case JOB_NOT_READY:     return json_string_new("not_ready");
        case JOB_READY:         return json_string_new("ready");
        case JOB_RUNNING:       return json_string_new("running");
        case JOB_COMPLETED:     return json_string_new("completed");
        case JOB_INCOMPLETE:    return json_string_new("incomplete");
        default:                return json_null_new();
    }
}

/* job_encode_status: Encode the job status. */
json_value* job_encode_status(Job* job) {
    json_value* obj = json_object_new(0);
    if (obj == NULL) return NULL;
    json_value* status = job_status_map(job_get_status(job));
    json_object_push(obj, "status", status);
    json_value* tasks = json_array_new(job->size);
    job_node* curr = job->head;
    while (curr) {
        json_value* task_status = task_encode_status(curr->task);
        json_object_push_string(task_status, "name", curr->task->name);
        json_array_push(tasks, task_status);
        curr = curr->next;
    }
    json_object_push(obj, "tasks", tasks);
    return obj;
}

/* job_encode: Encodes the job as a JSON object. */
json_value* job_encode(const Job *job) {
    json_value *obj = json_object_new(0);
    if (obj == NULL) return NULL;
    json_object_push_integer(obj, "id", job->id);

    // Add tasks
    json_value* arr = json_array_new(0);
    job_node *curr = job->head;
    while (curr) {
        json_value* val = task_encode(curr->task);
        json_array_push(arr, val);
        curr = curr->next;
    }

    json_object_push(obj, "tasks", arr);
    return obj;
}

/* job_decode: Decodes the JSON object into a new job. */
Job* job_decode(const json_value* obj) {
    json_value* id = json_object_get_value(obj, "id");
    json_value* tasks = json_object_get_value(obj, "tasks");
    if (id == NULL || tasks == NULL) return NULL;
    if (id->type != json_integer || tasks->type != json_array) return NULL;

    Job* job = job_create(id->u.integer);
    for (unsigned int i = 0; i < tasks->u.array.length; i++) {
        Task* task = task_decode(tasks->u.array.values[i]);
        if (task == NULL) {
            job_destroy(job);
            return NULL;
        }

        if (job_add_task(job, task) == -1) {
            job_destroy(job);
            return NULL;
        }
    }

    return job;
}

static void task_runner_handler(void* arg) {
    TaskRunner* runner = (TaskRunner*)arg;
    task_runner_destroy(runner);
}

static void job_runner_handler(void* arg) {
    JobRunner* runner = (JobRunner*)arg;
    runner->exit_code = JOB_EXIT_CANCELLED;
}

static void* job_runner_thread(void* arg) {
    JobRunner* runner = (JobRunner*)arg;
    job_node* curr = runner->job->head;
    while (curr) {
        int status = task_get_status(curr->task);

        // Run task
        if (status == TASK_READY || status == TASK_INCOMPLETE) {
            TaskRunner* tr = task_run(curr->task, runner->job_site);
            if (tr == NULL) {
                runner->exit_code = TASK_EXIT_FAILURE;
                pthread_exit(&runner->exit_code);
            }

            // Add cleanup handlers
            pthread_cleanup_push(job_runner_handler, runner);
            pthread_cleanup_push(task_runner_handler, tr);
            task_runner_wait(tr);
            pthread_cleanup_pop(1);
            pthread_cleanup_pop(0);
        }
        
        // Inconsistent state
        if (status == TASK_NOT_READY || status == TASK_RUNNING) {
            runner->exit_code = JOB_EXIT_FAILURE;
            pthread_exit(&runner->exit_code);
        }

        curr = curr->next;
    }
    runner->exit_code = EXIT_SUCCESS;
    pthread_exit(&runner->exit_code);
}

/* job_run: Runs the job and returns a new job runner. */
JobRunner* job_run(Job* job, Site* site) {
    if (!site || job->size == 0) return NULL;

    JobRunner* runner = malloc(sizeof(JobRunner) +
        job->size*sizeof(TaskRunner*));
    if (!runner) {
        perror("job_run: malloc");
        return NULL;
    }

    runner->job = job;
    runner->job_site = site;
    runner->exit_code = -1;

    int err = pthread_create(&runner->job_tid, NULL, job_runner_thread, runner);
    if (err != 0) {
        fprintf(stderr, "job_run: pthread_create: %s", strerror(err));
        return NULL;
    }

    return runner;
}

/* job_runner_wait: Waits for the job to finish. */
void job_runner_wait(JobRunner* runner) {
    if (!runner) return;
    pthread_join(runner->job_tid, NULL);
}

/* job_runner_stop: Stops the job. */
void job_runner_stop(JobRunner* runner) {
    if (!runner) return;
    pthread_cancel(runner->job_tid);
    job_runner_wait(runner);
}

/* job_runner_start: Starts the job. */
void job_runner_start(JobRunner* runner) {
    if (!runner) return;
    job_runner_stop(runner);
    pthread_create(&runner->job_tid, NULL, job_runner_thread, runner);
}

/* job_runner_destroy: Destroys the job runner and releases all of its
    resources. */
void job_runner_destroy(JobRunner* runner) {
    if (runner == NULL) return;
    job_runner_stop(runner);
    free(runner);
}
