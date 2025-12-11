#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_blueprints.h"

extern Site* SITE;

static Job* create_dummy(int id, int ntasks, int nbugs) {
    Job* job = job_create(id);
    for (int i = 0; i < ntasks; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    for (int i = 0; i < nbugs; i++) {
        Task* task = task_create(BUG_NAME);
        job_add_task(job, task);
    }

    return job;
}

static result_t test_case_wait(unittest_case* expected) {
    Job* job = create_dummy(VALID_ID, 3, 0);
    JobRunner* runner = job_run(job, SITE);
    if (runner == NULL) {
        job_destroy(job);
        return UNITTEST_ERROR;
    }
    job_runner_wait(runner);
    if (runner->exit_code == JOB_EXIT_FAILURE) {
        job_destroy(job);
        return UNITTEST_ERROR;
    }
    int actual = runner->exit_code;
    job_destroy(job);
    free(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_stop(unittest_case* expected) {
    Job* job = create_dummy(VALID_ID, 3, 0);
    JobRunner* runner = job_run(job, SITE);
    if (runner == NULL) {
        job_destroy(job);
        return UNITTEST_ERROR;
    }
    job_runner_stop(runner);
    int actual = runner->exit_code;
    job_destroy(job);
    free(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_start(unittest_case* expected) {
    Job* job = create_dummy(VALID_ID, 2, 1);
    JobRunner* runner = job_run(job, SITE);
    job_runner_stop(runner);
    job_runner_start(runner);
    job_runner_wait(runner);
    int actual = runner->exit_code;
    job_destroy(job);
    free(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_destory(unittest_case* expected) {
    Job* job = create_dummy(VALID_ID, 3, 2);
    JobRunner* runner = job_run(job, SITE);
    if (!runner) {
        job_destroy(job);
        return UNITTEST_ERROR;
    }
    job_runner_destroy(runner);
    job_destroy(job);
    return UNITTEST_SUCCESS;
}

Unittest* test_job_runner_create(const char* name) {
    Unittest* ut = unittest_create(name);
    if (!ut) return NULL;

    int success = EXIT_SUCCESS;
    unittest_add(ut, "job_runner_wait", test_case_wait, CASE_INT, &success);

    int cancelled = JOB_EXIT_CANCELLED;
    unittest_add(ut, "job_runner_stop", test_case_stop, CASE_INT, &cancelled);

    unittest_add(ut, "job_runner_start", test_case_start, CASE_INT, &success);

    unittest_add(ut, "job_runner_destroy", test_case_destory, CASE_INT, &cancelled);
    
    return ut;
}
