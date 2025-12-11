#include <stdio.h>
#include <string.h>

#include "test_blueprints.h"
#include "json-builder.h"
#include "json-helpers.h"

extern Site* SITE;

const char* JOB_JSON = "{\"id\": 33, \"tasks\": [{\"name\": \"task.py\"}, {\"name\": \"task.py\"}, {\"name\": \"task.py\"}]}";
const char* JOB_JSON_INVALID_TASK = "{\"id\": 42, \"tasks\": [{\"key\": \"value\"}]}";

static result_t test_case_create(unittest_case* expected) {
    Job* job = job_create(VALID_ID);
    if (job == NULL) return UNITTEST_FAILURE;
    
    // Check properties
    int result = 0;
    if (job->id != VALID_ID ||
        job->status != JOB_READY ||
        job->size != 0 ||
        job->head != NULL) result = 1;
                
    job_destroy(job);
    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_invalid_id(unittest_case* expected) {
    Job* job = job_create(INVALID_ID);
    if (job == NULL) return UNITTEST_SUCCESS;
    job_destroy(job);
    return UNITTEST_FAILURE;
}

static result_t test_case_destroy(unittest_case* expected) {
    // Check NULL
    Job* job = NULL;
    job_destroy(job);

    job = job_create(VALID_ID);
    for (int i = 0; i < 10; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    job_destroy(job);
    return UNITTEST_SUCCESS;
}

static result_t test_case_get_status(unittest_case* expected) {
    Job* job = job_create(VALID_ID);
    for (int i = 0; i < 3; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    // Check ready
    if (job_get_status(job) != JOB_READY) {
        job_node* curr = job->head;
        while (curr) {
            printf("task status: %d\n", curr->task->status);
            curr = curr->next;
        }
        job_destroy(job);
        printf("failed ready check");
        return UNITTEST_FAILURE;
    }

    // Check running
    job->head->next->task->status = TASK_RUNNING;
    if (job_get_status(job) != JOB_RUNNING) {
        job_destroy(job);
        printf("failed running check");
        return UNITTEST_FAILURE;
    }

    // Check not ready
    job->head->next->task->status = TASK_NOT_READY;
    if (job_get_status(job) != JOB_NOT_READY) {
        job_destroy(job);
        printf("failed not ready check");
        return UNITTEST_FAILURE;
    }

    // Check completed
    job_node* curr = job->head;
    while (curr) {
        curr->task->status = TASK_COMPLETED;
        curr = curr->next;
    }
    
    if (job_get_status(job) != JOB_COMPLETED) {
        job_destroy(job);
        printf("failed completed check");
        return UNITTEST_FAILURE;
    }

    // Check incomplete
    job->head->next->next->task->status = TASK_INCOMPLETE;
    if (job_get_status(job) != JOB_INCOMPLETE) {
        job_destroy(job);
        printf("failed incomplete check");
        return UNITTEST_FAILURE;
    }

    job_destroy(job);
    return UNITTEST_SUCCESS;
}

static result_t test_case_run(unittest_case* expected) {
    Job* job = job_create(VALID_ID);
    JobRunner* runner = job_run(job, SITE);
    if (runner) {
        job_destroy(job);
        job_runner_destroy(runner);
        return UNITTEST_FAILURE;
    }

    // Add tasks to job
    for (int i = 0; i < 3; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    runner = job_run(job, SITE);
    if (!runner) {
        job_destroy(job);
        return UNITTEST_FAILURE;
    }
    job_runner_wait(runner);

    // Clean up and return result
    int actual = job_get_status(job);
    job_destroy(job);
    job_runner_destroy(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_bug(unittest_case* expected) {
    Job* job = job_create(VALID_ID);
    for (int i = 0; i < 4; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }
    Task* bug = task_create(BUG_NAME);
    job_add_task(job, bug);

    JobRunner* runner = job_run(job, SITE);
    job_runner_wait(runner);

    int actaul = job_get_status(job);
    job_destroy(job);
    job_runner_destroy(runner);
    if (actaul == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_stop(unittest_case* expected) {
    Job* job = job_create(VALID_ID);
    for (int i = 0; i < 3; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    JobRunner* runner = job_run(job, SITE);
    job_runner_stop(runner);
    int actual = job_get_status(job);
    
    job_destroy(job);
    job_runner_destroy(runner);
    if (actual == expected->as.integer) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_add(unittest_case* expected) {
    Job* job = job_create(VALID_ID);
    if (job->size != 0) {
        job_destroy(job);
        return UNITTEST_FAILURE;
    }

    // Add 3 tasks
    for (int i = 0; i < 3; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    if (job->size != 3) {
        job_destroy(job);
        return UNITTEST_FAILURE;
    }

    // Add 7 more tasks
    for (int i = 0; i < 7; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    if (job->size != 10) {
        job_destroy(job);
        return UNITTEST_FAILURE;
    }

    job_destroy(job);
    return UNITTEST_SUCCESS;
}

static result_t test_case_encode(unittest_case* expected) {
    Job* job = job_create(VALID_ID);
    for (int i = 0; i < 3; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }

    json_value* actual = job_encode(job);
    if (actual == NULL) {
        printf("failed to encode");
        job_destroy(job);
        return UNITTEST_FAILURE;
    }

    int result = json_value_compare(actual, expected->as.json);
    if (result == -1) {
        printf("failed to compare\n");
        job_destroy(job);
        json_value_free(actual);
        return UNITTEST_ERROR;
    }

    char buf[256] = {0};
    json_serialize(buf, actual);
    printf("actual %s\n", buf);

    json_serialize(buf, expected->as.json);
    printf("expect %s\n", buf);

    job_destroy(job);
    json_value_free(actual);
    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_decode(unittest_case* expected) {
    json_value* obj = json_parse(JOB_JSON, strlen(JOB_JSON));
    if (obj == NULL) return UNITTEST_ERROR;

    Job* actual = job_decode(obj);
    if (actual == NULL) {
        printf("failed to decode\n");
        json_value_free(obj);
        return UNITTEST_FAILURE;
    }

    int result = unittest_compare_job(actual, expected->as.job);
    if (result == -1) {
        printf("failed to compare\n");
        job_destroy(actual);
        return UNITTEST_ERROR;
    }

    job_destroy(actual);
    if (result == 0) return UNITTEST_SUCCESS;
    return UNITTEST_FAILURE;
}

static result_t test_case_invalid_task(unittest_case* expected) {
    json_value* obj = json_parse(JOB_JSON_INVALID_TASK, strlen(JOB_JSON_INVALID_TASK));
    if (obj == NULL) return UNITTEST_ERROR;

    Job* actual = job_decode(obj);
    if (actual == NULL) return UNITTEST_SUCCESS;
    job_destroy(actual);
    return UNITTEST_FAILURE;
}


Unittest* test_job_create(const char* name) {
    Unittest* ut = unittest_create(name);

    unittest_add(ut, "job_create", test_case_create,
        CASE_NONE, NULL);

    unittest_add(ut, "job_create - invalid id", test_case_invalid_id,
        CASE_NONE, NULL);

    unittest_add(ut, "job_destroy", test_case_destroy,
        CASE_NONE, NULL);

    unittest_add(ut, "job_get_status", test_case_get_status,
        CASE_NONE, NULL);

    unittest_add(ut, "job_add_task", test_case_add, CASE_NONE, NULL);

    int completed = JOB_COMPLETED;
    unittest_add(ut, "job_run", test_case_run, CASE_INT, &completed);

    int not_ready = JOB_NOT_READY;
    unittest_add(ut, "job_run - with bug", test_case_bug,
        CASE_INT, &not_ready);

    int incomplete = JOB_INCOMPLETE;
    unittest_add(ut, "job_run - stop", test_case_stop,
        CASE_INT, &incomplete);

    json_settings settings = {0};
    settings.value_extra = json_builder_extra;  /* space for json-builder state */
    char error[128];
    json_value* obj = json_parse_ex(&settings, JOB_JSON, strlen(JOB_JSON), error);

    unittest_add(ut, "job_encode", test_case_encode, CASE_JSON, obj);

    Job* job = job_create(VALID_ID);
    for (int i = 0; i < 3; i++) {
        Task* task = task_create(TASK_NAME);
        job_add_task(job, task);
    }
    
    unittest_add(ut, "job_decode", test_case_decode, CASE_JOB, job);

    unittest_add(ut, "job_decode - invalid task",
        test_case_invalid_task, CASE_NONE, NULL);

    return ut;
}
