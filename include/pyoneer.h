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

typedef json_value* (*command)(Pyoneer*);
typedef json_value* (*command_blueprint)(Pyoneer*, Blueprint*);
typedef void (*signal)(Pyoneer*);

typedef struct _pyoneer {
    int role;
    union {
        Worker* worker;
        Manager* manager;
    } as;
    command get_status;
    command_blueprint run;
    command_blueprint assign;
    signal start;
    signal stop;
} Pyoneer;

Pyoneer* pyoneer_create(int id, int type, Site* site);
void pyoneer_destroy(Pyoneer* pyoneer);

json_value* pyoneer_status_encode(Pyoneer* pyoneer);
int pyoneer_status_decode(Pyoneer* pyoneer, json_value* obj);
Blueprint* pyoneer_blueprint_decode(Pyoneer* pyoneer, const json_value* val);

#endif