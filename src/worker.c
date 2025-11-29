#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "worker.h"

typedef enum {
    WORKER_NOT_ASSIGNED,
    WORKER_NOT_WORKING,
    WORKER_WORKING
} worker_status_t;

struct _worker;

typedef struct _running_job_node {
    job_node* task;
    pid_t pid;
    pthread_t tid;
    struct _running_job_node *next;
} running_job_node;

typedef struct _running_job {
    struct _worker *worker;
    Job *job;
    pthread_t tid;
    running_job_node *head;
} RunningJob;

typedef struct _worker {
    int id;
    worker_status_t status;
    RunningJob *running_job;
    sem_t lock;
} Worker;

/* lock: Decrements the value of the semaphore and waits if its value
    is negative. If sem_waits fails, the system exits. */
static void lock(sem_t *s, char *name) {
    if (sem_wait(s) == -1) {
        fprintf(stderr, "worker: %s: sem_wait: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* unlock: Increments the value of the semaphore and wakes up one or more
    waiting threads. */
static void unlock(sem_t *s, char *name) {
    if (sem_post(s) == -1) {
        fprintf(stderr, "worker: %s: sem_post: %s\n", name, strerror(errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* create_running_job: Creates a new running job. */
static RunningJob* create_running_job(Worker *worker) {
    RunningJob *rjob;
    if ((rjob = malloc(sizeof(RunningJob))) == NULL) {
        perror("worker: create_running_job: malloc");
        exit(EXIT_FAILURE);
    }
    rjob->worker = worker;
    rjob->job = NULL;
    rjob->head = NULL;
    return rjob;
}

/* free_running_job: Frees memory allocated to the running job. */
static void free_running_job(RunningJob *rjob) {
    if (rjob->job == NULL) {
        free(rjob);
        return;
    }
    running_job_node *prev, *curr = rjob->head;
    while (curr->next) {
        prev = curr;
        curr = curr->next;
        free(prev);
    }
    free(curr);
    free(rjob);
    return;
}

/* add_node: Adds a running job node to the running job. */
static void add(RunningJob *rjob, Task *task) {
    running_job_node *node;
    if ((node = malloc(sizeof(running_job_node))) == NULL) {
        perror("worker: add_task: malloc");
        exit(EXIT_FAILURE);
    }
    node->task = task;
    node->next = rjob->head;
    rjob->head = node;
    return;
}


/* bind: Binds the running job and job together, and chanages the worker
    status to assigned. */
static void bind(RunningJob *rjob, Job *job) {
    rjob->job = job;
    job_node *curr = job->head;
    while (curr) {
        add(rjob, curr->task);
        curr = curr->next;
    }
    rjob->worker->status = WORKER_NOT_WORKING;
    return;
}

/* unbind: Unbinds the running job and the job, changes the worker status
    to not assigned, and returns the project. */
static Job* unbind(RunningJob *rjob) {
    Job *job = rjob->job;
    rjob->worker->status = WORKER_NOT_ASSIGNED;
    rjob->job = NULL;
    running_job_node *prev, *curr = rjob->head;
    while (curr->next) {
        prev = curr;
        curr = curr->next;
        free(prev);
    }
    free(curr);
    return job;
}

/* create_worker: Creates a new worker. */
Worker* create_worker(int id) {
    Worker *worker;
    if ((worker = malloc(sizeof(worker))) == NULL) {
        perror("worker: create_worker: malloc");
        exit(EXIT_FAILURE);
    }
    worker->id = id;
    worker->status = WORKER_NOT_ASSIGNED;
    worker->running_job = create_running_job(worker);
    sem_init(&worker->lock, 0, 1);
    return worker;
}

/* free_worker: Frees the memory allocated to the worker. If the worker
    is working, then the running job is stopped. */
void free_worker(Worker *worker) {
    switch (worker->status) {
        // intentionally falling through
        case WORKER_WORKING:
            stop(worker);
        case WORKER_NOT_WORKING:
            free_job(unbind(worker->running_job));
        case WORKER_NOT_ASSIGNED:
            free_running_job(worker->running_job);
    }
    sem_destroy(&worker->lock);
    free(worker);
    return;
}

/* get_status: Returns the status of the worker. */
int get_status(Worker *worker) {
    int status;
    lock(&worker->lock, "get_status");
    status = worker->status;
    unlock(&worker->lock, "get_status");
    return status;
}

/* get_job_status: Returns the status of the running job. Otherwise,
    returns -1. */
int get_job_status(Worker *worker) {
    int status;
    lock(&worker->lock, "get_job_status");
    if (worker->status == WORKER_NOT_ASSIGNED) {
        unlock(&worker->lock, "get_job_status");
        return -1;
    }
    status = worker->running_job->job->status;
    unlock(&worker->lock, "get_job_status");
    return status;
}


/* task_status_handler: Sets the status of the task as incomplete. */
static void task_status_handler(void *arg) {
    job_node* task = (job_node *) arg;
    task->status = TASK_INCOMPLETE;
    return;
}

/* task_process_handler: Terminates the running task process. */
static void task_process_handler(void *arg) {
    running_job_node* task = (running_job_node*)arg;

    // terminate the task process
    if (kill(task->pid, SIGTERM) == -1) {
        perror("worker: task_process_handler: kill");
        exit(EXIT_FAILURE);
    }

    // reap zombie process
    if (waitpid(task->pid, NULL, 0) == -1) {
        perror("worker: task_process_handler: waitpid");
        exit(EXIT_FAILURE);
    }
    return;
}

/* task_thread: Runs the task and changes the status of the task. */
static void *task_thread(void *arg) {
    job_node* task = (job_node*)arg;
    task->status = TASK_RUNNING;

    // run task
    pid_t id = fork();
    if (id == -1) {
        perror("worker: task_thread: fork");
        exit(EXIT_FAILURE);
    }
    // child process
    if (id == 0) run_task(task);
    
    // parent process
    int rv;
    pthread_cleanup_push(task_status_handler, task);
    pthread_cleanup_push(task_process_handler, task);
    if (waitpid(id, &rv, 0) == -1) {
        perror("worker: task_thread: waitpid");
        exit(EXIT_FAILURE);
    }
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);

    // update task status
    if (WEXITSTATUS(rv) == 0)
        task->status = TASK_COMPLETED;
    else
        task->status = TASK_NOT_READY;
    return NULL;
}

/* job_status_handler: Updates the job status. */
static void job_status_handler(void* arg) {
    Job* job = (Job*)arg;
    int status = JOB_INCOMPLETE;
    job_node* curr = job->head;
    while (curr) {
        if (curr->status == TASK_NOT_READY) {
            status = JOB_NOT_READY;
            break;
        }
        curr = curr->next;
    }
    job->status = status;
    return;
}

/* job_task_handler: Cancels each running task thread. */
static void task_thread_handler(void *args) {
    RunningJob *rjob = (RunningJob *) args;
    running_job_node *curr = rjob->head;
    while (curr) {
        if (curr->task->status == TASK_RUNNING)
            pthread_cancel(curr->tid);
        curr = curr->next;
    }
    return;
}

/* job_thread: Runs the job and changes the status of the job. */
static void* job_thread(void* args) {
    RunningJob *rjob = (RunningJob *) args;

    // set job status
    rjob->job->status = JOB_RUNNING;

    // create task threads
    int old_errno;
    running_job_node *curr = rjob->head;
    while (curr) {
        switch (curr->task->status) {
            case TASK_NOT_READY:
                curr = curr->next;
                continue;
            case TASK_READY:
                old_errno = pthread_create(&curr->tid, NULL,
                    task_thread, curr->task);
                    break;
            case TASK_RUNNING:
                fprintf(stderr, "worker: job_thread: error: inconsistent task status\n");
                exit(EXIT_FAILURE);
            case TASK_COMPLETED:
                curr = curr->next;
                continue;
            case TASK_INCOMPLETE:
                old_errno = pthread_create(&curr->tid, NULL,
                    task_thread, curr->task);
                break;
        }
        if (old_errno != 0) {
            fprintf(stderr, "worker: job_thread: pthread_create: %s\n",
                strerror(old_errno));
            exit(EXIT_FAILURE);
        }
        curr = curr->next;
    }

    // join all task threads
    curr = rjob->head;
    pthread_cleanup_push(job_status_handler, rjob->job);
    pthread_cleanup_push(task_thread_handler, rjob);
    while (curr) {
        switch (curr->task->status) {
            case TASK_NOT_READY:
                curr = curr->next;
                continue;
            case TASK_READY:
                old_errno = pthread_join(curr->tid, NULL);
                break;
            case TASK_RUNNING:
                old_errno = pthread_join(curr->tid, NULL);
                break;
            case TASK_COMPLETED:
                curr = curr->next;
                continue;
            case TASK_INCOMPLETE:
                old_errno = pthread_join(curr->tid, NULL);
                break;
        }
        if (old_errno != 0) {
            fprintf(stderr, "worker: job_thread: pthread_join: %s\n",
                strerror(old_errno));
            exit(EXIT_FAILURE);
        }
        curr = curr->next;
    }
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);

    // update job and worker status
    int status = rjob->job->status;
    curr = rjob->head;
    while (curr && status != JOB_NOT_READY && status != JOB_INCOMPLETE) {
        switch (curr->task->status) {
            case TASK_NOT_READY:
                status = JOB_NOT_READY;
                continue;
            case TASK_READY:
                fprintf(stderr, "worker: job_thread: error: inconsistent task status\n");
                exit(EXIT_FAILURE);
            case TASK_RUNNING:
                fprintf(stderr, "worker: job_thread: error: orphan process %jd\n",
                    (intmax_t)curr->pid);
                exit(EXIT_FAILURE);
            case TASK_COMPLETED:
                status = JOB_COMPLETED;
                break;
            case TASK_INCOMPLETE:
                status = JOB_INCOMPLETE;
                continue;
        }
        curr = curr->next;
    }
    rjob->job->status = status;
    rjob->worker->status = WORKER_NOT_WORKING;
    return NULL;
}

/* run: Runs the job and returns 0, if the worker is not working. Otherwise,
    returns -1. */
int run(Worker *worker, Job *job) {
    lock(&worker->lock, "run");
    if (worker->status == WORKER_WORKING) {
        unlock(&worker->lock, "run");
        return -1;
    }

    if (worker->status == WORKER_NOT_WORKING)
        free(unbind(worker->running_job));
    bind(worker->running_job, job);

    // create job thread
    int old_errno = pthread_create(&worker->running_job->tid, NULL,
        job_thread, worker->running_job);
    if (old_errno != 0) {
        fprintf(stderr, "worker: run_job: pthread_create: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    worker->status = WORKER_WORKING;
    unlock(&worker->lock, "run");
    return 0;
}

/* assign: Assigns a job to the worker and returns 0, if the worker is
    not assigned. Otherwise, returns -1. */
int assign(Worker *worker, Job *job) {
    lock(&worker->lock, "assign");
    if (worker->status == WORKER_NOT_ASSIGNED) {
        bind(worker->running_job, job);
        unlock(&worker->lock, "assign");
        return 0;
    }
    unlock(&worker->lock, "assign");
    return -1;
}

/* unassign: Unassigns the worker job and returns 0, if the worker is
    not working. Otherwise, returns -1*/
int unassign(Worker *worker) {
    lock(&worker->lock, "unassign");
    if (worker->status == WORKER_NOT_WORKING) {
        free_job(unbind(worker->running_job));
        unlock(&worker->lock, "unassign");
        return 0;
    }
    unlock(&worker->lock, "unassign");
    return -1;
}

/* start: Starts the worker and returns the job status. */
int start(Worker *worker) {
    if (worker->status != WORKER_NOT_WORKING)
        return get_job_status(worker);
    
    // create job thread
    int old_errno = pthread_create(&worker->running_job->tid, NULL,
        job_thread, worker->running_job);
    if (old_errno != 0) {
        fprintf(stderr, "worker: start: pthread_create: %s\n",
            strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    worker->status = WORKER_WORKING;
    return get_job_status(worker);
}

/* stop: Stops the worker's running job and returns the job status. */
int stop(Worker *worker) {
    if (worker->status != WORKER_WORKING)
        return get_job_status(worker);

    // cancel job thread
    int old_errno;
    if ((old_errno = pthread_cancel(worker->running_job->tid)) != 0) {
        fprintf(stderr, "worker: stop: pthread_cancel: %s\n", 
            strerror(old_errno));
        return get_job_status(worker);
    }

    // join job thread
    if ((old_errno = pthread_join(worker->running_job->tid, NULL)) != 0) {
        fprintf(stderr, "worker: stop: pthread_join: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    worker->status = WORKER_NOT_WORKING;
    return get_job_status(worker);
}
