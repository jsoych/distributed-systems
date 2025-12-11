#ifndef _PROJECT_H
#define _PROJECT_H

#define TABLESIZE 512

#include "job.h"
#include "json.h"
#include "json-builder.h"

enum {
    PROJECT_NOT_READY,
    PROJECT_READY,
    PROJECT_RUNNING,
    PROJECT_COMPLETED,
    PROJECT_INCOMPLETE
};

// project node
typedef struct _project_node {
    Job* job;
    int* deps;
    int len;
    struct _project_node* next;
    struct _project_node* prev;
    struct _project_node* next_ent;
} project_node;

// jobs list
typedef struct _project_list {
    project_node* head;
    project_node* tail;
} project_list;

// Project object
typedef struct _project {
    int id;
    int status;
    int len;
    project_list jobs_list;
    project_node* jobs_table[TABLESIZE];
} Project;

// Construtor and destructor
Project *project_create(int id);
void project_destroy(Project* project);

// Methods
void project_add_job(Project* project, Job* job, int* deps, int size);
void project_remove_job(Project* project, int id);

// Helpers
json_value* project_encode(Project* project);
Project* project_decode(json_value* obj);

#endif
