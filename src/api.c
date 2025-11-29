#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "api.h"
#include "common.h"
#include "sys.h"

#define BACKLOG 5
#define BUFLEN 4096
#define CAPACITY 8

static enum {
    CLIENT_ACTIVE,
    CLIENT_INACTIVE
};

static struct api_client {
    int status;
    int client_fd;
    pthread_t tid;
    ApiServer* server;
    struct api_client* next;
};

// API server configuration
typedef struct _api_server {
    Worker* worker;
    Logger* logger;
    char* socket_path;
    int server_fd;
    volatile sig_atomic_t stop_flag;
    struct api_client* clients;
} ApiServer;

/* worker_thread: */
void* worker_thread(void* arg) {
    struct api_client* client = (struct client*)arg;
    Worker* worker = client->server->worker;
    Logger* logger = client->server->logger;

    json_settings settings;
    settings.value_extra = json_builder_extra;
    json_value *cmd, *obj, *res, *val;

    int nbytes;
    char buf[BUFLEN], error[128];
    while ((nbytes = recv(client->client_fd, buf, BUFLEN, 0)) > 0) {
        buf[(nbytes >= BUFLEN) ? BUFLEN - 1 : nbytes] = '\0';
        if ((res = json_object_new(0)) == NULL) {
            fprintf(stderr, "main: json_object_new: Error: Unable to create new JSON object\n");
            break;
        }

        debug(logger, "api request: %s\n", buf);

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

            Job* job = decode_job(val);
            if (job == NULL) {
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

        json_object_push(res, "debug", obj);

        send:
        if (json_measure(res) > BUFLEN) {
            fprintf(stderr, "main: json_measure: buffer length too small\n");
            strcpy(buf, "{\"error\":\"api failure\"}");
        } else {
            json_serialize(buf, res);
        }

        if (send(client->client_fd, buf, strlen(buf), 0) == -1) {
            perror("main: send");
            json_builder_free(res);
            exit(EXIT_FAILURE);
        }

        debug(logger, "api response: %s\n", buf);

        json_builder_free(res);
    }
    
    // close socket
    close(client->client_fd);
    return NULL;
}

ApiServer* api_server_create(Worker* worker, Logger* logger,
        const char* socket_path) {
    ApiServer* server = XMALLOC(sizeof(ApiServer));
    
    server->logger = logger;
    server->worker = worker;

    int len = strlen(socket_path);
    server->socket_path = malloc((len + 1)*sizeof(char));
    if (server->socket_path == NULL) {
        perror("api_server_init: malloc");
        return -1;
    }
    strncpy(server->socket_path, socket_path, len);
    server->socket_path[len] = '\0';

    server->stop_flag = 0;
    server->server_fd = -1;
    server->clients = NULL;

    return 0;
}

// Add client
static struct api_client* api_server_add_client(ApiServer* server, int client_fd) {
    struct api_client* client = malloc(sizeof(struct api_client));
    if (client == NULL) {
        perror("api_server_add_client: malloc");
        return NULL;
    }

    client->client_fd = client_fd;
    client->status = CLIENT_ACTIVE;
    client->server = server;
    client->next = NULL;

    struct api_client* curr = server->clients;
    struct api_client* prev = NULL;
    while (curr) {
        if (curr->status == CLIENT_INACTIVE) {
            struct api_client* tmp = curr->next;
            if (prev){
                prev->next = tmp;
            } else {
                server->clients = tmp;
            }
            free(curr);
            curr = tmp;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    if (prev) {
        prev->next = client;
    } else {
        server->clients = client;
    }

    return client;
}

// Start the API server loop (blocking call)
int api_server_start(ApiServer* server) {
    if (!server) return -1;

    server->server_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    XSYSCALL(server->server_fd);

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

    debug(server->logger, "API server running on %s\n", server->socket_path);

    while (!server->stop_flag) {
        int client_fd = accept(server->server_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("api_server_run: accept");
            break;
        }

        struct api_client* client = api_server_add_client(server, client_fd);
        if (client == NULL) {
            close(client_fd);
        }

        int err = pthread_create(&client->tid, NULL, worker_thread, client);
        if (err != 0) {
            fprintf(stderr, "api_server_run: pthread_create: %s\n", strerror(err));
            break;
        }
    }

    return 0;
}

// Stop the API server gracefully
int api_server_stop(ApiServer* server) {
    struct api_client* curr = server->clients;
    while (curr) {
        if (curr->status == CLIENT_ACTIVE) {
            int err = pthread_cancel(curr->tid);
            if (err != 0)
                fprintf(stderr, "api_server_stop: pthread_cancel: %s\n", strerror(err));
        }
        struct api_client* prev = curr;
        curr = curr->next;
        free(prev);
    }
    server->clients = NULL;
    
    close(server->server_fd);

    return 0;
}

// Marshal status to JSON
json_value* worker_status_to_json(Worker* worker);
json_value* job_status_to_json(Job* job);