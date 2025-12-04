#ifndef _UNITTEST_H
#define _UNITTEST_H

#include "blueprint.h"
#include "json.h"

#define UNITTEST_CAPACITY 8

typedef enum {
    UNITTEST_SUCCESS,
    UNITTEST_FAILURE,
    UNITTEST_ERROR
} result_t;

typedef enum {
    CASE_INT,
    CASE_NUM,
    CASE_STR,
    CASE_JSON,
    CASE_TASK,
    CASE_JOB
} case_t;

typedef struct {
    int type;
    union {
        int integer;
        double number;
        char* string;
        json_value* json;
        Task* task;
        Job* job;
    } as;
    char name[];
} unittest_case;

typedef result_t (*unittest_test)(unittest_case*);

typedef struct _unittest {
    int size;
    int capacity;
    unittest_test* tests;
    unittest_case** cases;
    char name[];
} Unittest;

Unittest* unittest_create(const char* name);
void unittest_destroy(Unittest* ut);

int unittest_add(Unittest* ut, const char* name, unittest_test tc, 
    case_t type, void* expected);
int unittest_run(Unittest* ut);

int unittest_compare_task(const Task* a, const Task* b);

#endif
