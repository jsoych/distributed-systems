#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
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
volatile sig_atomic_t int_flag;

json_value *manager_status_map(int);
json_value *project_status_map(int);
void *manager_thread(void *);
void signal_handler(int);

struct client_node {
    int sd;
    pthread_t tid;
    struct client_node *next;
    struct client_node *prev;
};

struct client_list {
    struct client_node *head;
    int len;
    pthread_mutex_t lock;
};

void init_client_list(struct client_list *);
void free_client_list(struct client_list *);
struct client_node *add_client(struct client_list *, int);
void remove_node(struct client_list *, struct client_node *);

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
        "/home/jsoychak/pyoneer/run/manager%d.socket",
        manager->id
    );
#endif
    
    int serv;
    if ((serv = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("main: socket");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("main: sigaction");
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

    int err, sd;
    struct client_list clients;
    struct client_node *node;
    init_client_list(&clients);
    int_flag = 0;
    while (int_flag != 1) {
        // Accept client connections
        if ((sd = accept(serv, NULL, NULL)) == -1) {
            perror("main: accept");
            if (errno == EINTR)
                continue;
            break;
        }
        node = add_client(&clients, sd);

        // Create manager thread
        if ((err = pthread_create(&node->tid, NULL, manager_thread, &node->sd)) != 0) {
            fprintf(stderr, "main: pthread_create: %s\n", strerror(err));
            break;
        }
    }
    free_client_list(&clients);

    if (unlink(addr.sun_path) == -1) {
        perror("main: unlink");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}

/* manager_status_map: Maps manager status codes to its corresponding
    JSON value. */
json_value *manager_status_map(int status) {
    json_value *val;
    switch (status) {
        case _MANAGER_NOT_ASSIGNED:
            val = json_string_new("not_assigned");
            break;
        case _MANAGER_ASSIGNED:
            val = json_string_new("assigned");
            break;
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
        if (logging_level == debug)
            printf("%s\n", buf);

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

/* init_client_list: Initializes the list. */
void init_client_list(struct client_list *l) {
    int err;
    l->head = NULL;
    l->len = 0;
    if ((err = pthread_mutex_init(&l->lock, NULL)) == -1) {
        fprintf(stderr, "main: init_client_list: pthread_mutex_init: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    return;
}

/* add_client: Adds a new client to the list and returns a pointer to the
    newly created node. */
struct client_node *add_client(struct client_list *l, int sd) {
    int err;
    struct client_node *node;
    if ((node = malloc(sizeof(struct client_node))) == NULL) {
        perror("main: add_client: malloc");
        exit(EXIT_FAILURE);
    }
    node->sd = sd;
    node->prev = NULL;
    if ((err = pthread_mutex_lock(&l->lock)) != 0) {
        fprintf(stderr, "main: add_client: pthread_mutex_lock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    node->next = l->head;
    l->head = node;
    l->len++;
    if ((err = pthread_mutex_unlock(&l->lock)) != 0) {
        fprintf(stderr, "main: add_client: pthread_mutex_unlock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    return node;
}

/* remove_node: Removes the node from the list. */
void remove_node(struct client_list *l, struct client_node *n) {
    int err;
    if ((err = pthread_mutex_lock(&l->lock)) != 0) {
        fprintf(stderr, "main: remove_node: pthread_mutex_lock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    l->len--;
    if (l->len == 0)
        l->head = NULL;
    if (n->prev)
        n->prev->next = n->next;
    else
        l->head = n->next;
    if (n->next)
        n->next->prev = n->prev;
    if ((err == pthread_mutex_unlock(&l->lock)) != 0) {
        fprintf(stderr, "main: remove_node: pthread_mutex_unlock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    free(n);
    return;
}

/* free_client_list: Frees all of the resources allocated to the list. */
void free_client_list(struct client_list *l) {
    int err;
    if ((err = pthread_mutex_lock(&l->lock)) != 0) {
        fprintf(stderr, "main: free_client_list: pthread_mutex_lock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    if (l->len == 0)
        goto unlock;
    
    struct client_node *curr = l->head;
    while (curr->next) {
        if ((err = pthread_cancel(curr->tid)) != 0)
            fprintf(stderr, "main: free_client_list: pthread_cancel: %s\n", strerror(err));
        curr = curr->next;
        free(curr->prev);
    }
    if ((err = pthread_cancel(curr->tid)) != 0)
        fprintf(stderr, "main: free_client_list: pthread_cancel: %s\n", strerror(err));
    free(curr);

    unlock:
    if ((err = pthread_mutex_unlock(&l->lock)) != 0) {
        fprintf(stderr, "main: free_client_list: pthread_mutex_unlock: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = pthread_mutex_destroy(&l->lock)) != 0) {
        fprintf(stderr, "main: free_client_list: pthread_mutex_destroy: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }
    return;
}

void signal_handler(int signo) {
    switch (signo) {
        case SIGINT:
            int_flag = 1;
            break;
        case SIGPIPE:
            // Close connection with client
            break;
    }
    return;
}
