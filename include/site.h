#ifndef _SITE_H
#define _SITE_H

typedef struct _site {
    char* python;
    char* task_dir;
    char* working_dir;
} Site;

Site* site_create(const char* python, 
    const char* task_dir, const char* working_dir);
void site_destroy(Site* site);

#endif
