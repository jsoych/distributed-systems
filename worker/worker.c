#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "worker.h"

/* create_running_job_node: Creates a new running job node. */
static RunningJobNode *create_running_job_node(JobNode *job_node) {
    RunningJobNode *rjob_node;
    if ((rjob_node = malloc(sizeof(RunningJobNode)) == NULL)) {
        perror("worker: create_running_job_node: malloc");
        exit(EXIT_FAILURE);
    }

    rjob_node->job_node = job_node;
    rjob_node->next = NULL;

    return rjob_node;
}

/* free_running_job_node: Frees memory allocated to the running job node. */
static void free_running_job_node(RunningJobNode *rjob_node);

/* create_running_job: Creates a new running job. */
static RunningJob *create_running_job(Worker *worker, Job *job) {
    RunningJob *rjob;
    if ((rjob = malloc(sizeof(RunningJob))) == NULL) {
        perror("worker: create_running_job: malloc");
        exit(EXIT_FAILURE);
    }

    rjob->worker = worker;
    rjob->job = job;
    rjob->head = NULL;

    return rjob;
}

/* free_running_job: Frees memory allocated to the running job. */
static void free_running_job(RunningJob *rjob) {
    if (rjob->worker->status == _WORKER_WORKING)
        stop_job(rjob->worker);

    // Free all nodes
    RunningJobNode *prev, *curr;    
    if (rjob->head != NULL) {
        curr = rjob->head;
        while (curr->next) {
            prev = curr;
            curr = curr->next;
            free(prev);
        }
        free(curr);
    }

    free(rjob);
}

/* add_node: Adds a running job node to the running job. */
static void add_node(RunningJob *rjob, JobNode *job_node) {
    RunningJobNode *new_node, *curr;
    new_node = create_running_job_node(job_node);

    // Check if the list is empty
    if (rjob->head == NULL) {
        rjob->head = new_node;
        return;
    }

    // Add the new node to the end of the list
    for (curr = rjob->head; curr->next; curr = curr->next)
        ;
    curr->next = new_node;
    
    return;
}

/* create_worker: Creates a new worker. */
Worker *create_worker(int id) {
    Worker *worker;
    if ((worker = malloc(sizeof(worker))) == NULL) {
        perror("worker: create_worker: malloc");
        exit(EXIT_FAILURE);
    }

    worker->id = id;
    worker->status = _WORKER_NOT_WORKING;
    worker->running_job = NULL;

    return worker;
}

/* free_worker: Frees the memory allocated to the worker. If the worker
    is working, then the running job is stopped. */
void free_worker(Worker *worker) {
    if (worker->status == _WORKER_WORKING)
        stop_job(worker);
    free_running_job(worker->running_job);
    free(worker);
}

/* task_status_handler: Sets the status of the task as incomplete. */
static void *task_status_handler(void *arg) {
    int *task_status = (int *) arg;
    *task_status = _TASK_INCOMPLETE;
    return NULL;
}

/* task_process_handler: Kills running task process. */
static void *task_process_handler(void *arg) {
    pid_t *id = (pid_t *) arg;

    // Kill task process
    if (kill(*id,SIGKILL) == -1) {
        perror("worker: task_process_handler: kill");
        exit(EXIT_FAILURE);
    }

    return NULL;
}

/* task_thread: Runs the task and changes the status of the task. */
static void *task_thread(void *args) {
    // Unpack args
    RunningJobNode *rjob_node = (RunningJobNode *) args;
    int *task_status = &rjob_node->job_node->status;
    Task *task = rjob_node->job_node->task;

    // Set task status to running
    *(task_status) = _TASK_RUNNING;

    // Create task process
    pid_t id;
    if ((id = fork()) == -1) {
        perror("worker: task_thread: fork");
        exit(EXIT_FAILURE);
    }

    if (id == 0)
        // Task process
        run_task(task);
    
    // Add cleanup handlers
    pthread_cleanup_push(task_status_handler, task_status);
    pthread_cleanup_push(task_process_handler, &id);

    // Wait for the task to complete
    int *status;
    if (waitpid(id,status,0) == -1) {
        perror("worker: task_thread: waitpid");
        exit(EXIT_FAILURE);
    }

    // Set task status
    if (WEXITSTATUS(*status) == 0)
        *(task_status) = _TASK_COMPLETED;
    else
        *(task_status) = _TASK_INCOMPLETE;
    
    // Remove cleanup handlers
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);

    pthread_exit(NULL);
}

/* job_thread: Runs the job and changes the status of the job. */
static void *job_thread(void *args) {
    // Unpack args
    RunningJob *rjob = (RunningJob *) args;
    Job *job = rjob->job;

    // Set job status to running
    job->status = _JOB_RUNNING;

    // Create task threads
    RunningJobNode *rnode;
    for (rnode = rjob->head; rnode; rnode = rnode->next)
        if (pthread_create(&rnode->task_tid, NULL, task_thread, rnode) != 0) {
            perror("worker: job_thread: pthread_create");
            exit(EXIT_FAILURE);
        }

    // Join all task threads
    for (rnode = rjob->head; rnode; rnode = rnode->next)
        if (pthread_join(rnode->task_tid,NULL) != 0) {
            perror("worker: job_thread: pthread_join");
            exit(EXIT_FAILURE);
        }

    // Update job status
    JobNode *node;
    for (node = job->head, job->status = _JOB_COMPLETED; node; node = node->next)
        if (node->status == _TASK_INCOMPLETE) {
            job->status = _JOB_INCOMPLETE;
            break;
        }
    
    // Set worker status
    rjob->worker->status = _WORKER_NOT_WORKING;
    
    pthread_exit(NULL);
}

/* run_job: Runs the job and returns 0. If the worker is workering then,
    run_job returns -1. */
int run_job(Worker *worker, Job *job) {
    if (worker->status == _WORKER_WORKING)
        return -1;
    
    // Check the job status
    if (job->status == _JOB_COMPLETED)
        return 0;
    
    // Free the old running job
    if (worker->running_job)
        free_running_job(worker->running_job);
    
    // Create new running job
    RunningJob *rjob = create_running_job(worker,job);

    // Add incomplete tasks to the running job
    JobNode *node;
    for (node = job->head; node; node = node->next)
        if (node->status == _TASK_INCOMPLETE)
            add_node(rjob,node);

    // Create job thread
    if (pthread_create(&rjob->job_tid, NULL, job_thread, rjob) != 0) {
        perror("worker: run_job: pthread_create");
        exit(EXIT_FAILURE);
    }

    // Detach job thread
    if (pthread_detach(rjob->job_tid) != 0) {
        perror("worker: run_job: pthread_detach");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// stop: Stops the worker and its running job.
void stop(Worker *worker) {
    if (worker->status == _WORKER_NOT_WORKING)
        return;

    if (worker->running_job == NULL)
        return;

    // Stop each running task
    RunningJobNode *node;
    for (node = worker->running_job->head; node; node = node->next) {
        if (pthread_cancel(node->task_tid) != 0) {
            perror("worker: stop: pthread_cancel");
            exit(EXIT_FAILURE);
        }
    }

    return;
}