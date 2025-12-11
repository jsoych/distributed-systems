#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "suite.h"
#include "test_blueprints.h"

Site* SITE;

int main() {
    SITE = site_create(
        getenv("PYONEER_PYTHON"),
        getenv("PYONEER_TASK_DIR"),
        getenv("PYONEER_WORKING_DIR")
    );
    
    if (!SITE) {
        fprintf(stderr, "main: Error: Missing environment variables\n");
        exit(EXIT_FAILURE);
    }

    Suite* suite = suite_create(4 /* number of unit tests */);
    if (suite == NULL) {
        exit(EXIT_FAILURE);
    }

    Unittest* task = test_task_create("task");
    Unittest* task_runner = test_task_runner_create("task runner");
    Unittest* job = test_job_create("job");
    Unittest* job_runner = test_job_runner_create("job runner");

    if (!task || !task_runner || !job || !job_runner) {
        unittest_destroy(task);
        unittest_destroy(task_runner);
        unittest_destroy(job);
        suite_destroy(suite);
        exit(EXIT_FAILURE);
    }

    suite_add(suite, task);
    suite_add(suite, task_runner);
    suite_add(suite, job);
    suite_add(suite, job_runner);

    suite_run(suite);
    
    suite_destroy(suite);
    site_destroy(SITE);
    exit(EXIT_SUCCESS);
}
