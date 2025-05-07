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
#include "cJSON.h"

#define True 1
#define False 0

// cJSON macros
#define CreateObject() cJSON_CreateObject()
#define Delete(item) cJSON_Delete(item)
#define IsInvalid(item) cJSON_IsInvalid(item)
#define IsObject(item) cJSON_IsObject(item)
#define HasItem(obj,name) cJSON_HasObjectItem(obj,name)
#define GetItem(obj,name) cJSON_GetObjectItem(obj,name)
#define AddItem(obj,name,item) cJSON_AddItemToObject(obj,name,item)
#define CreateNumber(num) cJSON_CreateNumber(num)
#define CreateString(str) cJSON_CreateString(str)
#define CreateNull() cJSON_CreateNull()
#define CallCommand(obj,ret) cJSON_CallCommand(obj,ret)
#define Marshal(item) cJSON_PrintUnformatted(item)
#define Unmarshal(str) cJSON_Parse(str)

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

// log levels
enum {
    info,
    debug
};

int logging_level;

#define BUFLEN 1024
char buf[BUFLEN];

Worker *worker;
Job *job;

int main(int argc, char *argv[]) {
    // Check argument
    if (argc != 3) {
        fprintf(stderr, "main: expected 2 arguments, received %d arguments\n", argc-1);
        exit(EXIT_FAILURE);
    }

    // Create worker
    worker = create_worker((int) strtol(argv[1], NULL, 0));
    job = NULL;

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
        "/Users/leejahsprock/pyoneer/run/worker%d.socket",
        worker->id,
        sizeof(addr.sun_path)
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
    int sd;
    if ((sd = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("main: socket");
        exit(EXIT_FAILURE);
    }

    if (unlink(addr.sun_path) == 1) {
        perror("main: unlink");
        exit(EXIT_FAILURE);
    }

    if (bind(sd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("main: bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sd,1) == -1) {
        perror("main: listen");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_storage client_addr;
    socklen_t addr_len;
    int client_fd, nbytes, status;
    json_value *obj, *res, *val, *cmd;
    while (1) {
        // Accept client connection
        if ((client_fd = accept(sd, (struct sockaddr *) &client_addr, &addr_len)) == -1) {
            perror("main: accept");
            exit(EXIT_FAILURE);
        }

        while ((nbytes = recv(client_fd, buf, BUFLEN, BUFLEN)) > 0) {
            buf[(nbytes >= BUFLEN) ? BUFLEN - 1 : nbytes] = '\0';
            obj = json_parse(buf, BUFLEN);
            if (obj == NULL) {
                dprintf(logfd, "main: json_parse: Warning: Parsed %s to NULL\n", buf);
                continue;
            }

            res = json_object_new(0);
            if (res == NULL) {
                fprintf(stderr, "main: json_object_new: Error: Unable to create new JSON object\n");
                json_value_free(res);
                close(client_fd);
                goto close;
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

            // Call command
            if (strcmp(cmd->u.string.ptr, "run_job")) {
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

                switch (get_status(worker)) {
                    case _WORKER_NOT_WORKING:
                        val = json_string_new("not_working");
                        break;
                    case _WORKER_WORKING:
                        val = json_string_new("working");
                        break;
                }
                json_object_push(res, "status", val);
            }
            
            if (logging_level == debug) {
                json_object_push(res, "debug", obj);
            }
            send:
            json_serialize(buf, res);
            if (send(client_fd, buf, strlen(buf), 0) == -1) {
                perror("main: send");
                json_value_free(obj);
                json_value_free(res);
                close(client_fd);
                goto close;
            }


            json_value_free(obj);
            json_value_free(res);
        }
        // Check recv return value
        if (nbytes == -1) {
            perror("main: recv");
            exit(EXIT_FAILURE);
        }

        if (close(client_fd) == -1) {
            perror("main: close(client_fd)");
            exit(EXIT_FAILURE);
        }
    }
    
    close:
    if (close(sd) == -1) {
        perror("main: close(sd)");
        exit(EXIT_FAILURE);
    }

    free_worker(worker);
    if (job) free_job(job);
    exit(EXIT_SUCCESS);
}
