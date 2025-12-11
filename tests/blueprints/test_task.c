#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "test_blueprints.h"
#include "json-helpers.h"

extern Site* SITE;

// Test Cases
static result_t test_case_create(unittest_case* expected) {
    Task* actual = task_create(SHORT_NAME);
    if (actual == NULL) return UNITTEST_FAILURE;
    int result = (strcmp(actual->name, SHORT_NAME) == 0 && 
        actual->status == TASK_READY) ? 0 : 1;
    task_destroy(actual);
    RETURN_RESULT(result);
}

static result_t test_case_destroy(unittest_case* expected) {
    task_destroy(NULL);
    Task* task = task_create(SHORT_NAME);
    if (task == NULL) return UNITTEST_ERROR;
    task_destroy(task);
    return UNITTEST_SUCCESS;
}

static result_t test_case_run(unittest_case* expected) {
    Task* task = task_create(SHORT_NAME);
    TaskRunner* runner = task_run(task, SITE);
    if (runner == NULL) {
        task_destroy(task);
        return UNITTEST_ERROR;
    }
    task_runner_wait(runner);
    int actual = task->status;
    task_destroy(task);
    task_runner_destroy(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_bug(unittest_case* expected) {
    Task* task = task_create(BUG_NAME);
    TaskRunner* runner = task_run(task, SITE);
    task_runner_wait(runner);
    int actual = task->status;
    task_destroy(task);
    task_runner_destroy(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_write(unittest_case* expected) {
    Task* task = task_create(WRITE_NAME);
    TaskRunner* runner = task_run(task, SITE);
    task_runner_wait(runner);
    int actual = task->status;
    task_destroy(task);
    task_runner_destroy(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_encode(unittest_case* expected) {
    Task* task = task_create(SHORT_NAME);
    json_value* actual = task_encode(task);
    int result = json_value_compare(actual, expected->as.json);
    task_destroy(task);
    json_value_free(actual);
    RETURN_RESULT(result);
}

static result_t test_case_encode_status(unittest_case* expected) {
    Task* task = task_create(SHORT_NAME);
    json_value* actual = task_encode_status(task);
    int result = json_value_compare(actual, expected->as.json);
    task_destroy(task);
    RETURN_RESULT(result);
}

static result_t test_case_encode_bug(unittest_case* expected) {
    Task* task = task_create(BUG_NAME);
    TaskRunner* runner = task_run(task, SITE);
    task_runner_wait(runner);
    json_value* actual = task_encode_status(task);
    int result = json_value_compare(actual, expected->as.json);
    task_destroy(task);
    task_runner_destroy(runner);
    RETURN_RESULT(result);
}

static result_t test_case_decode(unittest_case* expected) {
    json_value* obj = json_object_new(0);
    json_object_push_string(obj, "name", SHORT_NAME);
    Task* task = task_decode(obj);
    if (task == NULL) {
        json_value_free(obj);
        return UNITTEST_FAILURE;
    }
    int result = (strcmp(task->name, SHORT_NAME) == 0 &&
        task->status == TASK_READY) ? 0 : 1;
    json_value_free(obj);
    task_destroy(task);
    RETURN_RESULT(result);
}

static result_t test_case_missing(unittest_case* expected) {
    json_value* obj = json_object_new(0);
    Task* actual = task_decode(obj);
    int result = unittest_compare_task(actual, expected->as.task);
    json_value_free(obj);
    task_destroy(actual);
    RETURN_RESULT(result);
}

static result_t test_case_invalid(unittest_case* expected) {
    json_value* obj = json_object_new(0);
    json_object_push_string(obj, "name", "");
    Task* actual = task_decode(obj);
    int result = unittest_compare_task(actual, expected->as.task);
    task_destroy(actual);
    json_value_free(obj);
    RETURN_RESULT(result);
}

Unittest* test_task_create(const char* name) {
    Unittest* ut = unittest_create(name);
    if (ut == NULL) return NULL;

    unittest_add(ut, "task_create", test_case_create, CASE_NONE, NULL);

    unittest_add(ut, "task_destroy", test_case_destroy, CASE_NONE, NULL);

    int completed = TASK_COMPLETED;
    unittest_add(ut, "task_run", test_case_run, CASE_INT, &completed);

    int not_ready = TASK_NOT_READY;
    unittest_add(ut, "task_run - with bug", test_case_bug, CASE_INT,
        &not_ready);
    
    unittest_add(ut, "task_run - write to directory", test_case_write,
            CASE_INT, &completed);

    json_value* encode = json_object_new(0);
    json_object_push_string(encode, "name", SHORT_NAME);
    unittest_add(ut, "task_encode", test_case_encode, CASE_JSON, encode);

    json_value* status = json_object_new(0);
    json_object_push_string(status, "status", "ready");
    unittest_add(ut, "task_encode_status", test_case_encode_status,
        CASE_JSON, status);

    json_value* bug = json_object_new(0);
    json_object_push_string(bug, "status", "not_ready");
    unittest_add(ut, "task_encoce_status - with bug", test_case_encode_bug,
        CASE_JSON, bug);

    unittest_add(ut, "task_decode - missing name", test_case_missing,
        CASE_JSON, NULL);

    unittest_add(ut, "task_decode - invalid type", test_case_invalid,
        CASE_JSON, NULL);
    
    return ut;
}
