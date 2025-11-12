#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "api.h"
#include "common.h"

#define BACKLOG 5
#define BUFFLEN 4096

/* worker_thread: */
void* worker_thread(void* arg) {
    Client *client = (Client *) arg;
    Job *job;
    int nbytes;
    char buf[BUFLEN], error[128];
    json_settings settings;
    settings.value_extra = json_builder_extra;
    json_value *cmd, *obj, *res, *val;
    while ((nbytes = recv(client->sockfd, buf, BUFLEN, BUFLEN)) > 0) {
        buf[(nbytes >= BUFLEN) ? BUFLEN - 1 : nbytes] = '\0';
        if ((res = json_object_new(0)) == NULL) {
            fprintf(stderr, "main: json_object_new: Error: Unable to create new JSON object\n");
            break;
        }

        if (logging_level == logging_debug)
            printf("api request: %s\n", buf);

        if ((obj = json_parse_ex(&settings, buf, BUFLEN, error)) == NULL) {
            fprintf(stderr, "main: json_parse_ex: error: %s\n", error);
            obj = json_null_new();
            goto send;
        }

        // Check object type
        if (obj->type != json_object) {
            val = json_string_new("invalid api request");
            json_object_push(res, "error", val);
            goto send;
        }

        // Get command
        if ((cmd = json_get_value(obj, "command")) == NULL) {
            val = json_string_new("missing command");
            json_object_push(res, "error", val);
            goto send;
        }

        // Call worker command
        // run
        if (strcmp(cmd->u.string.ptr, "run") == 0) {
            if ((val = json_get_value(obj, "job")) == NULL) {
                val = json_string_new("missing job");
                json_object_push(res, "error", val);
                goto send;
            }

            if ((job = decode_job(val)) == NULL) {
                val = json_string_new("unable to decode job");
                json_object_push(res, "error", val);
                goto send;
            }

            if (run(worker, job) == -1) {
                val = json_string_new("unable to run job");
                json_object_push(res, "error", val);
                goto send;
            }
            val = worker_status_map(get_status((worker)));
            json_object_push(res, "status", val);

            val = job_status_map(get_job_status(worker));
            json_object_push(res, "job_status", val);
        }
        // get_status
        else if (strcmp(cmd->u.string.ptr, "get_status") == 0) {
            val = worker_status_map(get_status(worker));
            json_object_push(res, "status", val);
        } 
        // get_job_status
        else if (strcmp(cmd->u.string.ptr, "get_job_status") == 0) {
            val = job_status_map(get_job_status(worker));
            json_object_push(res, "job_status", val);
        } 
        // start
        else if (strcmp(cmd->u.string.ptr, "start") == 0) {
            val = job_status_map(start(worker));
            json_object_push(res, "job_status", val);
        } 
        // start
        else if (strcmp(cmd->u.string.ptr, "stop") == 0) {
            val = job_status_map(stop(worker));
            json_object_push(res, "job_status", val);
        } 
        // Unknown command
        else {
            val = json_string_new("unknown command");
            json_object_push(res, "error", val);
        }

        if (logging_level == logging_debug)
            json_object_push(res, "debug", obj);
        else
            json_value_free(obj);

        send:
        if (json_measure(res) > BUFLEN) {
            fprintf(stderr, "main: json_measure: buffer length too small\n");
            strcpy(buf, "{\"error\":\"api failure\"}");
        } else {
            json_serialize(buf, res);
        }

        if (send(client->sockfd, buf, strlen(buf), 0) == -1) {
            perror("main: send");
            json_builder_free(res);
            exit(EXIT_FAILURE);
        }

        if (logging_level == logging_debug)
            printf("api response: %s\n", buf);

        json_builder_free(res);
    }
    
    // update client status and close sock descriptor
    client->status = client_inactive;
    close(client->sockfd);
    return NULL;
}


int api_server_init(api_server_t* server, Worker* worker,
        const char* socket_path, api_log_level_t log_level) {
    if (!server || !worker || !socket_path) return -1;
    
    server->worker;

    int len = strlen(socket_path);
    server->socket_path = malloc((len + 1)*sizeof(char));
    if (server->socket_path == NULL) {
        // TODO: Write error message module
        perror("api_server_init: malloc");
        return -1;
    }
    strncpy(server->socket_path, socket_path, len);

    server->log_level = log_level;
    server->stop_flag = 0;
    server->server_fd = -1;

    return;
}

// Start the API server loop (blocking call)
int api_server_run(api_server_t* server) {
    if (!server) return -1;

    server->server_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (server->server_fd == -1) {
        perror("api_server_run: socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, server->socket_path, sizeof(addr.sun_path));

    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("api_server_run: bind");
        close(server->server_fd);
        return -1;
    }

    if (unlink(server->socket_path) == -1) {
        perror("api_server_run: unlink");
        close(server->server_fd);
        return -1;
    }

    if (listen(server->server_fd, BACKLOG) == -1) {
        perror("api_server_run: listen");
        close(server->server_fd);
        return -1;
    }

    if (server->log_level == API_LOG_DEBUG)
        printf("API server running on %s\n", server->socket_path);

    while (!server->stop_flag) {
        int client_fd = accept(server->server_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("api_server_run: accept");
            break;
        }

        pthread_t tid;
        int err = pthread_create(&tid, NULL, worker_thread, (void*)(intptr_t)client_fd);
        
    }
}

// Stop the API server gracefully
int api_server_stop(api_server_t* server);

// Marshal status to JSON
json_value* worker_status_to_json(Worker* worker);
json_value* job_status_to_json(Job* job);