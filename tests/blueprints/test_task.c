#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "task.h"
#include "json-helpers.h"
#include "unittest.h"

enum {
    MISSING_NAME = 0,
    INVALID_TYPE,
    SHORT_NAME,
    LONG_NAME,
    NO_NAME
};

const char* const TEST_JSON[] = {
    [MISSING_NAME]      = "{}",
    [INVALID_TYPE]      = "[\"task.py\"]",
    [SHORT_NAME]        = "{\"task\":\"task.py\"}",
    [LONG_NAME]         = "{\"task\":\"very_very_very_very_very_very_very_very_task_name.py\"}",
    [NO_NAME]           = "{\"task\":\"""\"}"
};

int test_task_run(Task* task, const char* python, int *rv) {
    pid_t id = fork();
    if (id == -1) {
        perror("test_task_run: fork");
        return -1;
    }

    if (id == 0) {
        // Child process
        if (task_run(task, python) == -1) _exit(255);
    }

    int status;
    if (wait(&status) == -1) {
        perror("test_task_run: wait");
        return -1;
    }

    if (WIFEXITED(status)) {
        int code =  WEXITSTATUS(status);
        if (code == 255) {
            return -1;
        }
        *rv = code;
        return 0;
    }

    return -1;
}

static int intcmp(int* lhs, int* rhs) {
    if (*lhs == *rhs) return 0;
    return 1;
}

unittest_result test_case_run(unittest_compare cmp, const void* expected) {
    char* python = getenv("PYONEER_PYTHON");
    if (python == NULL) {
        fprintf(stderr, "test_case_run: Missing environment variable PYONEER_PYTHON\n");
        return UNITTEST_ERROR;
    }

    Task* task = task_create("task.py");
    if (task == NULL) {
        fprintf(stderr, "test_case_run: Unable to create task\n");
        return UNITTEST_ERROR;
    }

    int actual;
    int err = test_task_run(task, python, &actual);
    task_destroy(task);

    if (err == -1) {
        fprintf(stderr, "test_case_run: Unable to run task");
        return UNITTEST_ERROR;
    }

    if (cmp(actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static int taskcmp(const Task* lhs, const Task* rhs) {
    if (lhs == rhs) return 0;
    if (lhs == NULL && rhs != NULL) return 1;
    if (lhs != NULL && rhs == NULL) return 1;
    if (lhs->status == rhs->status && strcmp(lhs->name, rhs->name) == 0)
        return 0;
    return 1;
}

static json_value* parse_case(int arg) {
    return json_parse(TEST_JSON[arg], sizeof(TEST_JSON[arg]));
}

unittest_result test_case_missing(unittest_compare cmp, const void* expectec) {
    json_value* obj = parse_case(MISSING_NAME);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(obj);

}

unittest_result test_case_short(unittest_compare cmp, const void* expected) {
    json_value* obj = parse_case(SHORT_NAME);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(obj);
    if (cmp(actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_invalid(unittest_compare cmp, const void* expected) {
    json_value* obj = parse_case(INVALID_TYPE);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(obj);
    if (cmp(actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_long(unittest_compare cmp, const void* expected) {
    json_value* obj = parse_case(LONG_NAME);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(TEST_JSON[LONG_NAME]);
    if (cmp(actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

int main() {
    Unittest* ut = unittest_create("task");
    if (ut == NULL) exit(EXIT_FAILURE);

    unittest_add(
        ut, "task_decode missing name", test_case_missing,
        json_value_compare, NULL
    );

    unittest_add(
        ut, "task_decode invalid type", test_case_invalid,
        json_value_compare, NULL
    );

    Task short_name;
    short_name.status = TASK_READY;
    short_name.name = "task.py";
    unittest_add(
        ut, "task_decode short name", test_case_short, 
        json_value_compare, &short_name
    );

    Task long_name;
    long_name.status = TASK_READY;
    short_name.name = "very_very_very_very_very_very_very_very_task_name.py";
    unittest_add(
        ut, "task_decode long name", test_case_long,
        json_value_compare, &long_name
    );

    int rv = 0;
    unittest_add(ut, "task_run", test_case_run, intcmp, &rv);

    unittest_run(ut);
    unittest_destroy(ut);
    exit(EXIT_SUCCESS);
}
