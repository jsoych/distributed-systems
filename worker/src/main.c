#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pthread.h>
#include "worker.h"
#include "json.h"

#define BUFLEN 1024
#define MAXLEN 64

// Globals
Worker *worker;
int logging_level;
volatile sig_atomic_t sig_flag;

typedef enum {
    client_active,
    client_inactive
} client_status;

typedef struct _client {
    int sockfd;
    client_status status;
} Client;

Client *create_client(int);
void free_client(Client *);

typedef struct _client_node {
    Client *client;
    struct _client_node *next;
} client_node;

typedef struct _client_list {
    int len;
    client_node *head;
} client_list;

// Logging levels
enum {
    logging_info,
    logging_debug
};

// Helper functions
void init_client_list(client_list *);
void free_client_list(client_list *);
void add_client(client_list *, Client *);
int remove_inactive_clients(client_list *);
json_value *worker_status_map(int);
json_value *job_status_map(int);
json_value *json_get_value(json_value *, char *);
void signal_handler(int);
void *worker_thread(void *);

int main(int argc, char *argv[]) {
    // Check argument
    if (argc != 3) {
        fprintf(stderr, "main: expected 2 arguments, received %d arguments\n", argc-1);
        exit(EXIT_FAILURE);
    }

    // Create worker
    worker = create_worker((int) strtol(argv[1], NULL, 0));

    // Set logging level
    if (strcmp(argv[2],"debug") == 0)
        logging_level = logging_debug;
    else
        logging_level = logging_info;
    
   
#ifdef __APPLE__
    snprintf(
        addr.sun_path,
        sizeof(addr.sun_path),
        "/Users/leejahsprock/pyoneer/run/worker%d.socket",
        worker->id
    );
#elif __linux__
    snprintf(
        addr.sun_path,
        sizeof(addr.sun_path),
        "/home/jsoychak/pyoneer/run/worker%d.socket",
        worker->id
    );
#endif

    // Set signal handler
    sig_flag = 0;
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("main: sigaction");
        exit(EXIT_FAILURE);
    }
    
    // Create unix socket
    int serv;
    if ((serv = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("main: socket");
        exit(EXIT_FAILURE);
    }

    if (bind(serv, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("main: bind");
        exit(EXIT_FAILURE);
    }

    if (listen(serv, 1) == -1) {
        perror("main: listen");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    int old_errno, sockfd;
    pthread_t tid;
    Client *client;
    client_list list;
    init_client_list(&list);
    while (sig_flag == 0) {
        remove_inactive_clients(&list);
        if ((sockfd = accept(serv, (struct sockaddr *) &client_addr, &addr_len)) == -1) {
            perror("main: accept");
            continue;
        }
        client = create_client(sockfd);
        old_errno = pthread_create(&tid, NULL, worker_thread, client);
        if (old_errno != 0) {
            fprintf(stderr, "main: pthread_create: error: %s\n", strerror(old_errno));
            free_client(client);
            continue;
        }
        add_client(&list, client);
    }
    free_client_list(&list);
    
    if (close(serv) == -1) {
        perror("main: server: close");
        exit(EXIT_FAILURE);
    }

    if (unlink(addr.sun_path) == -1) {
        perror("main: unlink");
        exit(EXIT_FAILURE);
    }

    free_worker(worker);
    exit(EXIT_SUCCESS);
}

/* create_client: Creates a new client. */
Client *create_client(int sockfd) {
    Client *client;
    if ((client = malloc(sizeof(Client))) == NULL) {
        perror("main: create_client");
        exit(EXIT_FAILURE);
    }
    client->sockfd = sockfd;
    client->status = client_active;
    return client;
}

/* free_client: Frees the client resources. */
void free_client(Client *client) {
    if (client->status == client_active)
        close(client->sockfd);
    free(client);
    return;
}

/* init_client_list: Intializes the client list. */
void init_client_list(client_list *l) {
    l->len = 0;
    l->head = NULL;
    return;
}

/* free_client_list: Frees all the client list resources. */
void free_client_list(client_list *l) {
    if (l->len == 0)
        return;
    client_node *prev, *curr = l->head;
    while (curr->next) {
        prev = curr;
        curr = curr->next;
        free_client(prev->client);
        free(prev);
    }
    free_client(curr->client);
    free(curr);
    l->len = 0;
    l->head = NULL;
    return;
}

/* add_client: Adds the client to the head of the list. */
void add_client(client_list *l, Client *c) {
    client_node *n;
    if ((n = malloc(sizeof(client_node))) == NULL) {
        perror("main: add_client");
        exit(EXIT_FAILURE);
    }
    n->client = c;
    n->next = l->head;
    l->head = n;
    return; 
}

/* remove_inactive_clients: Removes the inactive clients from the list and
    returns the number of clients removed. */
int remove_inactive_clients(client_list *l) {
    int count = 0;
    if (l->len == 0)
        return count;
    // head
    client_node *prev, *curr = l->head;
    while (curr) {
        if (curr->client->status == client_active)
            break;
        prev = curr;
        curr = curr->next;
        free_client(prev->client);
        free(prev);
        l->len--;
        count++;
    }
    l->head = curr;
    if (l->len == 0)
        return count;

    // body
    client_node *next = curr->next;
    prev = curr;
    while (next) {
        curr = next;
        next = curr->next;
        if (curr->client->status == client_active) {
            prev->next = curr;
            prev = curr;
        }
        else {
            free_client(curr->client);
            free(curr);
            l->len--;
            count++;
        }
        curr = next;
    }
    prev->next = next;
    return count;
}

/* worker_status_map: Maps worker status codes to its corresponding
    JSON value. */
json_value *worker_status_map(int status) {
    json_value *val;
    switch (status) {
        case _WORKER_NOT_ASSIGNED:
            val = json_string_new("not_assigned");
            break;
        case _WORKER_NOT_WORKING:
            val = json_string_new("not_working");
            break;
        case _WORKER_WORKING:
            val = json_string_new("working");
            break;
        default:
            val = json_null_new();
            break;
    }
    return val;
}

/* job_status_map: Maps job status codes to its corresponding
    JSON value. */
json_value *job_status_map(int status) {
    json_value *val;
    switch (status) {
        case _JOB_RUNNING:
            val = json_string_new("running");
            break;
        case _JOB_COMPLETED:
            val = json_string_new("completed");
            break;
        case _JOB_INCOMPLETE:
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
    for (unsigned int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, name) == 0) {
            val = obj->u.object.values[i].value;
            break;
        }
    }
    return val;
}

/* signal_handler: Raises the sig_flag. */
void signal_handler(int signo) {
    switch (signo) {
        case SIGINT: sig_flag = 1; break;
        default: sig_flag = 1; break;
    }
    return;
}

/* worker_thread: */
void *worker_thread(void *arg) {
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
