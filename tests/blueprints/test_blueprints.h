#ifndef _TEST_BLUEPRINTS_H
#define _TEST_BLUEPRINTS_H

#include "blueprint.h"
#include "unittest.h"

// Task names
#define TASK_NAME "task.py"
#define BUG_NAME "bug.py"
#define WRITE_NAME "write.py"
#define SHORT_NAME "task.py"
#define LONG_NAME "very_very_very_very_very_very_very_very_task_name.py"

// Job ids
#define VALID_ID 33
#define INVALID_ID -13

// Result macro
#define RETURN_RESULT(result) switch((result)) { \
    case 0: return UNITTEST_SUCCESS; \
    case 1: return UNITTEST_FAILURE; \
} \
return UNITTEST_ERROR

Unittest* test_task_create(const char* name);
Unittest* test_task_runner_create(const char* name);
Unittest* test_job_create(const char* name);
Unittest* test_job_runner_create(const char* name);

#endif
