#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "manager.h"
#include "project.h"
#include "json.h"
#include "json-builder.h"

#define BACKLOG 10
#define BUFLEN 1024

// Logging levels
enum {
    info,
    debug
};

// Globals
int logging_level;
Manager *manager;

json_value *json_get_value(json_value *, char *);
json_value *manager_status_map(int);
json_value *project_status_map(int);
void *manager_thread(void *arg);

int main(int argc, char *argv[]) {
    // Check arguments
    if (argc != 3) {
        fprintf(stderr, "main: expected 2 arguments, received %d arguments\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    // Set logging level
    if (strcmp(argv[2], "debug") == 0)
        logging_level = debug;
    else
        logging_level = info;

    // Create manager
    manager = create_manager((int) strtol(argv[1], NULL, 0));
    
    // Create endpoint
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_LOCAL;
#ifdef __APPLE__
    snprintf(
        addr.sun_path,
        sizeof(addr.sun_path),
        "/Users/leejahsprock/pyoneer/run/manager%d.socket",
        manager->id
    );
#elif __linux__
    snprintf(
        addr.sun_path,
        sizeof(addr.sun_path),
        "/run/pyoneer/manager%d.socket",
        manager->id
    );
#endif
    
    int serv;
    if ((serv = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("main: socket");
        exit(EXIT_FAILURE);
    }

    if (unlink(addr.sun_path) == -1) {
        perror("main: unlink");
        exit(EXIT_FAILURE);
    }

    if (bind(serv, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("main: bind");
        exit(EXIT_FAILURE);
    }

    if (listen(serv, BACKLOG) == -1) {
        perror("main: listen");
        exit(EXIT_FAILURE);
    }

    int old_errno, clients[BACKLOG];
    pthread_t manager_tids[BACKLOG];
    socklen_t addr_len;
    struct sockaddr_storage client_addr;
    for (int idx = 0; 1; idx = (idx + 1) % BACKLOG) {
        // Accept client connections
        if ((clients[idx] = accept(serv, (struct sockaddr *) &client_addr, &addr_len)) == -1) {
            perror("main: accept");
            exit(EXIT_FAILURE);
        }

        // Create manager thread
        if ((old_errno = pthread_create(&manager_tids[idx], NULL, manager_thread, &clients[idx])) != 0) {
            fprintf(stderr, "main: pthread_create: %s\n", strerror(old_errno));
            exit(EXIT_FAILURE);
        }
    }

}

/* manager_status_map: Maps manager status codes to its corresponding
    JSON value. */
json_value *manager_status_map(int status) {
    json_value *val;
    switch (status) {
        case _MANAGER_NOT_WORKING:
            val = json_string_new("not_working");
            break;
        case _MANAGER_WORKING:
            val = json_string_new("working");
            break;
    }
    return val;
}

/* project_status_map: Maps project status codes to its corresponding
    JSON value. */
json_value *project_status_map(int status) {
    json_value *val;
    switch (status) {
        case _PROJECT_READY:
            val = json_string_new("ready");
            break;
        case _PROJECT_NOT_READY:
            val = json_string_new("not_ready");
            break;
        case _PROJECT_RUNNING:
            val = json_string_new("running");
            break;
        case _PROJECT_COMPLETED:
            val = json_string_new("completed");
            break;
        case _PROJECT_INCOMPLETE:
            val = json_string_new("incomplete");
            break;
        default:
            val = json_null_new();
            break;
    }
    return val;
}

/* json_get_value */
json_value *json_get_value(json_value *obj, char *name) {
    json_value *val = NULL;
    for (int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, name) == 0) {
            val = obj->u.object.values[i].value;
            break;
        }
    }
    return val;
}

/* manager_thread: Responds to API requests (commands). */
void *manager_thread(void *arg) {
    int client = *((int *) arg);
    Project *proj;
    int nbytes;
    json_value *obj, *res, *val, *cmd;
    json_settings settings;
    settings.value_extra = json_builder_extra;
    char buf[BUFLEN], error[128];
    while ((nbytes = recv(client, buf, BUFLEN, 0)) > 0) {
        buf[(nbytes == BUFLEN ? BUFLEN - 1 : nbytes)] = '\0';

        if ((res = json_object_new(0)) == NULL) {
            fprintf(stderr, "main: json_object_new: Error: Unable to create new JSON object\n");
            close(client);
            break;
        }

        if ((obj = json_parse_ex(&settings, buf, strlen(buf), error)) == NULL) {
            fprintf(stderr, "main: json_parse_ex: %s\n", error);
            obj = json_null_new();
            val = json_string_new("unable to parse API request");
            json_object_push(res, "error", val);
            goto send;
        }

        if (obj->type != json_object) {
            val = json_string_new("invalid JSON type");
            json_object_push(res, "error", val);
            goto send;
        }

        if ((cmd = json_get_value(obj, "command")) == NULL) {
            val = json_string_new("missing command");
            json_object_push(res, "error", val);
            goto send;
        }

        // Call manager command
        // run_project
        if (strcmp(cmd->u.string.ptr, "run_project") == 0) {
            if ((val = json_get_value(obj, "project")) == NULL) {
                val = json_string_new("missing project");
                json_object_push(res, "error", val);
                goto send;
            }

            if ((proj = decode_project(val)) == NULL) {
                val = json_string_new("unable to decode project");
                json_object_push(res, "error", val);
                goto send;
            }

            if (run_project(manager, proj) == -1) {
                val = json_string_new("unable to run project");
                json_object_push(res, "error", val);
                goto send;
            }

            val = manager_status_map(get_status(manager));
            json_object_push(res, "status", val);
            val = project_status_map(get_project_status(manager));
            json_object_push(res, "project_status", val);
        }
        // get_status
        else if (strcmp(cmd->u.string.ptr, "get_status") == 0) {
            val = manager_status_map(get_status(manager));
            json_object_push(res, "status", val);
        }
        // get_project_status
        else if (strcmp(cmd->u.string.ptr, "get_project_status") == 0) {
            val = project_status_map(get_project_status(manager));
            json_object_push(res, "project_status", val);
        }
        // start
        else if (strcmp(cmd->u.string.ptr, "start") == 0) {
            val = project_status_map(start(manager));
            json_object_push(res, "project_status", val);
        }
        // stop
        else if (strcmp(cmd->u.string.ptr, "stop") == 0) {
            val = project_status_map(stop(manager));
            json_object_push(res, "project_status", val);
        }
        // unknown command
        else {
            val = json_string_new("unknown command");
            json_object_push(res, "error", val);
        }

        send:
        if (logging_level == debug)
            json_object_push(res, "debug", obj);
        
        if (json_measure(res) > BUFLEN) {
            fprintf(stderr, "main: Error: buffer length too small\n");
            strcpy(buf, "{\"error\":\"system failure\"}");
        } else {
            json_serialize(buf, res);
        }

        if ((nbytes = send(client, buf, strlen(buf), 0)) == -1) {
            perror("main: send");
            close(client);
            break;
        }

        if (logging_level == debug) {
            json_builder_free(res);
        } else {
            json_builder_free(obj);
            json_builder_free(res);
        }
    }
    if (nbytes == -1) {
        // To do. We need to return the errno to the calling thread
        return NULL;
    } else {
        // To. We need to return 0 to the calling thread
        return NULL;
    }
}
