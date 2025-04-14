#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include "manager.h"
#include "project.h"
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
#define Marshal(item) cJSON_PrintUnformatted(item)
#define Unmarshal(str) cJSON_Parse(str)

#define BUFFLEN 4096

char buf[BUFFLEN];
Manager *manager;
Project *project;

int main(int argc, char *argv[]) {
    if (argc < 1) {
        fprintf(stderr, "main: expected 1 or 2 arguments, recieved %d\n", argc-1);
        exit(EXIT_FAILURE);
    }

    manager = create_manager(strtol(argv[1], NULL, 10));
    project = NULL;

    // Create a local socket
    struct sockaddr_un addr;
    char path[sizeof(addr.sun_path)];
    char *s = path;
    int len, sd;
    socklen_t size;

    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_LOCAL;

    // Create the manager socket path
    len = sizeof(path);
#ifdef __APPLE__
    s = stpncpy(s, "/Users/leejahsprock/pyoneer/run/", len-1);
#elif __linux__
    s = stpcpy(s, "/run/pyoneer/", len-1);
#endif
    s = stpncpy(s, "manager.socket", len-1);
    *s = '\0';

    strcpy(addr.sun_path,path);
    size = SUN_LEN(&addr);

    if ((sd = socket(PF_LOCAL, SOCK_STREAM, 0)) == -1) {
        perror("main: socket");
        exit(EXIT_FAILURE);
    }

    if (unlink(addr.sun_path) == -1) {
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

    int client_sd, nbytes;
    struct sockaddr_storage client_addr;
    size = sizeof(client_addr);
    cJSON *obj, *ret, *item;
    while (True) {
        // Accept client connection
        if ((client_sd = connect(sd, (struct sockaddr *) &client_addr, &size)) == -1) {
            perror("main: connect");
            exit(EXIT_FAILURE);
        }

        while ((nbytes = recv(client_sd, buf, BUFFLEN, BUFFLEN)) > 0) {
            buf[(nbytes > BUFFLEN) ? BUFFLEN-1 : nbytes] = '\0';
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
                
            }

            Delete(obj);
            Delete(ret);
        }

    }

    if (close(sd) == -1) {
        perror("main: close");
        exit(EXIT_FAILURE);
    }

    free_manager(manager);
    if (project)
        free_project(project);
    exit(EXIT_SUCCESS);
}