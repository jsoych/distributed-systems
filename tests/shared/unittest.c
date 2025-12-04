#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "unittest.h"
#include "json-helpers.h"

#define CHECK_NULL(a, b) do {               \
    if (a == b) return 0;                   \
    if (a == NULL && b != NULL) return 1;   \
    if (a != NULL && b == NULL) return 1;   \
} while (0)                                 \

/* unittest_create: Creates a new unittest. */
Unittest* unittest_create(const char* name) {
    Unittest* ut = malloc(sizeof(Unittest));
    if (ut == NULL) {
        perror("unittest_create: malloc");
        return NULL;
    }

    int len = strlen(name);
    ut->name = malloc((len + 1)*sizeof(char));
    if (ut->name == NULL) {
        perror("unittest_create: malloc");
        free(ut);
        return NULL;
    }

    strcpy(ut->name, name);
    ut->name[len] = '\0';
    ut->size = 0;
    ut->capacity = UNITTEST_CAPACITY;
    ut->tests = malloc(ut->capacity*sizeof(unittest_test));
    if (ut->tests == NULL) {
        perror("unittest_create: malloc");
        free(ut->name);
        free(ut);
        return NULL;
    }

    ut->cases = malloc(ut->capacity*sizeof(unittest_case));
    if (ut->cases == NULL) {
        perror("unittest_create: malloc");
        free(ut->name);
        free(ut->tests);
        free(ut);
        return NULL;
    }

    for (int i = 0; i < ut->capacity; i++) {
        ut->tests[i] = NULL;
        ut->cases[i] = NULL;
    }

    return ut;
}

/* unittest_destroy: Destroys the unittest and frees all its resources. */
void unittest_destroy(Unittest* ut) {
    if (ut == NULL) return;
    free(ut->name);
    free(ut->tests);
    for (int i = 0; i < ut->size; i++) free(ut->cases[i]);
    free(ut);
}

/* unittest_add: Adds a new test case to the unittest. */
int unittest_add(
    Unittest* ut, const char* name, unittest_test tc, 
    unittest_compare cmp, void* expected
) {
    if (ut->size == ut->capacity) {
        int capacity = 2*ut->capacity;
        unittest_test* tests = malloc(capacity*sizeof(unittest_test));
        if (tests == NULL) {
            perror("unittest_add: malloc");
            return -1;
        }

        unittest_case** cases = malloc(capacity*sizeof(unittest_case*));
        if (cases == NULL) {
            perror("unittest_add: malloc");
            free(tests);
            return -1;
        }

        memcpy(tests, ut->tests, ut->size*sizeof(unittest_test));
        free(ut->tests);
        ut->tests = tests;

        memcpy(cases, ut->cases, ut->size*sizeof(unittest_case*));
        free(ut->cases);
        ut->cases = cases;

        for (int i = ut->size; i < capacity; i++) {
            ut->tests[i] = NULL;
            ut->cases[i] = NULL;
        }

        ut->capacity = capacity;
    }

    unittest_case* args = malloc(sizeof(unittest_case));
    if (args == NULL) {
        perror("unittest_add: malloc");
        return -1;
    }

    args->expected = expected;
    args->cmp = cmp;
    args->name = name;
    ut->tests[ut->size] = tc;
    ut->cases[ut->size++] = args;
    return 0;
}

/* unittest_run: Runs all the test cases and stores its results. */
int unittest_run(Unittest* ut) {
    int n_success = 0;
    int n_failure= 0;
    int n_error = 0;
    int n_unknown = 0;
    for (int i = 0; i < ut->size; i++) {
        printf("%s (%s) ...", ut->name, ut->cases[i]->name);
        fflush(stdout);

        int result = ut->tests[i](ut->cases[i]->cmp, ut->cases[i]->expected);

        switch (result) {
            case UNITTEST_SUCCESS:
                printf("ok\n");
                n_success++;
                break;
            case UNITTEST_FAILURE:
                printf("FAIL\n");
                n_failure++;
                break;
            case UNITTEST_ERROR:
                printf("ERROR\n");
                n_error++;
                break;
            default:
                printf("UNKNOWN\n");
                n_unknown++;
        }
    }

    // Print summary
    printf("\n----------------------------------------------------------------------\n");
    printf("Ran %d tests\n\n", ut->size);

    if (n_failure > 0 || n_error > 0 || n_unknown > 0) {
        printf("FAILED (");
        if (n_failure) printf("failures=%d", n_failure);
        if (n_failure && n_error) printf(", ");
        if (n_error) printf("errors=%d", n_error);
        if ((n_failure + n_error) && n_unknown) printf(", ");
        if (n_unknown) printf("unknown=%d", n_unknown);
        printf(")\n");
        return -1;
    } else {
        printf("OK\n");
        return 0;
    }
}

/* unittest_compare_int: Compares the intergers. */
int unittest_compare_int(const void* a, const void* b) {
    CHECK_NULL(a, b);
    const int* ia = (const int*)a;
    const int* ib = (const int*)b;
    if (*ia == *ib) return 0;
    return 1;
}

/* unittest_compare_string: Compares the strings. */
int unittest_compare_string(const void* a, const void* b) {
    CHECK_NULL(a, b);
    const char* sa = (const char*)a;
    const char* sb = (const char*)b;
    if (strcmp(sa, sb) == 0) return 0;
    return 1;
}


/* compare_task: Compares tasks. */
static int compare_task(const Task* a, const Task* b) {
    CHECK_NULL(a, b);
    if (a->status == b->status && strcmp(a->name, a->name) == 0)
        return 0;
    return 1;
}

/* unittest_compare_task: Compares the tasks. */
int unittest_compare_task(const void* a, const void* b) {
    CHECK_NULL(a, b);
    const Task* ta = (const Task*)a;
    const Task* tb = (const Task*)b;
    if (compare_task(ta, tb) == 0) return 0;
    return 1;
}

/* unittest_compare_json_value: Compares json values. */
int unittest_compare_json_value(const void* a, const void* b) {
    CHECK_NULL(a, b);
    const json_value* ja = (const json_value*)a;
    const json_value* jb = (const json_value*)b;
    if (json_value_compare(ja, jb) == 0) return 0;
    return 1;
}

#undef CHECK_NULL
