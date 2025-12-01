#ifndef _CREW_H
#define _CREW_H

#include <pthread.h>
#include "json.h"
#include "json-builder.h"
#include "worker.h"
#include "job.h"

#define CREW_MAXLEN 512

// job structure
typedef struct _crew_job {
    int id;
    int status;
} crew_job;

// worker structure
typedef struct {
    int id;
    int status;
    crew_job job;
} crew_worker;

// crew node
typedef struct _crew_node {
    crew_worker* worker;
    struct _crew_node* next;
    struct _crew_node* prev;
    struct _crew_node* next_free;
    struct _crew_node* prev_free;
    pthread_t tid;
} crew_node;

// crew list
typedef struct _crew_list {
    crew_node* head;
    crew_node* tail;
    int len;
} crew_list;

// Crew object
typedef struct _crew {
    crew_list workers[CREW_MAXLEN];
    crew_list freelist;
    int len;
    pthread_mutex_t lock;
    pthread_t tid;
} Crew;

// Constructor and destructor
Crew *crew_create();
void crew_destroy(Crew* crew);

// Methods
int crew_add(Crew* crew, int id);
int crew_remove(Crew* crew, int id);
int crew_get_status(Crew* crew, int id);
int crew_get_job_status(Crew* crew, int id);
int crew_assign_job(Crew* crew, Job* job);
int crew_unassign(Crew* crew, int id);

void crew_send_command(Crew* crew, int id, const char* command);
void crew_broadcast(Crew* crew, const char* command);

#endif