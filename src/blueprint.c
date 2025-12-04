#include <stdio.h>

#include "blueprint.h"
#include "json-helpers.h"

/* blueprint_destroy: Destroys the blueprint and frees its resources. */
void blueprint_destroy(Blueprint* blueprint) {
    if (blueprint == NULL) return;
    
    switch (blueprint->kind) {
        case BLUEPRINT_JOB:
            job_destroy(blueprint->as.job);
            break;
        case BLUEPRINT_PROJECT:
            project_destroy(blueprint->as.project);
            break;
        default:
            fprintf(stderr, "blueprint_destroy: Warning: Unknown blueprint kind %d\n", blueprint->kind);
            break;
    }
    free(blueprint);
}


/* blueprint_decode: Decodes the JSON object and returns a blueprint. */
Blueprint* blueprint_decode(const json_value* obj, int kind) {
    Blueprint* blueprint = malloc(sizeof(Blueprint));
    if (blueprint == NULL) {
        perror("blueprint_decode: malloc");
        return NULL;
    }

    switch (kind) {
        case BLUEPRINT_JOB:
            blueprint->kind = kind;
            blueprint->as.job = job_decode(obj);
            if (blueprint->as.job == NULL) {
                free(blueprint);
                return NULL;
            }
            break;
        case BLUEPRINT_PROJECT:
            blueprint->kind = kind;
            blueprint->as.project = project_decode(obj);
            if (blueprint->as.project == NULL) {
                free(blueprint);
                return NULL;
            }
            break;
        default:
            free(blueprint);
            return NULL;
    }

    return blueprint;
}

/* blueprint_status_encode: Encodes the status of the blueprint into a
    JSON object. */
json_value* blueprint_status_encode(Blueprint* blueprint) {
    switch (blueprint->kind) {
        case BLUEPRINT_JOB:
            return job_status_encode(job_get_status(blueprint->as.job));
        case BLUEPRINT_PROJECT:
            return project_status_encode(project_get_status(blueprint->as.project));
    }
    return NULL;
}
