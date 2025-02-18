#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "worker.h"

void print_status(FILE *file);

Worker *worker;
Job *job;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "main: expected 1 argument, received %d arguments\n", argc-1);
        exit(EXIT_FAILURE);
    }

    worker = create_worker((int) strtol(argv[1],NULL,0));
    job = create_job(42,_JOB_INCOMPLETE);
    add_task(job, "task.py");
    add_task(job, "task.py");
    add_task(job, "task.py");
    add_task(job, "task2.py");
    add_task(job, "task2.py");
    add_task(job, "task2.py");
    add_task(job, "bug.py");
    add_task(job, "bug.py");
    print_status(stdout);

    if (run_job(worker,job) == -1) {
        print_status(stderr);
        exit(EXIT_FAILURE);
    }

    sleep(1);

    int i = 0;
    while (worker->status == _WORKER_WORKING) {
        if (++i > 4)
            stop(worker);
        print_status(stdout);
        sleep(5);
    }
    start(worker);
    print_status(stdout);
    sleep(5);

    free_worker(worker);
    free_job(job);
    exit(EXIT_SUCCESS);
}

void print_status(FILE *file) {
    char s[15];
    strcpy(s,"invalid status");
    if (worker->status == _WORKER_NOT_WORKING)
        strcpy(s,"not working");
    else if (worker->status == _WORKER_WORKING)
        strcpy(s,"working");
    fprintf(file, "worker status: %s\n", s);

    strcpy(s,"invalid status");
    if (job->status == _JOB_INCOMPLETE)
        strcpy(s,"incomplete");
    else if (job->status == _JOB_RUNNING)
        strcpy(s,"running");
    else if (job->status == _JOB_COMPLETED)
        strcpy(s,"completed");
    fprintf(file, "job status: %s\n", s);
}