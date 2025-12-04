#ifndef _UNITTEST_H
#define _UNITTEST_H

#include "blueprint.h"
#include "json.h"

#define UNITTEST_CAPACITY 8

typedef enum {
    UNITTEST_SUCCESS,
    UNITTEST_FAILURE,
    UNITTEST_ERROR
} unittest_result;

typedef int (*unittest_compare)(const void *, const void *);

typedef unittest_result (*unittest_test)(unittest_compare, const void *);

typedef struct {
    void* expected;
    unittest_compare cmp;
    const char* name;
} unittest_case;

typedef struct _unittest {
    char* name;
    int size;
    int capacity;
    unittest_test* tests;
    unittest_case** cases;
} Unittest;

Unittest* unittest_create(const char* name);
void unittest_destroy(Unittest* ut);

int unittest_add(
    Unittest* ut, const char* name, unittest_test tc, 
    unittest_compare cmp, void* expected
);
int unittest_run(Unittest* ut);

int unittest_compare_int(const void* a, const void* b);
int unittest_compare_string(const void* a, const void* b);
int unittest_compare_task(const void* a, const void* b);
int unittest_compare_json_value(const void* a, const void* b);

#endif
