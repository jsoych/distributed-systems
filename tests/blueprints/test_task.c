#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "test_blueprints.h"
#include "json-helpers.h"

enum {
    MISSING_NAME = 0,
    INVALID_TYPE,
    SHORT_NAME,
    LONG_NAME,
    NO_NAME,
    BUG_NAME
};

const char* const TEST_NAME[] = {
    [MISSING_NAME]      = NULL,
    [INVALID_TYPE]      = NULL,
    [SHORT_NAME]        = "task.py",
    [LONG_NAME]         = "very_very_very_very_very_very_very_very_task_name.py",
    [NO_NAME]           = "",
    [BUG_NAME]          = "bug.py"
};

const char* const TEST_JSON[] = {
    [MISSING_NAME]      = "{}",
    [INVALID_TYPE]      = "[\"task.py\"]",
    [SHORT_NAME]        = "{\"name\":\"task.py\"}",
    [LONG_NAME]         = "{\"name\":\"very_very_very_very_very_very_very_very_task_name.py\"}",
    [NO_NAME]           = "{\"name\":\"\"}"
};

// Test Cases
static result_t test_case_create(unittest_case* expected) {
    Task* actual = task_create("task.py");
    int result = unittest_compare_task(actual, expected->as.task);
    task_destroy(actual);
    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_destroy(unittest_case* expected) {
    // Check NULL
    task_destroy(NULL);

    Task* task = task_create("task.py");
    if (task == NULL) return UNITTEST_ERROR;
    task_destroy(task);
    return UNITTEST_SUCCESS;
}

static result_t test_case_run(unittest_case* expected) {
    char* python = getenv("PYONEER_PYTHON");
    if (python == NULL) {
        fprintf(stderr, "test_case_run: Missing environment variable PYONEER_PYTHON\n");
        return UNITTEST_ERROR;
    }

    Task* task = task_create(TEST_NAME[SHORT_NAME]);
    if (task == NULL) {
        fprintf(stderr, "test_case_run: Unable to create task\n");
        return UNITTEST_ERROR;
    }

    int actual = task_run(task, python);
    if (actual == -1) {
        fprintf(stderr, "test_case_run: Unable to run task\n");
        task_destroy(task);
        return UNITTEST_ERROR;
    }

    task_destroy(task);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_bug(unittest_case* expected) {
    char* python = getenv("PYONEER_PYTHON");
    if (python == NULL) {
        fprintf(stderr, "test_case_bug: Missing envirnoment variable PYONEER_PYTHON\n");
        return UNITTEST_ERROR;
    }

    Task* task = task_create(TEST_NAME[BUG_NAME]);
    if (task == NULL) {
        fprintf(stderr, "test_case_bug: Unable to create task\n");
        return UNITTEST_ERROR;
    }

    int actual = task_run(task, python);
    if (actual == -1) {
        fprintf(stderr, "test_case_bug: Unable to run task\n");
        task_destroy(task);
        return UNITTEST_ERROR;
    }

    task_destroy(task);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static json_value* parse_case(int arg) {
    return json_parse(TEST_JSON[arg], strlen(TEST_JSON[arg]));
}

static result_t test_case_encode(unittest_case* expected) {
    Task* task = task_create(TEST_NAME[SHORT_NAME]);
    json_value* actual = task_encode(task);
    int result = json_value_compare(actual, expected->as.json);

    // Clean up and return result
    task_destroy(task);
    json_value_free(actual);
    if (result == 0)
        return UNITTEST_SUCCESS;
    else if (result == 1)
        return UNITTEST_FAILURE;
    return UNITTEST_ERROR;
}

static result_t test_case_missing(unittest_case* expected) {
    // Get test case object
    json_value* obj = parse_case(MISSING_NAME);
    if (obj == NULL) return UNITTEST_ERROR;

    // Decode and compare
    Task* actual = task_decode(obj);
    int result = unittest_compare_task(actual, expected->as.task);

    // Clean up and return result
    json_value_free(obj);
    task_destroy(actual);
    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_invalid(unittest_case* expected) {
    // Get test case object
    json_value* obj = parse_case(INVALID_TYPE);
    if (obj == NULL) return UNITTEST_ERROR;

    // Decode and compare
    Task* actual = task_decode(obj);
    int result = unittest_compare_task(actual, expected->as.task);

    // Clean up and return result
    json_value_free(obj);
    task_destroy(actual);
    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

Unittest* test_task_create(const char* name) {
    Unittest* ut = unittest_create(name);
    if (ut == NULL) return NULL;

    int len = strlen(TEST_NAME[SHORT_NAME]);
    Task* task = malloc(sizeof(Task) + (len + 1)*sizeof(char));
    task->status = TASK_READY;
    strcpy(task->name, TEST_NAME[SHORT_NAME]);
    task->name[len] = '\0';

    unittest_add(
        ut, "task_create - short name", test_case_create, 
        CASE_TASK, task
    );

    unittest_add(ut, "task_destroy", test_case_destroy, CASE_NONE, NULL);

    int success = 0;
    unittest_add(
        ut, "task_run - task with no bugs", test_case_run,
        CASE_INT, &success
    );

    int failed = 1;
    unittest_add(
        ut, "task_run - task with bug", test_case_bug,
        CASE_INT, &failed
    );

    json_value* short_json = parse_case(SHORT_NAME);
    unittest_add(
        ut, "task_encode - small name", test_case_encode,
        CASE_JSON, short_json
    );

    unittest_add(
        ut, "task_decode - missing name", test_case_missing,
        CASE_JSON, NULL
    );

    unittest_add(
        ut, "task_decode - invalid type", test_case_invalid,
        CASE_JSON, NULL
    );

    return ut;
}
