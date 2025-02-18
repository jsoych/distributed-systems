#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "worker.h"
#include "cJSON.h"

#define BUFFLEN 1024

char buf[BUFFLEN];

void print_status(FILE *);
void print_job_status(FILE *, Job *);
Worker *worker;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "main: expected 1 argument, received %d arguments\n", argc-1);
        exit(EXIT_FAILURE);
    }

    worker = create_worker((int) strtol(argv[1],NULL,0));

    // Create named socket
    struct sockaddr_un name;
    int sd;
    socklen_t size;

    memset(&name,0,sizeof(name));
    name.sun_family = AF_LOCAL;

    // Append worker id to socket name
    char *s;
    s = stpcpy(name.sun_path, "worker");
    strcpy(s,argv[1]);
    size = SUN_LEN(&name);
    
    if ((sd = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("main: socket");
        exit(EXIT_FAILURE);
    }

    if (unlink(name.sun_path) == 1) {
        perror("main: unlink");
        exit(EXIT_FAILURE);
    }

    if (bind(sd, (struct sockaddr *) &name, size) == -1) {
        perror("main: bind");
        exit(EXIT_FAILURE);
    }

    if (listen(sd,1) == -1) {
        perror("main: listen");
        exit(EXIT_FAILURE);
    }

    int client_fd, nbytes;
    struct sockaddr_storage client_addr;
    size = sizeof(client_addr);
    cJSON *obj;
    cJSON *command, *item;
    while (1) {
        // Accept client connection
        if ((client_fd = accept(sd, (struct sockaddr *) &client_addr, &size)) == -1) {
            perror("main: accept");
            exit(EXIT_FAILURE);
        }

        while ((nbytes = recv(client_fd, buf, BUFFLEN, BUFFLEN)) > 0) {
            buf[(nbytes >= BUFFLEN) ? BUFFLEN-1 : nbytes] = '\0';
            printf("%s\n",buf);
            obj = cJSON_Parse(buf);

            if (cJSON_IsObject(obj)) {
                command = cJSON_GetObjectItem(obj,"command");

                if (strcmp(command->valuestring,"get_status") == 0)
                    ;
                
                if (strcmp(command->valuestring,"run_job") == 0)
                    ;

                if (strcmp(command->valuestring,"start") == 0)
                    ;
                
                if (strcmp(command->valuestring,"stop") == 0)
                    ;

                if (strcmp(command->valuestring,"quit") == 0)
                    break;
            }

            if (send(client_fd, buf, nbytes, 0) == -1)
                perror("main: send");
            
            cJSON_Delete(obj);
        }
        
        close(client_fd);

        // Check recv return value
        if (nbytes == -1)
            perror("main: recv");

        if (nbytes == 0)
            ;

        if (nbytes > 0)
            break;
    }
    
    close(sd);
    free_worker(worker);
}

// prints the status of the worker
void print_status(FILE *file) {
    char s[15];
    strcpy(s,"invalid status");
    if (worker->status == _WORKER_NOT_WORKING)
        strcpy(s,"not working");
    else if (worker->status == _WORKER_WORKING)
        strcpy(s,"working");
    fprintf(file, "worker status: %s\n", s);
}

// prints the status of the job
void print_job_status(FILE *file, Job *job) {
    char s[15];
    strcpy(s,"invalid status");
    if (job->status == _JOB_INCOMPLETE)
        strcpy(s,"incomplete");
    else if (job->status == _JOB_RUNNING)
        strcpy(s,"running");
    else if (job->status == _JOB_COMPLETED)
        strcpy(s,"completed");
    fprintf(file, "job status: %s\n", s);
}