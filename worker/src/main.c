#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "worker.h"
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
#define CreateJob(obj) cJSON_CreateJob(obj)
#define CallCommand(obj,ret) cJSON_CallCommand(obj,ret)
#define Marshal(item) cJSON_PrintUnformatted(item)
#define Unmarshal(str) cJSON_Parse(str)

// Helper functions
Job *cJSON_CreateJob(cJSON *obj);
void cJSON_CallCommand(cJSON *obj, cJSON *ret);

// log levels
enum log_levels{info, debug};
enum log_levels log_level;

#define BUFFLEN 1024

char buf[BUFFLEN];
Worker *worker;
Job *job;

int main(int argc, char *argv[]) {
    // Check argument
    if (argc != 3) {
        fprintf(stderr, "main: expected 2 arguments, received %d arguments\n", argc-1);
        exit(EXIT_FAILURE);
    }

    // Create logging file and set logging level
    int logfd;
    if (strcmp(argv[2],"debug") == 0) {
        log_level = debug;
#ifdef __APPLE__
        logfd = STDOUT_FILENO;
#elif __linux__
        char *log, *b;
        b = (log = buf);
        b = stpcpy(b,"/var/log/pyoneer/worker");
        b = stpcpy(b,argv[1]);
        b = stpcpy(b,".log");
      
        if ((logfd = open(log, O_CREAT | O_APPEND | O_WRONLY, 0666)) == -1) {
            perror("main: open");
            exit(EXIT_FAILURE);
        }

        // Reset buffer
        memset(buf,'\0',BUFFLEN);
#endif
    } else
        log_level = info;

    // Create worker
    worker = create_worker((int) strtol(argv[1],NULL,0));
    job = NULL;

    // Create a local socket
    struct sockaddr_un addr;
    char path[sizeof(addr.sun_path)];
    char *s = path;
    int len, sd;
    socklen_t size;

    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_LOCAL;

    // Create the worker socket path
    len = sizeof(path);
#ifdef __APPLE__
    s = stpncpy(s, "/Users/leejahsprock/pyoneer/run/", len-1);
#elif __linux__
    s = stpcpy(s, "/run/pyoneer/", len-1);
#endif
    s = stpncpy(s, "worker", len-1);
    s = stpncpy(s, argv[1], len-1);
    s = stpncpy(s, ".socket", len-1);
    *s = '\0';

    if (log_level == debug)
        dprintf(logfd,"%s\n",path);
    
    strcpy(addr.sun_path,path);

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
    cJSON *obj, *ret, *item;
    while (True) {
        // Accept client connection
        if ((client_fd = accept(sd, (struct sockaddr *) &client_addr, &size)) == -1) {
            perror("main: accept");
            exit(EXIT_FAILURE);
        }

        while ((nbytes = recv(client_fd, buf, BUFFLEN, BUFFLEN)) > 0) {
            buf[(nbytes >= BUFFLEN) ? BUFFLEN-1 : nbytes] = '\0';
            obj = Unmarshal(buf);
            ret = CreateObject();

            // Check obj
            if (IsInvalid(obj)) {
                item = CreateString("invalid");
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
            else {
                CallCommand(obj,ret);
            }

            // Add debugging info to the return object
            if (log_level == debug) {
                item = CreateString(Marshal(obj));
                AddItem(ret,"debug",item);
            }

            // Copy the return object to buffer
            strncpy(buf, Marshal(ret), BUFFLEN);

            // Add debugging info to the logging file
            if (log_level == debug)
                dprintf(logfd,"%s\n",buf);

            // Send return object to client
            if (send(client_fd, buf, strnlen(buf,BUFFLEN), 0) == -1)
                perror("main: send");

            Delete(ret);
            Delete(obj);
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
    
    // Clean up and exit
    if (close(sd) == -1) {
        perror("main: close(sd)");
        exit(EXIT_FAILURE);
    }

#if __linux__
    close(logfd);
#endif

    free_worker(worker);
    if (job) free_job(job);
    exit(EXIT_SUCCESS);
}

/* cJSON_CallCommand: Calls one of the worker's commands and adds its
    status to ret. If the called command includes a side effect, it adds
    the status of the side effect to ret. */
void cJSON_CallCommand(cJSON *obj, cJSON *ret) {
    Job *new_job;
    cJSON *item;
    char *command = GetItem(obj,"command")->valuestring;
    
    // Get worker status
    if (strcmp(command,"get_status") == 0) {
        item = CreateNumber(worker->status);
        AddItem(ret,"status",item);
    }
    // Get job status
    else if (strcmp(command,"get_job_status") == 0) {
        if (job == NULL) {
            item = CreateNull();
            AddItem(ret,"job_status",item);
        } else {
            item = CreateNumber(job->status);
            AddItem(ret,"job_status",item);
        }
    }
    // Run Job
    else if (strcmp(command,"run_job") == 0) {
        // Check obj for job
        if (HasItem(obj,"job") == False) {
            item = CreateString("missing job spec");
            AddItem(ret,"error",item);
            return;
        }

        // Create new job
        item = GetItem(obj,"job");
        new_job = cJSON_CreateJob(item);
        if (run_job(worker,new_job) == -1) {
            item = CreateString("worker is already working");
            AddItem(ret,"error",item);
        } else {
            // Replace old job with the new job
            if (job) free_job(job);
            job = new_job;

            // Return worker and job statuses
            item = CreateNumber(worker->status);
            AddItem(ret,"status",item);
            item = CreateNumber(job->status);
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
            item = CreateNumber(worker->status);
            AddItem(ret,"status",item);
            item = CreateNumber(job->status);
            AddItem(ret,"job_status",item);
        }
    }
    // Stop Job
    else if (strcmp(command,"stop") == 0) {
        stop(worker);
        item = CreateNumber(worker->status);
        AddItem(ret,"status",item);
    }
    // Invalid Command
    else {
        item = CreateString("invalid command");
        AddItem(ret,"error",item);
        return;
    }

    return;
}

/* cJSON_CreateJob: Creates a new job from an json object. The object
    is expected to formatted in the following way,
    {"id":number, "tasks":["task1","task2",...]}.*/
Job *cJSON_CreateJob(cJSON *obj) {
    cJSON *id = GetItem(obj,"id");
    Job *new_job = create_job(id->valuedouble,_JOB_INCOMPLETE);
    for (cJSON *task = GetItem(obj,"tasks")->child; task; task = task->next) {
        add_task(new_job,task->valuestring);
    }
    return new_job;
}
