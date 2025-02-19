#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "worker.h"
#include "cJSON.h"

// Debug level
#define DEBUG 1

// cJSON macros
#define Bool CJSON_PUBLIC(cJSON_bool)
#define Invalid cJSON_Invalid
#define True cJSON_True
#define False cJSON_False
#define CreateObject() cJSON_CreateObject()
#define Delete(item) cJSON_Delete(item)
#define IsNull(item) cJSON_IsNull(item)
#define IsValid(item) cJSON_IsInvalid(item)
#define IsObject(item) cJSON_IsObject(item)
#define HasItem(obj,name) cJSON_HasObjectItem(obj,name)
#define GetItem(obj,name) cJSON_GetObjectItem(obj,name)
#define AddItem(obj,name,item) cJSON_AddItemToObject(obj,name,item)
#define CreateNumber(num) cJSON_CreateNumber(num)
#define CreateString(str) cJSON_CreateString(str)
#define CreateJob(obj) cJSON_CreateJob(obj)
#define CallCommand(obj,ret) cJSON_CallCommand(obj,ret)
#define Marshal(item) cJSON_PrintUnformatted(item)
#define Unmarshal(str) cJSON_Parse(str)

// Helper functions
Job *cJSON_CreateJob(cJSON *obj);
Bool cJSON_CallCommand(cJSON *obj, cJSON *ret);

#define BUFFLEN 1024

char buf[BUFFLEN];
Worker *worker;

int main(int argc, char *argv[]) {
    // Check argument
    if (argc != 2) {
        fprintf(stderr, "main: expected 1 argument, received %d arguments\n", argc-1);
        exit(EXIT_FAILURE);
    }

    worker = create_worker((int) strtol(argv[1],NULL,0));

    // Create a local socket
    char *name, *s;
    struct sockaddr_un addr;
    int sd;
    socklen_t size;

    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_LOCAL;

    // Create socket name with worker id
    s = stpncpy(s, "worker", sizeof(addr.sun_path) - 1);
    s = stpncpy(s, argv[1], sizeof(addr.sun_path) - (6+1));
    *s = '\0';
    strcpy(addr.sun_path,name);

    size = SUN_LEN(&addr);
    
    if ((sd = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("main: socket");
        exit(EXIT_FAILURE);
    }

    if (unlink(addr.sun_path) == 1) {
        perror("main: unlink");
        exit(EXIT_FAILURE);
    }

    if (bind(sd, (struct sockaddr *) &addr, size) == -1) {
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
            obj = Unmarshal(buf);
            ret = CreateObject();

            // Check obj
            if (IsValid(obj) == Invalid) {
                item = CreateString("invalid json object");
                AddItem(ret,"error",item);
            }
            // Check if obj is a json object
            else if (IsObject(obj) == False) {
                item = CreateString("invalid json object");
                AddItem(ret, "error", item);
            } 
            // Check if obj has a command item
            else if (HasItem(obj,"command") == False) {
                item = CreateString("missing command item");
                AddItem(ret, "error", item);
            }
            // Pass the command to the worker
            else if (CallCommand(obj,ret) == False) {
                item = CreateString("invalid command");
                AddItem(ret, "error", item);
            }

            // Add debugging info
            if (DEBUG == 1) {
                item = CreateString(Marshal(obj));
                AddItem(ret, "debug", item);
            }

            // Copy object to buffer
            strncpy(buf, Marshal(ret), BUFFLEN);
            printf("%s\n",buf);

            // Send return object to client
            if (send(client_fd, buf, strnlen(buf,BUFFLEN), 0) == -1)
                perror("main: send");

            Delete(ret);
            Delete(obj);
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

/* cJSON_CallCommand: Calls one of the worker's commands and adds its
    status to ret. If the called command includes a side effect, it adds
    the status of the side effect to ret. Lastly, if the function succeeds,
    it returns True, otherwise, it returns False. */
Bool cJSON_CallCommand(cJSON *obj, cJSON *ret) {
    int status, job_status;
    cJSON *item;
    char *command = GetItem(obj,"command")->valuestring;
    
    // Get worker status
    if (strcmp(command,"get_status") == 0) {
        status = get_status(worker,NULL);
        item = CreateNumber(status);
        AddItem(ret,"status",item);
    }
    // Run Job
    else if (strcmp(command,"run_job") == 0) {
        // Check obj for job
        if (HasItem(obj,"job") == False) {
            item = CreateString("missing job");
            AddItem(ret,"error",item);
            return False;
        }

        // Create new job
        item = GetItem(obj,"job");
        if (run_job(worker,CreateJob(item)) == -1) {
            item = CreateString("worker is already working");
            AddItem(ret,"error",item);
        } else {
            status = get_status(worker,&job_status);
            item = CreateNumber(status);
            AddItem(ret,"status",item);
            item = CreateNumber(job_status);
            AddItem(ret,"job_status",item);
        }
    }
    // Start job
    else if (strcmp(command,"start") == 0) {
        // Check worker status
        if (worker->status == _WORKER_WORKING) {
            item = CreateString("worker is already working");
            AddItem(ret,"error",item);
        }
        // Start job
        else if (start(worker) == -1) {
            item = CreateString("no job assigned to worker");
            AddItem(ret,"error",item);
        }
        // Get status
        else {
            status = get_status(worker,&job_status);
            item = CreateNumber(status);
            AddItem(ret,"status",item);
            item = CreateNumber(job_status);
            AddItem(ret,"job_status",item);
        }
    }
    // Stop Job
    else if (strcmp(command,"stop") == 0) {
        stop(worker);
        status = get_status(worker,NULL);
        item = CreateNumber(status);
        AddItem(ret,"status",item);
    }
    // Invalid Command
    else {
        item = CreateString("invalid command");
        AddItem(ret,"error",item);
        return False;
    }

    return True;
}

/* cJSON_CreateJob: Creates a new job from an json object. The object
    is expected to formatted in the following way,
    {"id":number, "tasks":{"task1","task2",...}. */
Job *cJSON_CreateJob(cJSON *obj) {
    cJSON *id = GetItem(obj,"id");
    Job *job = create_job(id->valuedouble,_JOB_INCOMPLETE);
    for (cJSON *task = GetItem(obj,"tasks"); task; task = task->next) {
        add_task(job,task->string);
    }
    return job;
}
