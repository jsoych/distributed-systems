#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "manager.h"

/* create_manager: Creates a new manager. */
Manager *create_manager(int id) {
    Manager *man;
    if ((man = malloc(sizeof(Manager))) == NULL) {
        perror("manager: malloc");
        exit(EXIT_SUCCESS);
    }
    man->id = id;
    man->status = _MANAGER_NOT_BUSY;
    man->running_proj = NULL;
    return man;
}

/* free_manager: Frees the memory allocated to the manager. */
void free_manager(Manager *man) {
    // Free running project memory

    free(man);
}

/* get_status: Gets the manager status. */
int get_status(Manager *man) {
    return man->status;
}

/* get_project_status: Gets the status of the running project. */
int get_project_status(Manager *man) {
    return man->running_proj->proj->status;
}

// Supervisor handles
static int assign_job(Supervisor *, Job *);
static int update_job_status(Supervisor *, Job *);

static struct commmand {
    char *cmd;
    void *args;
};

/* call_command: Calls one of the manager's commands and returns a
    response JSON object. */
cJSON *call_command(Manager *man, cJSON *cmd) {
    cJSON *obj, *item, *res;
    res = cJSON_CreateObject();
    
    return res;
}
