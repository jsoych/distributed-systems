#ifndef _BLUEPRINT_H
#define _BLUEPRINT_H

#include "job.h"
#include "project.h"

enum {
    BLUEPRINT_JOB,
    BLUEPRINT_PROJECT
};

// Blueprint
typedef struct _blueprint {
    int kind;
    union {
        Job* job;
        Project* project;
    } as;
} Blueprint;

void blueprint_destroy(Blueprint* blueprint);

// Helpers
Blueprint* blueprint_decode(const json_value* obj, int kind);
json_value* blueprint_status_encode(Blueprint* blueprint);

#endif
