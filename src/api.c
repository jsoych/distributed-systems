#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "api.h"
#include "json-builder.h"
#include "json-helpers.h"

#define BACKLOG 5
#define BUFLEN 4096
#define CAPACITY 8
#define API_ERROR -1

// Globals
volatile sig_atomic_t sig_flag = 0;

// Client status codes
static enum {
    CLIENT_ACTIVE,
    CLIENT_INACTIVE
};

// Client structure
static struct api_client {
    int status;
    int client_fd;
    pthread_t tid;
    Api* server;
    struct api_client* next;
};

static const char* const API_MSG[] = {
    [API_SERVER_START]          = "API: starting server",
    [API_SERVER_STOP]           = "API: stopping server",
    [API_WORKING]               = "API: pyoneer is working",
    [API_NOT_WORKING]           = "API: pyoneer is not working"
};

static const char* const API_ERROR_MSG[] = {
    [API_ERR_INTERNAL]          = "API: internal error",
    [API_ERR_SHUTTING_DOWN]     = "API: shutting down",
    [API_ERR_BLUEPRINT]         = "API: blueprint - invalid JSON payload",
    [API_ERR_CMD]               = "API: command error",
    [API_ERR_SIGNAL]            = "API: signal error",
    [API_ERR_JSON_PARSE]        = "API: json_parse - invalid JSON payload",
    [API_ERR_JSON_TYPE]         = "API: json_value - invalid JSON type",
    [API_ERR_JSON_MISSING]      = "API: json_object - missing JSON value"
};

// Api server
typedef struct _api {
    Pyoneer* pyoneer;
    Logger* logger;
    char* socket_path;
    int server_fd;
    struct api_client* clients;
} Api;

/* api_client_thread: Handles the clients api requests. */
static void* api_client_thread(void* arg) {
    struct api_client* client = (struct client*)arg;
    Pyoneer* pyoneer = client->server->pyoneer;
    Logger* logger = client->server->logger;

    json_settings settings = {0};
    settings.value_extra = json_builder_extra;
    char error[json_error_max];

    int nbytes;
    char buf[BUFLEN];
    while ((nbytes = recv(client->client_fd, buf, BUFLEN, 0)) > 0) {
        buf[(nbytes >= BUFLEN) ? BUFLEN - 1 : nbytes] = '\0';
        json_value* resp = json_object_new(0);
        if (resp == NULL) {
            logger_debug(logger, API_ERROR_MSG[API_ERR_INTERNAL]);
            break;
        }

        json_value* req = json_parse_ex(&settings, buf, nbytes, error);
        if (req == NULL) {
            logger_info(logger, API_ERROR_MSG[API_ERR_JSON_PARSE]);
            logger_debug(logger, buf);
            json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_JSON_PARSE]);
            goto send;
        }

        // Check object type
        if (req->type != json_object) {
            logger_info(logger, API_ERROR_MSG[API_ERR_JSON_TYPE]);
            logger_debug(logger, buf);
            json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_JSON_TYPE]);
            goto send;
        }

        // Get command
        json_value* cmd = json_object_get_value(req, "command");
        if (cmd == NULL) {
            logger_info(logger, API_ERROR_MSG[API_ERR_JSON_MISSING]);
            logger_debug(logger, buf);
            json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_JSON_MISSING]);
            goto send;
        }

        // Call pyoneer commands and signals
        if (strcmp(cmd->u.string.ptr, "run") == 0) {
            json_value* val = json_get_object_value(req, "blueprint");
            if (val == NULL) {
                logger_info(logger, API_ERROR_MSG[API_ERR_JSON_MISSING]);
                logger_debug(logger, buf);
                json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_JSON_MISSING]);
                goto send;
            }

            int kind = pyoneer->get_blueprint_kind(pyoneer);
            if (kind == -1) {
                sig_flag = 1;   /* send resp and exit */
                logger_info(logger, API_ERROR_MSG[API_ERR_INTERNAL]);
                logger_debug(logger, API_ERROR_MSG[API_ERR_SHUTTING_DOWN]);
                json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_INTERNAL]);
                goto send;
            }

            Blueprint* blueprint = blueprint_decode(val, kind);
            if (blueprint == NULL) {
                logger_info(logger, API_ERROR_MSG[API_ERR_BLUEPRINT]);
                logger_debug(logger, buf);
                json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_BLUEPRINT]);
                goto send;
            }

            if (pyoneer->run(pyoneer, blueprint) == -1) {
                blueprint_destroy(blueprint);
                json_object_push_string(resp, "Error", API_MSG[API_WORKING]);
                goto send;
            }
            
            json_value* status = pyoneer_status_encode(pyoneer->get_status(pyoneer));
            json_object_push(resp, "status", status);

            status = blueprint_status_encode(pyoneer->get_blueprint_status(pyoneer));
            json_object_push(resp, "blueprint_status", status);
        }
        // get_status
        else if (strcmp(cmd->u.string.ptr, "get_status") == 0) {
            json_value* status = pyoneer_status_encode(pyoneer->get_status(pyoneer));
            json_object_push(resp, "status", status);
        } 
        // get_blueprint_status
        else if (strcmp(cmd->u.string.ptr, "get_blueprint_status") == 0) {
            json_value* status = blueprint_status_encode(pyoneer->get_blueprint_status(pyoneer));
            json_object_push(resp, "blueprint_status", status);
        }
        // assign
        else if (strcmp(cmd->u.string.ptr, "assign") == 0) {
            json_value* val = json_get_object_value(req, "blueprint");
            if (val == NULL) {
                logger_info(logger, API_ERROR_MSG[API_ERR_JSON_MISSING]);
                logger_debug(logger, buf);
                goto send;
            }

            int kind = pyoneer->get_blueprint_kind(pyoneer);
            if (kind == -1) {
                sig_flag = -1; // Exit loop
                logger_info(logger, API_ERROR_MSG[API_ERR_INTERNAL]);
                logger_debug(logger, API_ERROR_MSG[API_ERR_SHUTTING_DOWN]);
                json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_INTERNAL]);
                goto send;
            }

            Blueprint* blueprint = blueprint_decode(val, kind);
            if (blueprint == NULL) {
                logger_info(logger, API_ERROR_MSG[API_ERR_BLUEPRINT]);
                logger_debug(logger, buf);
                json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_BLUEPRINT]);
                goto send;
            }

            if (pyoneer->assign(pyoneer, blueprint) == -1) {
                blueprint_destroy(blueprint);
                logger_info(logger, API_MSG[API_WORKING]);
                json_object_push_string(resp, "Error", API_MSG[API_WORKING]);
                goto send;
            }
        }
        // unassign
        else if (strcmp(cmd->u.string.ptr, "unassign") == 0) {
            Blueprint* blueprint = NULL;
            if (pyoneer->unassign(pyoneer, blueprint) == -1) {
                logger_info(logger, API_ERROR_MSG[API_ERR_INTERNAL]);
                // TODO: Add debug info
                goto send;
            }
            blueprint_destroy(blueprint);
        }
        // start
        else if (strcmp(cmd->u.string.ptr, "start") == 0) {
            json_value* status = blueprint_status_encode(
                pyoneer->get_blueprint_status(pyoneer));
            json_object_push(resp, "blueprint_status", status);
        }
        // Stop
        else if (strcmp(cmd->u.string.ptr, "stop") == 0) {
            json_value* status = blueprint_status_encode(
                pyoneer->get_blueprint_status(pyoneer)
            );
            json_object_push(resp, "blueprint_status", status);
        }
        // Unknown command
        else {
            logger_debug(logger, buf);
            json_object_push_string(resp, "Error", API_ERROR_MSG[API_ERR_CMD]);
        }

        send:
        if (json_measure(resp) > BUFLEN) {
            sig_flag = -1; // Exit loop
            logger_info(logger, API_ERROR_MSG[API_ERR_INTERNAL]);
            logger_info(logger, API_ERROR_MSG[API_ERR_SHUTTING_DOWN]);
            logger_debug(logger, buf);
            strcpy(buf, "{\"error\":\"api failure\"}");
        } else {
            json_serialize(buf, resp);
            logger_debug(logger, buf);
        }

        if (send(client->client_fd, buf, strlen(buf), 0) == -1) {
            sig_flag = -1; // Exit loop
            perror("api: send");
        }

        json_builder_free(resp);
    }

    // close socket
    close(client->client_fd);
    return NULL;
}

/* api_signal_handler: Handles the signal to the Api server. */
static void api_signal_handler(int signo) {
    sig_flag = 1;
}

/* api_create: Creates a new server and returns it. */
Api* api_create(Pyoneer* pyoneer, Logger* logger, const char* socket_path) {
    Api* server = malloc(sizeof(Api));
    if (server == NULL) {
        perror("api_create: malloc");
        return NULL;
    }
    
    server->pyoneer = pyoneer;
    server->logger = logger;

    int len = strlen(socket_path);
    server->socket_path = malloc((len + 1)*sizeof(char));
    if (server->socket_path == NULL) {
        perror("api_create: malloc");
        free(server);
        return NULL;
    }
    strncpy(server->socket_path, socket_path, len);
    server->socket_path[len] = '\0';

    server->server_fd = -1;
    server->clients = NULL;

    struct sigaction sa = {0};
    sa.sa_handler = api_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("api_create: sigaction");
        free(server->socket_path);
        free(server);
        return NULL;
    }

    return server;
}

/* api_destroy: Stops the server and frees its resources. */
void api_destroy(Api* server) {
    if (server == NULL) return;

    api_stop(server);
    free(server->socket_path);
    free(server);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;

    if (sigaction(SIGINT, &sa, NULL) == -1)
        perror("api_destroy: sigaction");
}

/* api_add_client: Adds a client to the server and returns a pointer to the
    newly created client */
static struct api_client* api_add_client(Api* server, int client_fd) {
    struct api_client* client = malloc(sizeof(struct api_client));
    if (client == NULL) {
        perror("api_add_client: malloc");
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

/* api_start: Creates a socket and listens for client connections. If the
    process encounters an interupt, the server stops listening for new
    client connections and returns 0. Otherwise, returns -1. */
int api_start(Api* server) {
    if (server == NULL) return -1;

    // Create local socket
    server->server_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (server->server_fd == -1) {
        perror("api_start: socket");
        return -1;
    }

    if (unlink(server->socket_path) == -1) {
        perror("api_server_run: unlink");
        close(server->server_fd);
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, server->socket_path, sizeof(addr.sun_path));

    if (bind(server->server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("api_server_run: bind");
        close(server->server_fd);
        return -1;
    }

    if (listen(server->server_fd, BACKLOG) == -1) {
        perror("api_server_run: listen");
        close(server->server_fd);
        return -1;
    }

    logger_info(server->logger, API_MSG[API_SERVER_START]);
    logger_debug(server->logger, server->socket_path);

    while (sig_flag != 1) {
        int client_fd = accept(server->server_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("api_server_run: accept");
            break;
        }

        struct api_client* client = api_server_add_client(server, client_fd);
        if (client == NULL) {
            close(client_fd);
            continue;
        }

        int err = pthread_create(&client->tid, NULL, api_client_thread, client);
        if (err != 0) {
            fprintf(stderr, "api_server_run: pthread_create: %s\n", strerror(err));
            break;
        }
    }

    return 0;
}

/* api_stop: Stops the server and frees all resources, and returns 0. */
int api_stop(Api* server) {
    if (server == NULL) return 0;
    logger_info(server->logger, API_MSG[API_SERVER_STOP]);

    struct api_client* curr = server->clients;
    while (curr) {
        struct api_client* next = curr->next;
        if (curr->status == CLIENT_ACTIVE) {
            int err = pthread_cancel(curr->tid);
            if (err != 0)
                fprintf(stderr, "api_stop: pthread_cancel: %s\n", strerror(err));
        }
        free(curr);
        curr = next;
    }
    server->clients = NULL;
    close(server->server_fd);
    server->server_fd = -1;
    return 0;
}
