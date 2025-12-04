#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "task.h"
#include "json.h"
#include "unittest.h"

enum {
    MISSING_NAME = 0,
    INVALID_TYPE,
    SHORT_NAME,
    LONG_NAME,
    NO_NAME,
    BUG_NAME
};

char* const TEST_NAME[] = {
    [MISSING_NAME]      = NULL,
    [INVALID_TYPE]      = NULL,
    [SHORT_NAME]        = "task.py",
    [LONG_NAME]         = "very_very_very_very_very_very_very_very_task_name.py",
    [NO_NAME]           = "",
    [BUG_NAME]          = "bug.py"
};

char* const TEST_JSON[] = {
    [MISSING_NAME]      = "{}",
    [INVALID_TYPE]      = "[\"task.py\"]",
    [SHORT_NAME]        = "{\"name\":\"task.py\"}",
    [LONG_NAME]         = "{\"name\":\"very_very_very_very_very_very_very_very_task_name.py\"}",
    [NO_NAME]           = "{\"name\":\"\"}"
};

int test_task_run(Task* task, const char* python, int *rv) {
    // Parent child redirection pipe
    int parent_child_pipe[2];
    if (pipe(parent_child_pipe) == -1) {
        perror("test_task_run: pipe");
        return -1;
    }

    pid_t id = fork();
    if (id == -1) {
        perror("test_task_run: fork");
        close(parent_child_pipe[0]);
        close(parent_child_pipe[1]);
        return -1;
    }

    if (id == 0) {
        // Child process

        // Redirect stdout and stderr
        close(parent_child_pipe[0]);
        if (dup2(parent_child_pipe[1], STDOUT_FILENO) == -1) _exit(253);
        if (dup2(parent_child_pipe[1], STDERR_FILENO) == -1) _exit(254);
        close(parent_child_pipe[1]);

        // task_run overlays the child process with a running python script
        if (task_run(task, python) == -1) _exit(255);

        // python script
    }

    // Parent process
    close(parent_child_pipe[1]);

    int status;
    if (wait(&status) == -1) {
        perror("test_task_run: wait");
        return -1;
    }

    char buf[256];
    int n = read(parent_child_pipe[0], buf, sizeof(buf) - 1);
    if (n == -1) {
        perror("test_task_run: read");
        close(parent_child_pipe[0]);
        return -1;
    }

    buf[n] = '\0';
    printf("%s", buf);
    close(parent_child_pipe[0]);
    

    if (WIFEXITED(status)) {
        int code =  WEXITSTATUS(status);
        if (code >= 243 && code <= 255) {
            fprintf(stderr, "test_task_run: Error: Child process exited with code (%d)\n", code);
            return -1;
        }
        *rv = code;
        return 0;
    }

    fprintf(stderr, "test_task_run: Error: Child process did not exit as expected\n");
    return -1;
}

// Test Cases
unittest_result test_case_create(unittest_compare cmp, const void* expected) {
    Task* actual = task_create("task.py");
    int result = cmp(actual, expected);
    task_destroy(actual);

    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_run(unittest_compare cmp, const void* expected) {
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

    int actual;
    int err = test_task_run(task, python, &actual);
    task_destroy(task);

    if (err == -1) {
        fprintf(stderr, "test_case_run: Unable to run task\n");
        return UNITTEST_ERROR;
    }

    if (cmp(&actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_bug(unittest_compare cmp, const void* expected) {
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

    int actual;
    int err = test_task_run(task, python, &actual);

    if (err == -1) {
        fprintf(stderr, "test_case_bug: Unable to run task\n");
        return UNITTEST_ERROR;
    }

    if (cmp(&actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static json_value* parse_case(int arg) {
    return json_parse(TEST_JSON[arg], strlen(TEST_JSON[arg]));
}

unittest_result test_case_missing(unittest_compare cmp, const void* expected) {
    json_value* obj = parse_case(MISSING_NAME);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(obj);
    json_value_free(obj);

    if (cmp(actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_invalid(unittest_compare cmp, const void* expected) {
    json_value* obj = parse_case(INVALID_TYPE);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(obj);
    json_value_free(obj);

    if (cmp(actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_short(unittest_compare cmp, const void* expected) {
    json_value* obj = parse_case(SHORT_NAME);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(obj);
    json_value_free(obj);

    if (cmp(actual, expected) == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_long(unittest_compare cmp, const void* expected) {
    json_value* obj = parse_case(LONG_NAME);
    if (obj == NULL) return UNITTEST_ERROR;
    Task* actual = task_decode(obj);
    int result = cmp(actual, expected);
    json_value_free(obj);
    task_destroy(actual);
    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

unittest_result test_case_encode(unittest_compare cmp, const void* expected) {
    Task* task = task_create(TEST_NAME[SHORT_NAME]);
    json_value* actual = task_encode(task);
    int result = cmp(actual, expected);
    task_destroy(task);
    json_value_free(actual);
    if (result == 0)
        return UNITTEST_SUCCESS;
    else if (result == 1)
        return UNITTEST_FAILURE;
    return UNITTEST_ERROR;
}

int main() {
    Unittest* ut = unittest_create("task");
    if (ut == NULL) exit(EXIT_FAILURE);

    Task short_name;
    short_name.status = TASK_READY;
    short_name.name = TEST_NAME[SHORT_NAME];
    unittest_add(
        ut, "task_create short name", test_case_create, 
        unittest_compare_task, &short_name
    );

    int success = 0;
    unittest_add(
        ut, "task_run - task with no bugs", test_case_run,
        unittest_compare_int, &success
    );

    int failed = 1;
    unittest_add(
        ut, "task_run - task with bug", test_case_bug,
        unittest_compare_int, &failed
    );

    json_value* short_json = parse_case(SHORT_NAME);
    unittest_add(
        ut, "task_encode - small name", test_case_encode,
        unittest_compare_json_value, short_json
    );

    unittest_add(
        ut, "task_decode - missing name", test_case_missing,
        unittest_compare_json_value, NULL
    );

    unittest_add(
        ut, "task_decode - invalid type", test_case_invalid,
        unittest_compare_json_value, NULL
    );

    unittest_run(ut);

    json_value_free(short_json);
    unittest_destroy(ut);
    exit(EXIT_SUCCESS);
}
