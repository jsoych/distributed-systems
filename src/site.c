#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "site.h"

/* site_create: Creates a new site. */
Site* site_create(const char* python, const char* task_dir,
    const char* working_dir) {
    if (!python || !task_dir || !working_dir) return NULL;

    Site* site = malloc(sizeof(Site));
    if (!site) return NULL;
    
    site->python = malloc(strlen(python) + 1);
    site->task_dir = malloc(strlen(task_dir) + 1);
    site->working_dir = malloc(strlen(working_dir) + 1);
    strcpy(site->python, python);
    strcpy(site->task_dir, task_dir);
    stpcpy(site->working_dir, working_dir);
    return site;
}

/* site_destroy: Destroys the site and releases of its resources. */
void site_destroy(Site* site) {
    if (!site) return;
    free(site->python);
    free(site->task_dir);
    free(site->working_dir);
    free(site);
}
