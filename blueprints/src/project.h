#ifndef _PROJECT_H
#define _PROJECT_H
#define _PROJECT_INCOMPLETE -2
#define _PROJECT_NOT_READY -1
#define _PROJECT_READY 0
#define _PROJECT_RUNNING 1
#define _PROJECT_COMPLETED 2
#define MAXLEN 512

#include "cJSON.h"
#include "job.h"

// List structure
typedef struct _list {
    ProjectNode *head;
    ProjectNode *tail;
} List;

// ProjectNode structure
typedef struct _project_node {
    Job *job;
    int *deps;
    int len;
    struct _project_node *next;
    struct _project_node *prev;
    struct _project_node *next_ent;
} ProjectNode;

// Project Object
typedef struct _project {
    int id;
    int status;
    int len;
    List *jobs_list;
    ProjectNode *jobs_table[MAXLEN];
} Project;

Project *create_project(int);
void free_project(Project *);
void add_job(Project *, Job *, int [], int);
void remove_job(Project *, int);
int audit_project(Project *);
cJSON *encode_project(Project *);
Project *decode_project(cJSON *);

#endif