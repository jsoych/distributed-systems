#include <stdio.h>
#include <stdlib.h>

#include "test_blueprints.h"

int main() {
    Unittest* ut = test_task_create("task");
    if (ut == NULL) exit(EXIT_FAILURE);

    // Test tasks
    if (unittest_run(ut) == -1) {
        fprintf(stderr, "main: Error: Failed to run task unit test\n");
        unittest_destroy(ut);
        exit(EXIT_FAILURE);
    }

    unittest_destroy(ut);
    fflush(stdout);


    // Test jobs
    ut = test_job_create("job");
    if (ut == NULL) exit(EXIT_FAILURE);

    if (unittest_run(ut) == -1) {
        fprintf(stderr, "test_blueprints: Failed to run job unit test\n");
        unittest_destroy(ut);
        exit(EXIT_FAILURE);
    }
    
    unittest_destroy(ut);
    exit(EXIT_SUCCESS);
}
