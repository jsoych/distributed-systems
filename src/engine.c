#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "engine.h"

/* engine_create: Creates a new engine. */
Engine* engine_create() {
    Engine* eng = malloc(sizeof(Engine));
    if (eng == NULL) return NULL;
    eng->job_id = -1;
    eng->job_thread = -1;
    eng->runtime = NULL;
    return eng;
}

/* engine_destroy: Destroys the engine and releases all of its resources. */
void engine_destroy(Engine* eng) {
    if (eng->runtime == NULL) {
        free(eng);
        return;
    }
    engine_stop(eng);
    free(eng);
}

/* task_handler: Terminates the running task process. */
static void task_handler(void *arg) {
    Task* task = (Task*)arg;
    task_stop(task);
}

/* task_thread: Runs the task and changes the status of the task. */
static void* task_thread(void *arg) {
    Task* task = (Task*)arg;
    task->status = TASK_RUNNING;

    const char* python = getenv("PYONEER_PYTHON");
    if (python == NULL) {
        fprintf(stderr, "task_thread: Error: Missing environment variable PYONEER_PYTHON\n");
        task->status = TASK_READY;
        return NULL;
    }

    pthread_cleanup_push(task_handler, task);
    task_run(task, python);
    pthread_cleanup_pop(0);
    return NULL;
}

/* job_thread: Runs the job and changes the status of the job. */
static void* job_thread(void* args) {
    engine_runtime* rt = (engine_runtime*)args;
    rt->task_count = 0;
    job_node* curr = rt->job->head;
    while (curr) {
        int err = pthread_create(&rt->task_threads[rt->task_count], NULL,
            task_thread, curr->task);
        if (err != 0) {
            fprintf(stderr, "job_thread: pthread_create: %s\n", strerror(err));
            for (int j = 0; j < rt->task_count; j++)
                pthread_cancel(rt->task_threads[j]);
            break;
        }
        rt->task_count++;
        curr = curr->next;
    }


    for (int i = 0; i < rt->task_count; i++)
        pthread_join(rt->task_threads[i], NULL);
    return NULL;
}


/* engine_run: Creates a new runtime (runs the job) and returns 0, if
    the job thread is successfully created. Otherwise, returns -1. */
int engine_run(Engine* eng, Job* job) {
    if (eng->runtime != NULL || job == NULL) return -1;
    if (job->size == 0) return 0;

    // Create runtime
    eng->runtime = malloc(sizeof(engine_runtime) + (job->size)*sizeof(pthread_t));
    if (eng->runtime == NULL) {
        perror("engine_run: malloc");
        return -1;
    }
    eng->job_id = job->id;
    eng->runtime->job = job;

    // Run the job
    int err = pthread_create(&eng->job_thread, NULL, job_thread, job);
    if (err != 0) {
        fprintf(stderr, "engine_run: pthread_create: %s\n", strerror(err));
        return -1;
    }
    return 0;
}

/* engine_stop: Stops the engine runtime and returns 0, if successful. 
    Otherwise, returns -1. */
int engine_stop(Engine* eng) {
    if (eng->job_id == -1 || eng->job_thread == -1 ||
        eng->runtime == NULL) return 0;

    // Cancel all task threads
    for (int i = 0; i < eng->runtime->task_count; i++)
        pthread_cancel(eng->runtime->task_threads[i]);
    
    int err = pthread_join(eng->job_thread, NULL);
    if (err != 0) {
        fprintf(stderr, "engine_destroy: pthread_join: %s", strerror(err));
        eng->job_id = -1;
        eng->job_thread = -1;
        free(eng->runtime);
        return -1;
    }

    eng->job_id = -1;
    eng->job_thread = -1;
    free(eng->runtime);
    return 0;
}
