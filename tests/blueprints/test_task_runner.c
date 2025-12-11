#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_blueprints.h"

extern Site* SITE;

static result_t test_case_wait(unittest_case* expected) {
    Task* task = task_create(SHORT_NAME);
    TaskRunner* runner = task_run(task, SITE);
    if (runner == NULL) {
        printf("failed to create runner");
        task_destroy(task);
        return UNITTEST_ERROR;
    }
    task_runner_wait(runner);
    int actual = runner->exit_code;
    task_destroy(task);
    free(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_bug(unittest_case* expected) {
    Task* task = task_create(BUG_NAME);
    TaskRunner* runner = task_run(task, SITE);
    task_runner_wait(runner);
    int actual = runner->exit_code;
    task_destroy(task);
    free(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_stop(unittest_case* expected) {
    Task* task = task_create(SHORT_NAME);
    TaskRunner* runner = task_run(task, SITE);
    task_runner_stop(runner);
    int actual = task->status;
    task_destroy(task);
    free(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_destroy(unittest_case* expected) {
    Task* task = task_create(SHORT_NAME);
    TaskRunner* runner = task_run(task, SITE);
    task_runner_destroy(runner);
    int actual = task->status;
    task_destroy(task);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_not_ready(unittest_case* expected) {
    Task* task = task_create(BUG_NAME);
    TaskRunner* runner = task_run(task, SITE);
    task_runner_wait(runner);
    int actual = task->status;
    task_destroy(task);
    task_runner_destroy(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

Unittest* test_task_runner_create(const char* name) {
    Unittest* ut = unittest_create(name);
    if (ut == NULL) return NULL;

    int success = 0;
    unittest_add(ut, "task_runner_wait", test_case_wait, CASE_INT, &success);

    int bug = 1;
    unittest_add(ut, "task_runner_wait - with bug", test_case_bug,
        CASE_INT, &bug);

    int incomplete = TASK_INCOMPLETE;
    unittest_add(ut, "task_runner_stop", test_case_stop, CASE_INT, &incomplete);

    unittest_add(ut, "task_destroy", test_case_destroy, CASE_INT, &incomplete);

    return ut;
}
