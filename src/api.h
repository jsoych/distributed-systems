#ifndef pyoneer_api_h
#define pyoneer_api_h

#include <signal.h>
#include "logger.h"
#include "worker.h"

typedef struct api_server ApiServer;

ApiServer* api_server_create(Worker* worker, Logger* logger,
    const char* socket_path);
void api_server_destroy(ApiServer* server);


// Start the API server loop (blocking call)
int api_server_start(ApiServer* server);

// Stop the API server gracefully
int api_server_stop(ApiServer* server);

// Marshal status to JSON
json_value* worker_status_to_json(Worker* worker);
json_value* job_status_to_json(Job* job);

#endif
