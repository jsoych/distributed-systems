#ifndef pyoneer_api_h
#define pyoneer_api_h

#include <signal.h>
#include "worker.h"

// Logging levels
typedef enum {
    API_LOG_INFO,
    API_LOG_DEBUG
} api_log_level_t;

// API server configuration
typedef struct {
    Worker* worker;
    const char* socket_path;
    api_log_level_t log_level;
    int server_fd;
    volatile sig_atomic_t stop_flag;
} api_server_t;

// Initialize the API server
// Returns 0 on success, -1 on failure
int api_server_init(api_server_t* server, Worker* worker,
    const char* socket_path, api_log_level_t log_level);

// Start the API server loop (blocking call)
int api_server_run(api_server_t* server);

// Stop the API server gracefully
int api_server_stop(api_server_t* server);

// Marshal status to JSON
json_value* worker_status_to_json(Worker* worker);
json_value* job_status_to_json(Job* job);

#endif
