#ifndef _PYONEER_H
#define _PYONEER_H

#include "worker.h"
#include "manager.h"

typedef enum {
    TYPE_WORKER,
    TYPE_MANAGER
} pyoneer_t;

typedef struct _pyoneer Pyoneer;

Pyoneer* pyoneer_create(int id, pyoneer_t type);
void pyoneer_destroy(Pyoneer* pyoneer);

#endif