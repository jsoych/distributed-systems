#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"

/* create_project: Creates a new project. */
Project *create_project(int id) {
    Project *proj;
    if ((proj = malloc(sizeof(Project))) == NULL) {
        perror("project: create_project: malloc");
        exit(EXIT_FAILURE);
    }
    proj->id = id;
    proj->status = _PROJECT_READY;
    proj->len = 0;
    proj->jobs.head = NULL;
    proj->jobs.tail = NULL;
    for (int i = 0; i < MAXLEN; i++) {
        proj->jobs_table[i] = NULL;
    }
    return proj;
}

// free_project: Frees the memory allocated to the project.
void free_project(Project *proj) {
    if (proj->len == 0){
        free(proj);
        return;
    }
    project_node *curr = proj->jobs.head;
    while (curr->next) {
        free_job(curr->job);
        free(curr->deps);
        curr = curr->next;
        free(curr->prev);
    }
    free_job(curr->job);
    free(curr->deps);
    free(curr);
    free(proj);
    return;
}

// add_job: Adds the job to the list with the ids of its dependencies.
void add_job(Project *proj, Job *job, int ids[], int len) {
    project_node *node;
    if ((node = malloc(sizeof(project_node))) == NULL) {
        perror("project: add_job: malloc");
        exit(EXIT_FAILURE);
    }
    int *deps;
    if ((deps = malloc(sizeof(int) * len)) == NULL) {
        perror("project: add_job: malloc");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < len; i++) {
        deps[i] = ids[i];
    }
    node->job = job;
    node->deps = deps;
    node->len = len;
    
    // Add new_node to jobs table
    node->next_ent = proj->jobs_table[job->id % MAXLEN];
    proj->jobs_table[job->id % MAXLEN] = node;

    // Add new_node to the jobs list
    node->next = NULL;
    node->prev = proj->jobs.tail;
    if (proj->len == 0) {
        proj->jobs.head = node;
        proj->jobs.tail = node;
        proj->len++;
        return;
    }
    proj->jobs.tail->next = node;
    proj->jobs.tail = node;
    proj->len++;
    return;
}

/* get_node: Gets and returns the node by its job id. Otherwise, NULL. */
static project_node *get_node(Project *proj, int id) {
    project_node *ent = proj->jobs_table[id % MAXLEN];
    while (ent) {
        if (ent->job->id == id)
            break;
        ent = ent->next_ent;
    }
    return ent;
}

/* remove_job: Removes the job from the project by its job id. */
void remove_job(Project *proj, int id) {
    if (proj->len == 0)
        return;

    project_node *node = get_node(proj, id);
    if (node == NULL)
        return;

    proj->len--;
    proj->jobs_table[id % MAXLEN] = node->next_ent;
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        proj->jobs.head = node->next;
    }
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        proj->jobs.tail = node->prev;
    }
    free_job(node->job);
    free(node->deps);
    free(node);
    return;
}

/* get_job: Gets the job from the project and returns it. */
static Job *get_job(Project *proj, int id) {
    project_node *ent =  proj->jobs_table[id % MAXLEN]; 
    while (ent->job->id != id)
        ent = ent->next_ent;
    return ent->job;
}

// deque node
static struct deque_node {
    int val;
    struct deque_node *next;
    struct deque_node *prev;
};

// deque
static struct deque {
    int len;
    struct deque_node *head;
    struct deque_node *tail;
};

// init_deque: Initializes the deque.
static void init_deque(struct deque *d) {
    d->len = 0;
    d->head = NULL;
    d->tail = NULL;
    return;
}

// free_queue: Frees all of the deque memory.
static void free_deque(struct deque *d) {
    if (d->len == 0)
        return;
    d->len = 0;
    struct deque_node *curr;
    curr = d->head;
    while (curr->next) {
        curr = curr->next;
        free(curr->prev);
    }
    free(curr);
    return;
}

// push: Adds a new node to the head of the deque.
static void push(struct deque *d, int v) {
    struct deque_node *n;
    if ((n = malloc(sizeof(struct deque_node))) == NULL) {
        perror("project: push: malloc");
        exit(EXIT_FAILURE);
    }
    n->val = v;
    n->next = d->head;
    if (d->len == 0)
        d->tail = n;
    if (d->len > 0)
        d->head->prev = n;
    d->head = n;
    d->len++;
    return;
}

/* dequeue: Removes the last node from the deque and returns its value.
    Otherwise, it returns -1. */
static int dequeue(struct deque *d) {
    if (d->len == 0)
        return -1;
    
    int r = d->tail->val;
    if (d->len-- == 1) {
        free(d->tail);
        d->head = NULL;
        d->tail = NULL;
        return r;
    }
    d->tail = d->tail->prev;
    free(d->tail->next);
    d->tail->next = NULL;
    return r;
}

// append: Adds a new node to the tail of the deque.
static void append(struct deque *d, int v) {
    struct deque_node *n;
    if ((n = malloc(sizeof(struct deque_node))) == NULL) {
        perror("project: queue: malloc");
        exit(EXIT_SUCCESS);
    }
    n->val = v;
    n->next = NULL;
    n->prev = d->tail;
    if (d->len == 0)
        d->head = n;
    if (d->len > 0)
        d->tail->next = n;
    d->tail = n;
    d->len++;
    return;
}

/* pop: Removes the first node from the deque and returns its value. Otherwise,
    it returns -1. */
static int pop(struct deque *d) {
    int r = -1;
    if (d->len == 0)
        return -1;

    r = d->head->val;
    if (d->len-- == 1) {
        free(d->head);
        d->head = NULL;
        d->tail = NULL;
        return r;
    }
    d->head = d->head->next;
    free(d->head->prev);
    d->head->prev = NULL;
    return r;
}

/* find: Searches for the value and returns 1, if the value is in the
    dequeue. Otherwise, it returns 0. */
static int find(struct deque *d, int v) {
    struct deque_node *curr = d->head;
    while (curr) {
        if (curr->val == v)
            return 1;
        curr = curr->next;
    }
    return 0;
}

// hash table structure
static struct table {
    int len;
    struct deque buckets[MAXLEN];
};

// init_table: Initializes hash table.
static void init_table(struct table *t) {
    t->len = 0;
    for (int i = 0; i < MAXLEN; i++)
        init_deque(&t->buckets[i]);
    return;
}

// free_table: Frees the memory allocated to the table.
static void free_table(struct table *t) {
    if (t->len == 0)
        return;
    for (int i = 0; i < MAXLEN; i++)
        free_deque(&t->buckets[i]);
}

// insert: Inserts a new entry to the hash table.
static void insert(struct table *t, int v) {
    if (find(&t->buckets[v % MAXLEN], v) == 0)
        append(&t->buckets[v % MAXLEN], v);
    return;
}

/* search: Searches the hash table for the value and returns 1, 
    if successful, and 0 otherwise. */
static int search(struct table *t, int v) {
    return find(&t->buckets[v % MAXLEN], v);
}

// path node object
static struct PathNode {
    int val;
    struct PathNode *next;
    struct PathNode *prev;
    struct PathNode *next_ent;
};

// path object
static struct Path {
    int len;
    struct PathNode *head;
    struct PathNode *tail;
    struct PathNode *table[MAXLEN];
};

// create_path: Creates a new path object.
static struct Path *create_path() {
    struct Path *p;
    if ((p = malloc(sizeof(struct Path))) == NULL) {
        perror("project: create_path: malloc");
        exit(EXIT_FAILURE);
    }
    p->len = 0;
    p->head = NULL;
    p->tail = NULL;
    for (int i = 0; i < MAXLEN; i++)
        p->table[i] = NULL;

    return p;
}

// free_path: Frees all the memory allocated to the path.
static void free_path(struct Path *p) {
    if (p->len == 0) {
        free(p);
        return;
    }

    struct PathNode *curr = p->head;
    while (curr->next) {
        curr = curr->next;
        free(curr->prev);
    }
    free(curr);
    free(p);
    return;
}

// print_path: Prints the path.
static void print_path(struct Path *p) {
    if (p->len == 0) {
        printf("path: empty\n");
        return;
    }

    struct PathNode *curr = p->head;
    printf("path: [ %d", curr->val);
    while (curr->next) {
        curr = curr->next;
        printf(", %d", curr->val);
    }
    printf(" ]\n");
    return;
}

// add_vxt: Adds the value to the end of the path.
static void add_vxt(struct Path *p, int v) {
    struct PathNode *pn;
    if ((pn = malloc(sizeof(struct PathNode))) == NULL) {
        perror("project: add: malloc");
        exit(EXIT_FAILURE);
    }
    pn->val = v;
    pn->next = NULL;
    pn->prev = p->tail;
    if (p->len == 0)
        p->head = pn;
    if (p->len > 0)
        p->tail->next = pn;
    p->tail = pn;
    pn->next_ent = p->table[v % MAXLEN];
    p->table[v % MAXLEN] = pn;
    p->len++;
    return;
}

/* dequeue_vxt: Removes the last vertex from the path and returns its
    value. If the path is empty, dequeue_vxt returns -1. */
static int dequeue_vxt(struct Path *p) {
    if (p->len == 0)
        return -1;
    
    // Remove the last vxt from the path
    struct PathNode *vxt = p->tail;
    if (p->len-- == 1) {
        p->head = NULL;
        p->tail = NULL;
    } else {
        p->tail = vxt->prev;
        p->tail->next = NULL;
    }

    // Remove the vxt from the table
    int v = vxt->val;
    struct PathNode *curr = p->table[v % MAXLEN];
    if (curr->val = v) {
        p->table[v % MAXLEN] = curr->next_ent;
        free(vxt);
        return v;
    }

    struct PathNode *prev;
    while (curr->next_ent) {
        prev = curr;
        curr = curr->next_ent;
        if (curr->val == v) {
            prev->next_ent = curr->next_ent;
            free(vxt);
            return v;
        }
    }
    curr->next_ent = NULL;
    free(vxt);
    return v;
}

/* pod_vxt: Removes the first vertex from the path and returns its value.
    If the path is empty, pop_vxt returns -1. */
static int pop_vxt(struct Path *p) {
    if (p->len == 0)
        return -1;
    
    // Remove the first vertex from the path
    struct PathNode *vxt = p->head;
    if (p->len-- == 1) {
        p->head = NULL;
        p->tail = NULL;
    } else {
        p->head = vxt->next;
        p->head->prev = NULL;
    }

    // Remove the vertex from the table
    int v = vxt->val;
    struct PathNode *curr = p->table[v % MAXLEN];
    if (curr->val == v) {
        p->table[v % MAXLEN] = curr->next_ent;
        free(vxt);
        return v;
    }

    struct PathNode *prev;
    while (curr->next) {
        prev = curr;
        curr = curr->next_ent;
        if (curr->val == v) {
            prev->next_ent = curr->next_ent;
            free(vxt);
            return v;
        }
    }
    curr->next_ent = NULL;
    free(vxt);
    return v;
}

/* push_vxt: Adds a new node to the start of the path. */
static void push_vxt(struct Path *p, int val) {
    struct PathNode *n;
    if ((n = malloc(sizeof(struct PathNode))) == NULL) {
        perror("project: push_vxt: malloc");
        exit(EXIT_FAILURE);
    }
    n->val = val;
    n->next = p->head;
    n->prev = NULL;

    // Add the node to the start of the path
    if (p->len == 0)
        p->tail = n;
    else
        p->head->prev = n;
    p->head = n;

    // Add the node to the table
    n->next_ent = p->table[val % MAXLEN];
    p->table[val % MAXLEN] = n;
    p->len++;
    return;
}

/* get_vxt: Gets the vertex from the path with its id and returns a
    pointer to the vertex. If there is no vertex, get_vxt returns NULL. */
static struct PathNode *get_vxt(struct Path *p, int id) {
    struct PathNode *pn = p->table[id % MAXLEN];
    if (pn == NULL)
        return pn;

    while (pn) {
        if (pn->val == id)
            return pn;
        pn = pn->next_ent;
    }
    return pn;
}

/* in_path: Checks if the value is in the path. */
static int in_path(struct Path *p, int v) {
    struct PathNode *ent = p->table[v % MAXLEN];
    if (ent == NULL)
        return 0;
    
    while (ent) {
        if (ent->val == v)
            return 1;
        ent = ent->next_ent;
    }
    return 0;
}

/* get_cycle: Searches for cycles in the project starting with given id
    and returns the first cycle found. */
static struct Path *get_cycle(Project *proj, int id) {
    // Create path
    struct Path *path = create_path();

    // Initialize data structures
    struct table visited;
    init_table(&visited);
    struct deque dq;
    init_deque(&dq);

    // Start DFS
    project_node *node;
    push(&dq, id);
    while (dq.len > 0) {
        insert(&visited, id = dequeue(&dq));
        add_vxt(path, id);
        node = get_node(proj, id);
        if (node->len == 0) {
            dequeue_vxt(path);
            continue;
        }

        // Check for cycles and add node neightbours to dq
        for (int i = 0; i < node->len; i++) {
            id = node->deps[i];

            // Check for cycle
            if (in_path(path, id)) {
                add_vxt(path, id);
                while (pop_vxt(path) != id)
                    ;
                push_vxt(path, id);
                return path;
            }

            if (search(&visited, id) == 0)
                push(&dq, id);
        }
    }

    // Remove all vertices from the last path checked
    while (pop_vxt(path) != -1)
        ;
    
    // Free data structures
    free_deque(&dq);
    free_table(&visited);

    return path;
}

/* audit_project: Checks the integrity of the project dependency graph. If
    there is no cycles in the graph, return 0. Otherwise, return -1. */
int audit_project(Project *proj) {
    if (proj->len == 0)
        return 0;

    // Check for missing job dependencies
    struct table job_ids;
    init_table(&job_ids);

    project_node *curr = proj->jobs.head;
    while (curr) {
        insert(&job_ids, curr->job->id);
        curr = curr->next;
    }

    curr = proj->jobs.head;
    while (curr) {
        for (int i = 0; i < curr->len; i++)
            if (search(&job_ids, curr->deps[i]) == 0) {
                printf("missing dependency %d from %d\n", curr->deps[i], curr->job->id);
                return -1;
            }
        curr = curr->next;
    }
    free_table(&job_ids);

    // Check for cycle
    struct Path *cycle;
    curr = proj->jobs.head;
    while (curr) {
        cycle = get_cycle(proj, curr->job->id);
        if (cycle->len > 2) {
            print_path(cycle);
            free_path(cycle);
            return -1;
        }
        free_path(cycle);
        curr = curr->next;
    }

    return 0;
}

/* encode_project: Encodes the project into a JSON object. */
json_value *encode_project(Project *proj) {
    json_settings settings;
    settings.value_extra = json_builder_extra;

    // Add id
    json_value *obj = json_object_new(0);
    json_object_push(obj, "id", json_integer_new(proj->id));

    // Add jobs
    json_value *job, *jobs, *deps, *val;
    jobs = json_array_new(proj->len);
    project_node *curr = proj->jobs.head;
    while (curr) {
        val = json_object_new(0);
        job = encode_job(curr->job);
        json_object_push(val, "job", job);
        deps = json_array_new(curr->len);
        for (int i = 0; i < curr->len; i++) {
            json_array_push(deps, json_integer_new(curr->deps[i]));
        }
        json_object_push(val, "dependencies", deps);
        json_array_push(jobs, val);
        curr = curr->next;
    }
    json_object_push(obj, "jobs", jobs);
    return obj;
}

/* decode_project: Decodes the JSON object into a new project. */
Project *decode_project(json_value *obj) {
    json_value *id = json_get_value(obj, "id");
    Project *proj = create_project(id->u.integer);

    int j, len, *ids;
    json_value *job, *deps;
    json_value *jobs = json_get_value(obj, "jobs");
    for (unsigned int i = 0; i < jobs->u.array.length; i++) {
        job = json_get_value(jobs->u.array.values[i], "job");
        deps = json_get_value(jobs->u.array.values[i], "dependencies");
        len = deps->u.array.length;
        if ((ids = malloc(sizeof(int) * len)) == NULL) {
            perror("project: decode_project: malloc");
            exit(EXIT_FAILURE);
        }
        for (j = 0; j < len; j++) {
            ids[j] = deps->u.array.values[j]->u.integer;
        }
        add_job(proj, decode_job(job), ids, len);
        free(ids);
    }
    return proj;
}

/* project_status_map: Maps project status codes to its corresponding
    JSON value. */
json_value* project_status_map(int status) {
    json_value *val;
    switch (status) {
        case _PROJECT_READY:
            val = json_string_new("ready");
            break;
        case _PROJECT_NOT_READY:
            val = json_string_new("not_ready");
            break;
        case _PROJECT_RUNNING:
            val = json_string_new("running");
            break;
        case _PROJECT_COMPLETED:
            val = json_string_new("completed");
            break;
        case _PROJECT_INCOMPLETE:
            val = json_string_new("incomplete");
            break;
        default:
            val = json_null_new();
            break;
    }
    return val;
}
