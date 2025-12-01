#ifndef _PYONEER_H
#define _PYONEER_H

#include "json.h"
#include "worker.h"
#include "manager.h"
#include "blueprint.h"

#define PYONEER_MAX_ID 4096

enum {
    PYONEER_WORKER,
    PYONEER_MANAGER
};

typedef int (*command)(Pyoneer*);
typedef int (*command_blueprint)(Pyoneer*, Blueprint*);
typedef int (*signal)(Pyoneer*);

typedef struct _pyoneer {
    int role;
    union {
        Worker* worker;
        Manager* manager;
    } as;
    command get_status;
    command get_blueprint_kind;
    command get_blueprint_status;
    command_blueprint run;
    command_blueprint assign;
    command_blueprint unassign;
    signal start;
    signal stop;
} Pyoneer;

Pyoneer* pyoneer_create(int id, int type);
void pyoneer_destroy(Pyoneer* pyoneer);

json_value* pyoneer_status_encode(Pyoneer* pyoneer);
int pyoneer_status_decode(Pyoneer* pyoneer, json_value* obj);

#endif