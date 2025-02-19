#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "worker.h"
#include "cJSON.h"

#define Run_Job(job) run_job(worker, cJSON_CreateJob(job))
#define Get_Status(ret) cJSON_AddWorkerStatus(ret)
#define Start() start(worker)
#define Stop() stop(worker)

#define BUFFLEN 1024

char buf[BUFFLEN];

void print_status(FILE *);
void print_job_status(FILE *, Job *);
Job *cJSON_CreateJob(cJSON *);
CJSON_PUBLIC(cJSON_bool) cJSON_AddWorkerStatus(cJSON *);
CJSON_PUBLIC(cJSON_bool) cJSON_CommandWorker(cJSON *, cJSON *);

int quit_flag = 0;
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
    cJSON *obj, *ret;
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
            ret = cJSON_CreateObject();

            // Check obj
            if (cJSON_IsNull(obj)) {
                item = cJSON_CreateString("invalid");
                cJSON_AddItemToObject(ret, "error", item);
            }
            // Check if obj is a json object
            else if (cJSON_IsObject(obj) == cJSON_False) {
                item = cJSON_CreateString("invalid json object");
                cJSON_AddItemToObject(ret, "error", item);
                // Return the parsed json object for debugging
                item = cJSON_CreateString(cJSON_PrintUnformatted(obj));
                cJSON_AddItemToObject(ret, "debug", item);
            } 
            // Check if obj has a command item
            else if (cJSON_HasObjectItem(obj,"command") == cJSON_False) {
                item = cJSON_CreateString("missing command item");
                cJSON_AddItemToObject(ret, "error", item);
                // Return the parsed json object for debugging
                item = cJSON_CreateString(cJSON_PrintUnformatted(obj));
                cJSON_AddItemToObject(ret, "debug", item);
            }
            // Pass the command to the worker
            else if (cJSON_Command(obj) == cJSON_False) {
                item = cJSON_CreateString("invalid command");
                cJSON_AddItemToObject(ret, "error", item);
                // Return the parsed json object for debugging
                item = cJSON_CreateString(cJSON_PrintUnformatted(obj));
                cJSON_AddItemToObject(ret, "debug", item);
            }
            // Add debugging info to ret
            else {
                item = cJSON_CreateString(cJSON_PrintUnformatted(obj));
                cJSON_AddItemToObject(ret, "debug", item);
            }

            // Copy object to buffer
            strncpy(buf,cJSON_PrintUnformatted(ret),BUFFLEN);
            printf("%s\n",buf);

            // Send return object to client
            if (send(client_fd, buf, strnlen(buf,BUFFLEN), 0) == -1)
                perror("main: send");

            cJSON_Delete(ret);
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

CJSON_PUBLIC(cJSON_bool) cJSON_CommandWorker(cJSON *obj, cJSON *ret) {
    Job *job;
    cJSON *item, *id, *tasks, *task;

    // Get command
    char *command = cJSON_GetObjectItem(obj,"command")->valuestring;

    // Get status
    if (strcmp(command,"get_status") == 0)
        ;
    // Run job
    else if (strcmp(command,"run_job") == 0) {
        if (cJSON_HasObjectItem(obj,"job") == cJSON_True) {
            item = cJSON_GetObjectItem(obj,"job");
            id = cJSON_GetObjectItem(item, "id");
            job = create_job((int) id->valuedouble,_JOB_INCOMPLETE);
            tasks = cJSON_GetObjectItem(item,"tasks");
            for (task = tasks; task; task->next) {
                add_task(job,task->string);
            }
            if (Run_Job(job) == -1) {
                item = cJSON_CreateString("worker is already working");
                cJSON_AddItemToObject(ret, "error", item);
            }
        }
    }
    // Start working
    else if (strcmp(command,"start") == 0) 
        Start();
    // Stop working
    else if (strcmp(command,"stop") == 0)
        Stop();
    // Invalid Command
    else {
        item = cJSON_CreateString("invalid command");
        cJSON_AddItemToObject(ret, "error", item);
        item = cJSON_CreateString(command);
        cJSON_AddItemToObject(ret, "command", item);
        return cJSON_False;
    }
    
    Get_Status(ret);
    return cJSON_True;
}