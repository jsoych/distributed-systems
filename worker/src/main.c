#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "worker.h"
#include "json.h"

#define BUFLEN 1024

// Logging levels
enum {
    info,
    debug
};

// Helper functions
json_value *worker_status_map(int);
json_value *job_status_map(int);
json_value *json_get_value(json_value *, char *);

// Globals
char buf[BUFLEN];
int logging_level;

int main(int argc, char *argv[]) {
    // Check argument
    if (argc != 3) {
        fprintf(stderr, "main: expected 2 arguments, received %d arguments\n", argc-1);
        exit(EXIT_FAILURE);
    }

    // Create worker
    Worker *worker = create_worker((int) strtol(argv[1], NULL, 0));
    Job *job = NULL;

    // Set logging level
    if (strcmp(argv[2],"debug") == 0)
        logging_level = debug;
    else
        logging_level = info;
    
    // Create logging file
    bzero(buf, BUFLEN);
#ifdef __APPLE__
    sprintf(buf, "/Users/leejahsprock/pyoneer/var/log/worker%d.log", worker->id);
#elif __linux__
    sprintf(buf, "/var/log/pyoneer/worker%d.log", worker->id);
#endif
    int logfd;
    if ((logfd = open(buf, O_CREAT | O_APPEND | O_WRONLY, 0666)) == -1) {
        perror("main: open");
        exit(EXIT_FAILURE);
    }

    // Create a local socket
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_LOCAL;
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
        "/run/pyoneer/worker%d.socket",
        worker->id,
        sizeof(addr.sun_path)
    );
#endif
    
    // Create unix socket
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

    if (listen(serv, 1) == -1) {
        perror("main: listen");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    int client, nbytes;
    json_value *obj, *res, *val, *cmd;
    json_settings settings;
    settings.value_extra = json_builder_extra;
    char error[128];
    while (1) {
        // Accept client connection
        if ((client = accept(serv, (struct sockaddr *) &client_addr, &addr_len)) == -1) {
            perror("main: accept");
            exit(EXIT_FAILURE);
        }

        while ((nbytes = recv(client, buf, BUFLEN, BUFLEN)) > 0) {
            buf[(nbytes >= BUFLEN) ? BUFLEN - 1 : nbytes] = '\0';
            if ((res = json_object_new(0)) == NULL) {
                dprintf(logfd, "main: json_object_new: Error: Unable to create new JSON object\n");
                close(client);
                close(serv);
                exit(EXIT_FAILURE);
            }

            if ((obj = json_parse_ex(&settings, buf, BUFLEN, error)) == NULL) {
                obj = json_null_new();
                goto send;
            }

            // Check object type
            if (obj->type != json_object) {
                val = json_string_new("invalid JSON type");
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
            // run_job
            if (strcmp(cmd->u.string.ptr, "run_job") == 0) {
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

                if (run_job(worker, job) == -1) {
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

            send:
            if (logging_level == debug)
                json_object_push(res, "debug", obj);
            
            if (json_measure(res) > BUFLEN) {
                dprintf(logfd, "main: Error: Buffer length too small\n");
                strcpy(buf, "{\"error\":\"API failure\"}");
            } else {
                json_serialize(buf, res);
            }

            if (send(client, buf, strlen(buf), 0) == -1) {
                perror("main: send");
                json_builder_free(res);
                json_builder_free(obj);
                exit(EXIT_FAILURE);
            }

            if (logging_level != debug)
                json_builder_free(obj);
            json_builder_free(res);
        }

        // Check recv return value
        if (nbytes == -1) {
            perror("main: recv");
            exit(EXIT_FAILURE);
        }

        if (close(client) == -1) {
            perror("main: client: close");
            exit(EXIT_FAILURE);
        }
    }
    
    if (close(serv) == -1) {
        perror("main: server: close");
        exit(EXIT_FAILURE);
    }

#if __linux__
    close(logfd);
#endif

    free_worker(worker);
    if (job != NULL)
        free_job(job);
    exit(EXIT_SUCCESS);
}

/* worker_status_map: Maps worker status codes to its corresponding
    JSON value. */
json_value *worker_status_map(int status) {
    json_value *val;
    switch (status) {
        case _WORKER_NOT_WORKING:
            val = json_string_new("not_working");
            break;
        case _WORKER_WORKING:
            val = json_string_new("working");
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
    for (int i = 0; i < obj->u.object.length; i++) {
        if (strcmp(obj->u.object.values[i].name, name) == 0) {
            val = obj->u.object.values[i].value;
            break;
        }
    }
    return val;
}
