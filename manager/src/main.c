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
Project *cJSON_CreateProject(cJSON *obj);

#define BUFFLEN 1024

char buf[BUFFLEN];
Manager *man;
Project *proj;

int main(int argc, char *argv[]) {
    man = create_manager();

    // Create a local socket
    struct sockaddr_un addr;
    char path[sizeof(addr.sun_path)];
    char *s = path;
    int len, sd;
    socklen_t size;

    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_LOCAL;

    exit(EXIT_SUCCESS);
}