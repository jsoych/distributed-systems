#include <stdio.h>
#include <string.h>
#include "project.h"

#define BUFSIZE 1024

typedef enum {
    test_success,
    test_failure
} test_result;

void print_result(test_result, char *, char *);

int main() {
    Project *proj;
    Job *job;
    char buf[BUFSIZE];
    int deps[MAXLEN];
    json_value *arr, *obj, *val;

    // empty project
    proj = create_project(0);

    // check id
    if (proj->id == 0)
        print_result(test_success, "empty project id", NULL);
    else
        print_result(test_failure, "empty project id", "unexpected id");

    // check status
    if (proj->status == _PROJECT_READY)
        print_result(test_success, "empty project status", NULL);
    else
        print_result(test_failure, "empty project status", "unexpected status");
    
    // check length
    if (proj->len == 0)
        print_result(test_success, "empty project length", NULL);
    else
        print_result(test_failure, "empty project length", "unexpected status");
    
    // check audit
    if (audit_project(proj) == 0)
        print_result(test_success, "empty project audit", NULL);
    else
        print_result(test_failure, "empty project audit", "failed audit");

    // encode project
    val = encode_project(proj);
    json_serialize(buf, val);
    
    if (strcmp(buf, "{ \"id\": 0, \"jobs\": [] }") == 0)
        print_result(test_success, "empty project encoding", NULL);
    else
        print_result(test_failure, "empty project encoding", buf);

    json_builder_free(val);
    free_project(proj);

    // small project
    proj = create_project(42);
    job = create_job(1);
    add_job(proj, job, NULL, 0);

    // check length
    if (proj->len == 1)
        print_result(test_success, "small project length", NULL);
    else
        print_result(test_failure, "small project length", "unexpected length");

    // check audit
    if (audit_project(proj) == 0)
        print_result(test_success, "small project audit", NULL);
    else
        print_result(test_failure, "small project audit", "failed audit");

    free_project(proj);

    // medium project with dependencies
    proj = create_project(1966);

    job = create_job(1);
    deps[0] = 2;
    deps[1] = 1;
    add_job(proj, job, deps, 2);

    job = create_job(2);
    deps[0] = 4;
    add_job(proj, job, deps, 1);

    job = create_job(3);
    add_job(proj, job, NULL, 0);

    job = create_job(4);
    deps[0] = 5;
    add_job(proj, job, deps, 1);

    job = create_job(5);
    add_job(proj, job, NULL, 0);

    // check length
    if (proj->len == 5)
        print_result(test_success, "medium project with job dependencies length", NULL);
    else
        print_result(test_failure, "medium project with dependencies length", "unexpected value");
    
    // check audit
    if (audit_project(proj) == 0)
        print_result(test_success, "medium project with job dependencies audit", NULL);
    else
        print_result(test_failure, "medium project with job dependencies audit", "failed audit");

    free_project(proj);

    // small project with cyclic job dependencies
    proj = create_project(1996);

    job = create_job(1);
    deps[0] = 2;
    deps[1] = 1;
    add_job(proj, job, deps, 2);
    
    job = create_job(2);
    deps[0] = 1;
    add_job(proj, job, deps, 1);

    job = create_job(3);
    add_job(proj, job, NULL, 0);

    // check audit
    if (audit_project(proj) == -1)
        print_result(test_success, "small project with cyclic job dependencies audit", NULL);
    else
        print_result(test_failure, "small project with cyclic job dependencies audit", "failed audit");

    free_project(proj);

    // large project with cyclic dependencies
    proj = create_project(13);

    job = create_job(1);
    deps[0] = 2;
    deps[1] = 3;
    add_job(proj, job, deps, 2);

    job = create_job(2);
    deps[0] = 4;
    deps[1] = 5;
    add_job(proj, job, deps, 2);

    job = create_job(3);
    add_job(proj, job, NULL, 0);

    job = create_job(4);
    add_job(proj, job, NULL, 0);

    job = create_job(5);
    deps[0] = 6;
    deps[1] = 7;
    add_job(proj, job, deps, 2);

    job = create_job(6);
    deps[0] = 2;    // cyclic dependency
    add_job(proj, job, deps, 1);

    job = create_job(7);
    deps[0] = 8;
    deps[1] = 9;
    deps[2] = 3;
    add_job(proj, job, deps, 3);

    job = create_job(8);
    add_job(proj, job, NULL, 0);

    job = create_job(9);
    deps[0] = 10;
    add_job(proj, job, deps, 1);

    job = create_job(10);
    deps[0] = 5;
    add_job(proj, job, deps, 1);

    // check audit
    if (audit_project(proj) == -1)
        print_result(test_success, "large project with cyclic job dependencies audit", NULL);
    else
        print_result(test_failure, "large project with cyclic job dependencies audit", "failed audit");

    remove_job(proj, 6);
    
    // check audit
    if (audit_project(proj) == -1)
        print_result(test_success, "missing job dependency", NULL);
    else
        print_result(test_failure, "missing job dependency", "failed audit");

    job = create_job(6);
    add_job(proj, job, NULL, 0);

    // check audit
    if (audit_project(proj) == -1)
        print_result(test_success, "adding missing job dependency", NULL);
    else
        print_result(test_failure, "adding missing job dependency", "failed audit");

    remove_job(proj, 10);
    job = create_job(10);
    add_job(proj, job, NULL, 0);

    // check audit
    if (audit_project(proj) == 0)
        print_result(test_success, "remove cycle", NULL);
    else
        print_result(test_failure, "remove cycle", "failed audit");
    
    free_project(proj);

    // max project length
    proj = create_project(64);

    for (int i = 0; i < MAXLEN; i++) {
        job = create_job(i);
        add_job(proj, job, NULL, 0);
    }

    // check length
    if (proj->len == MAXLEN)
        print_result(test_success, "max project length", NULL);
    else
        print_result(test_failure, "max project length", "unexpected length");
    
    free_project(proj);

    // decode empty project
    val = json_parse("{\"id\":0,\"jobs\":[]}", 19);
    proj = decode_project(val);

    // check id
    if (proj->id == 0)
        print_result(test_success, "decode empty project id", NULL);
    else
        print_result(test_failure, "decode empty project id", "unexpected value");
    
    // check length
    if (proj->len == 0)
        print_result(test_success, "decode empty project length", NULL);
    else
        print_result(test_failure, "decode empty project length", "unexpected length");
    
    json_value_free(val);
    free_project(proj);

    // decode small project
    val = json_parse("{\"id\":3,\"jobs\":[{\"job\":{\"id\":1,\"tasks\":[\"task.py\"]},\"dependencies\":[2]},{\"job\":{\"id\":2,\"tasks\":[\"task.py\"]},\"dependencies\":[]}]}", 129);
    proj = decode_project(val);

    // check length
    if (proj->len == 2)
        print_result(test_success, "decode small project length", NULL);
    else
        print_result(test_failure, "decode small project length", "unexpected length");

    // check audit
    if (audit_project(proj) == 0)
        print_result(test_success, "decode small project audit", NULL);
    else
        print_result(test_failure, "decode small project audit", "failed audit");

    remove_job(proj, 2);

    // check remove
    if (audit_project(proj) == -1)
        print_result(test_success, "decode small project remove job", NULL);
    else
        print_result(test_success, "decode small project remove job", "failed to remove job");

    job = create_job(2);
    deps[0] = 3;
    deps[1] = 4;
    add_job(proj, job, deps, 2);

    job = create_job(3);
    add_job(proj, job, NULL, 0);

    job = create_job(4);
    deps[0] = 1;
    add_job(proj, job, deps, 1);

    if (audit_project(proj) == -1)
        print_result(test_success, "decode small project with cycle", NULL);
    else
        print_result(test_failure, "decode small project with cycle", "failed audit");
    
    json_value_free(val);
    free_project(proj);

    return 0;
}

/* print_result: Prints the result of the test. */
void print_result(test_result result, char *name, char *msg) {
    switch (result) {
        case test_success:
            printf("test-project: success: %s\n", name);
            break;
        case test_failure:
            printf("test-project: failure: %s: %s\n", name, msg);
            break;
    }
    return;
}
