#ifndef _PYONEER
#define _PYONEER
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include "manager.h"
#include "worker.h"

typedef enum {
    pyoneer_manager,
    pyoneer_supervisor,
    pyoneer_worker
} pyoneer_kind;

typedef enum {
    pyoneer_local,
    pyoneer_tcp
} pyoneer_protocol;

typedef struct _metadata {
    int id;
    char *name; /* null terminated */
} pyoneer_metadata;

typedef struct _endpoint {
    pyoneer_protocol protocol;
    union {
        struct sockaddr_un local;
        struct in_addr tcp;
    } u;
} pyoneer_endpoint;

typedef enum {
    pyoneer_project,
    pyoneer_job
} pyoneer_blueprint;

typedef struct _worker {
    int id;
} pyoneer_worker;

typedef struct _supervisior {
    int id;
    char *ip; /* null terminated */
    char *port; /* null terminated */
} pyoneer_supervisor;

typedef struct _crew {
    pyoneer_kind kind;
    union {
        pyoneer_worker worker;
        pyoneer_supervisor supervisor;
    }u;
    struct _crew *next;
} pyoneer_crew;

typedef struct _spec {
    pyoneer_blueprint blueprint;
    pyoneer_crew crew;
} pyoneer_spec;

typedef struct _manifest {
    pyoneer_kind kind;
    pyoneer_metadata metadata;
    pyoneer_endpoint endpoint;
    pyoneer_spec spec;
} pyoneer_manifest;

typedef struct _blueprint {
    /* blueprint template */
} Blueprint;

typedef struct _pyoneer {
    pyoneer_kind kind;
    union {
        Manager manager;
        Worker worker;
    } u;
    /* put blueprints here */
} Pyoneer;

Pyoneer *create_pyoneer(pyoneer_manifest *);
void free_pyoneer(Pyoneer *);

int run(Pyoneer *, Blueprint *);
int get_status(Pyoneer *);
int get_blueprint_status(Pyoneer *, int);
int start(Pyoneer *);
int stop(Pyoneer *);

#endif