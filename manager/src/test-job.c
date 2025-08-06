#include <stdio.h>
#include <string.h>
#include "job.h"

#define BUFSIZE 1024

typedef enum {
    test_success,
    test_failure
} test_result;

void print_result(test_result, char *, char *);


int main(int argc, char *argv) {
    Job *job;
    json_value *arr, *obj, *val;
    char buf[BUFSIZE];
    memset(buf, '\0', BUFSIZE);

    // empty job
    job = create_job(0);

    // check id
    if (job->id == 0)
        print_result(test_success, "empty job id", NULL);
    else
        print_result(test_failure, "empty job id", "unexpected id");

    // check status
    if (job->status == _JOB_READY)
        print_result(test_success, "empty job status", NULL);
    else
        print_result(test_failure, "empty job status", "unexpected job status");

    // check length
    if (job->len == 0)
        print_result(test_success, "empty job length", NULL);
    else
        print_result(test_failure, "empty job length", "unexpected job length");

    // check encoding
    val = encode_job(job);
    json_serialize(buf, val);
    if (strcmp(buf, "{ \"id\": 0, \"tasks\": [] }") == 0)
        print_result(test_success, "empty job encoding", "");
    else
        print_result(test_failure, "empty job encoding", buf);
    json_value_free(val);
    free_job(job);

    // small job
    job = create_job(42);
    add_task(job, "task.py");

    // check length
    if (job->len == 1)
        print_result(test_success, "small job length", NULL);
    else
        print_result(test_failure, "small job length", "unexpected job length");

    // check encoding
    memset(buf, '\0', BUFSIZE);
    val = encode_job(job);
    json_serialize(buf, val);
    if (strcmp(buf, "{ \"id\": 42, \"tasks\": [ \"task.py\" ] }") == 0)
        print_result(test_success, "small job encoding", NULL);
    else
        print_result(test_failure, "small job encoding", buf);
    json_value_free(val);
    free_job(job);

    // large job
    job = create_job(3);
    for (int i = 0; i < 42; i++)
        add_task(job, "task.py");

    // check length
    if (job->len == 42)
        print_result(test_success, "large job length", NULL);
    else
        print_result(test_failure, "large job length", "unexpected length");

    // check encoding
    memset(buf, '\0', BUFSIZE);
    val = encode_job(job);
    json_serialize(buf, val);

    char json[BUFSIZE];
    memset(json, '\0', BUFSIZE);
    char *s = json;
    s = stpcpy(s, "{ \"id\": 3, \"tasks\": [ ");
    for (int i = 0; i < 41; i++)
        s = stpcpy(s, "\"task.py\", ");
    stpcpy(s, "\"task.py\" ] }");

    if (strcmp(buf, json) == 0)
        print_result(test_success, "large job encoding", NULL);
    else
        print_result(test_failure, "large job encoding", json);
    
    json_value_free(val);
    free_job(job);

    // decode empty job
    obj = json_object_new(0);
    val = json_integer_new(33);
    json_object_push(obj, "id", val);
    val = json_string_new("ready");
    json_object_push(obj, "status", val);
    val = json_array_new(0);
    json_object_push(obj, "tasks", val);

    job = decode_job(obj);
    if (job->id == 33)
        print_result(test_success, "decode empty job id", NULL);
    else
        print_result(test_failure, "decode empty job id", "unexpected id");
    
    if (job->status == _JOB_READY)
        print_result(test_success, "decode empty job status", NULL);
    else
        print_result(test_failure, "decode empty job status", "unexpected status");
    
    json_value_free(obj);
    free_job(job);

    // decode small job
    obj = json_object_new(0);
    val = json_integer_new(3);
    json_object_push(obj, "id", val);
    val = json_string_new("ready");
    json_object_push(obj, "status", val);
    arr = json_array_new(1);
    val = json_string_new("task.py");
    json_array_push(arr, val);
    json_object_push(obj, "tasks", arr);

    job = decode_job(obj);
    if (job->len == 1)
        print_result(test_success, "decode small job length", NULL);
    else
        print_result(test_failure, "decode small job length", "unexpected length");
    
    if (strcmp(job->head->task->name, "task.py") == 0)
        print_result(test_success, "decode small job tasks", NULL);
    else
        print_result(test_failure, "decode small job tasks", job->head->task->name);
    json_value_free(obj);
    free_job(job);

    // decode large job
    obj = json_object_new(0);
    val = json_integer_new(1966);
    json_object_push(obj, "id", val);
    val = json_string_new("not_ready");
    json_object_push(obj, "status", val);
    arr = json_array_new(32);
    for (int i = 0; i < 32; ++i) {
        val = json_string_new("task.py");
        json_array_push(arr, val);
    }
    json_object_push(obj, "tasks", arr);

    job = decode_job(obj);
    if (job->len == 32)
        print_result(test_success, "decode large job length", NULL);
    else
        print_result(test_failure, "decode large job length", "unexpected length");
    
    JobNode *curr = job->head;
    while (curr) {
        if (strcmp(curr->task->name, "task.py") != 0) {
            print_result(test_failure, "decode large job tasks", curr->task->name);
            break;
        }
        curr = curr->next;
    }
    if (curr == NULL)
        print_result(test_success, "decode large job tasks", NULL);
    json_value_free(obj);
    free_job(job);

    return 0;
}

void print_result(test_result result, char *name, char *msg) {
    switch (result) {
        case test_success:
            printf("test-job: success: %s\n", name);
            break;
        case test_failure:
            printf("test-job: failure: %s: %s\n", name, msg);
            break;
    }
    return;
}
