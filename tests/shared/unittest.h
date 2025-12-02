#ifndef _UNITTEST_H
#define _UNITTEST_H

#define UNITTEST_CAPACITY 8

typedef enum {
    UNITTEST_SUCCESS,
    UNITTEST_FAILURE,
    UNITTEST_ERROR
} unittest_result;

typedef int (*unittest_compare)(void *, void *);

typedef unittest_result (*unittest_test)(unittest_compare, const void *);

typedef struct {
    void* expected;
    unittest_compare cmp;
    char* name;
} unittest_args;

typedef struct _unittest {
    char* name;
    int size;
    int capacity;
    unittest_test* tests;
    unittest_args** cases;
} Unittest;


Unittest* unittest_create(const char* name);
void unittest_destroy(Unittest* ut);

int unittest_add(
    Unittest* ut, const char* name, unittest_test tc, 
    unittest_compare cmp, void* expected
);
int unittest_run(Unittest* ut);

#endif
