#ifndef _SUITE_H
#define _SUITE_H

#include "unittest.h"

#define UNITTEST_RUN_ERROR 255

typedef struct _suite {
    int size;
    int capacity;
    Unittest** tests;
} Suite;

Suite* suite_create(int size);
void suite_destroy(Suite* suite);

int suite_add(Suite* suite, Unittest* ut);
int suite_run(Suite* suite);

#endif
