#ifndef _API_H
#define _API_H

#include "pyoneer.h"
#include "blueprint.h"
#include "logger.h"

typedef enum {
    API_SERVER_START,
    API_SERVER_STOP,
    API_RUN,
    API_GET_STATUS,
    API_GET_BLUEPRINT_STATUS,
    API_ASSIGN,
    API_UNASSIGN,
    API_START,
    API_STOP,
    API_WORKING,
    API_NOT_WORKING
} ApiCode;

typedef enum {
    API_ERR_INTERNAL = 0,
    API_ERR_SHUTTING_DOWN,
    API_ERR_ASSIGN,
    API_ERR_UNASSIGN,
    API_ERR_BLUEPRINT,
    API_ERR_CMD,
    API_ERR_SIGNAL,
    API_ERR_JSON_PARSE,
    API_ERR_JSON_TYPE,
    API_ERR_JSON_MISSING
} ApiErrorCode;

typedef struct _api Api;

Api* api_create(Pyoneer* pyoneer, Logger* logger, const char* path);
void api_destroy(Api* server);

int api_start(Api* server);
int api_stop(Api* server);

#endif
