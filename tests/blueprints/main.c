#include <stdio.h>
#include <stdlib.h>

#include "test_task.h"

int main() {
    Unittest* ut = test_task_create("task");
    if (ut == NULL) exit(EXIT_FAILURE);

    if (unittest_run(ut) == -1) {
        fprintf(stderr, "unittest_run: Error: Failed to run unit tests\n");
        unittest_destroy(ut);
        exit(EXIT_FAILURE);
    }

    unittest_destroy(ut);
    exit(EXIT_SUCCESS);
}
