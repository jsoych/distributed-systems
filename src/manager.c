#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "manager.h"

#define BUFLEN 1024

// report types
typedef enum {
    missing_job_dependencies,
    cyclic_job_dependencies
} report_t;

// Report object
typedef struct _report {
    report_t type;
    union {
        int integer
    } u;
} Report;

/* init_queue: Initializes the queue. */
static void init_queue(queue *q) {
    q->head = NULL;
    q->tail = NULL;
    q->len = 0;
    return;
}

/* free_queue: Frees all queue resources. */
static void free_queue(queue *q) {
    if (q->len == 0) {
        return;
    }
    running_project_node *curr = q->head;
    while (curr->next) {
        curr = curr->next;
        free(curr->prev->deps);
        free(curr->prev);
    }
    free(curr->deps);
    free(curr);
    return;
}

/* add_node: Adds a node to the tail of the queue. */
static void add_node(queue *q, running_project_node *n) {
    n->next = NULL;
    if (q->len == 0) {
        n->prev = NULL;
        q->head = n;
        q->tail = n;
        q->len++;
        return;
    }
    n->prev = q->tail;
    q->tail = n;
    q->len++;
    return;
}

/* remove_node: Removes a node from the queue and returns it. */
static running_project_node *remove_node(queue *q, running_project_node *n) {
    q->len--;
    if (q->len == 0) {
        q->head = NULL;
        q->tail = NULL;
        return n;
    }
    if (n->prev)
        n->prev->next = n->next;
    else
        q->head = n->next;
    if (n->next)
        n->next->prev = n->prev;
    else
        q->tail = n->prev;
    n->next = NULL;
    n->prev = NULL;
    return n;
}

/* create_running_project: Creates a new running project. */
static RunningProject *create_running_project(Manager *man) {
    RunningProject *rproj;
    if ((rproj = malloc(sizeof(RunningProject))) == NULL) {
        perror("manager: create_running_project: malloc");
        exit(EXIT_FAILURE);
    }
    rproj->manager = man;
    rproj->project = NULL;
    init_queue(&rproj->not_ready_jobs);
    init_queue(&rproj->ready_jobs);
    init_queue(&rproj->running_jobs);
    init_queue(&rproj->completed_jobs);
    init_queue(&rproj->incomplete_jobs);
    return rproj;
}

/* bind_project: Binds the running project and project together, and
    changes the manager status to assigned. */
static void bind_project(RunningProject *rproj, Project *proj) {
    rproj->project = proj;
    running_project_node *node;
    for (project_node *pn = proj->jobs.head; pn; pn = pn->next) {
        if ((node = malloc(sizeof(running_project_node))) == NULL) {
            perror("manager: run_project: malloc");
            exit(EXIT_FAILURE);
        }
        node->job = pn->job;

        if ((node->deps = malloc(sizeof(running_project_node *) * pn->len)) == NULL) {
            perror("manager: run_project: malloc");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < pn->len; i++) {
            node->deps[i] = get_job(proj, pn->deps[i]);
        }
        node->len = pn->len;

        switch (pn->job->status) {
            case _JOB_NOT_READY:
                add_node(&rproj->not_ready_jobs, node);
                break;
            case _JOB_READY:
                add_node(&rproj->ready_jobs, node);
                break;
            case _JOB_RUNNING:
                add_node(&rproj->running_jobs, node);
                break;
            case _JOB_COMPLETED:
                add_node(&rproj->completed_jobs, node);
                break;
            case _JOB_INCOMPLETE:
                add_node(&rproj->incomplete_jobs, node);
                break;
        }
    }
    rproj->manager->status = _MANAGER_ASSIGNED;
    return;
}

/* unbind_project: Unbinds the running project and the project, changes the
    manager status to not assigned, and returns the project. */
static Project *unbind_project(RunningProject *rproj) {
    rproj->manager->status = _MANAGER_NOT_ASSIGNED;
    free_queue(&rproj->not_ready_jobs);
    free_queue(&rproj->ready_jobs);
    free_queue(&rproj->running_jobs);
    free_queue(&rproj->completed_jobs);
    free_queue(&rproj->incomplete_jobs);
    return rproj->project;
}

/* free_running_project: Frees all resources allocated to the running
    project. Remark: */
static void free_running_project(RunningProject *rproj) {
    if (get_status(rproj->manager) == _MANAGER_NOT_ASSIGNED) {
        free(rproj);
        return;
    }
    free_project(unbind(rproj));
    free(rproj);
    return;
}

/* create_manager: Creates a new manager. */
Manager *create_manager(int id) {
    Manager *man;
    if ((man = malloc(sizeof(Manager))) == NULL) {
        perror("manager: malloc");
        exit(EXIT_SUCCESS);
    }
    man->id = id;
    man->status = _MANAGER_NOT_ASSIGNED;
    man->crew = create_crew();
    man->running_project = create_running_project(man);
    if (sem_init(&man->lock, 0, 1) == -1) {
        perror("manager: create_manager: sem_init");
        exit(EXIT_FAILURE);
    }
    return man;
}

/* get_status: Gets the manager status. */
int get_status(Manager *man) {
    int status;
    if (sem_wait(&man->lock) == -1) {
        perror("manager: get_status: sem_wait");
        exit(EXIT_FAILURE);
    }
    status = man->status;
    if (sem_post(&man->lock) == -1) {
        perror("manager: get_status: sem_post");
        exit(EXIT_FAILURE);
    }
    return status;
}

/* get_project_status: Gets and returns the project. If the manager
    is not assigned a project, then get_project_status returns -1. */
int get_project_status(Manager *man) {
    int status;
    if (sem_wait(&man->lock) == -1) {
        perror("manager: get_project_status: sem_wait");
        exit(EXIT_FAILURE);
    }

    if (man->status == _MANAGER_NOT_ASSIGNED)
        status = -1;
    else
        status = man->running_project->project->status;
    
    if (sem_post(&man->lock) == -1) {
        perror("manager: get_project_status: sem_post");
        exit(EXIT_FAILURE);
    }
    return status;
}

/* sync_project: Synchronizes the project job statuses with the crew job
    job statuses. */
static void sync_project(Manager *man) {
    Project *proj = man->running_project->project;
    
    int old_errno;
    if ((old_errno = pthread_mutex_lock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: update_project_status: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    
    Job *job;
    crew_node *curr;
    for (int i = 0; i < _CREW_MAXLEN; i++) {
        curr = &man->crew->workers[i % _CREW_MAXLEN].head;
        while (curr) {
            if (curr->worker->status == worker_not_assigned) {
                curr = curr->next;
                continue;
            }

            job = get_job(proj, curr->worker->job.id);
            switch (curr->worker->job.status) {
                case job_running:
                    job->status = _JOB_RUNNING;
                    break;
                case job_completed:
                    job->status = _JOB_COMPLETED;
                    break;
                case job_incomplete:
                    job->status = _JOB_INCOMPLETE;
                    break;
            }
            curr = curr->next;
        }
    }

    if((old_errno = pthread_mutex_unlock(&man->crew->lock)) != 0) {
        fprintf(stderr, "manager: update_project_status: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    return;
}

/* status_handler: */
static void *status_handler(void *arg) {
    Manager *man = (Manager *) arg;
    if (man->status == _MANAGER_NOT_WORKING)
        return NULL;
    if (man->running_project->project->status == _PROJECT_RUNNING)
        man->running_project->project->status = _PROJECT_INCOMPLETE;
    man->status = _MANAGER_NOT_WORKING;
    return NULL;
}

/* stop_workers_handler: Stops all working workers. */
static void *stop_workers_handler(void *arg) {
    Manager *man = (Manager *) arg;
    json_value *cmd;
    cmd = json_object_new(0);
    json_object_push(cmd, "command", json_string_new("stop"));
    broadcast_command(man->crew, cmd);
    json_value_free(cmd);
    return NULL;
}

/* get_job_status: Get the status of the job. */
static int get_job_status(running_project_node *node) {
    if (node->job->status != _JOB_NOT_READY) {
        return node->job->status;
    }

    // Check the status of each dependency
    for (int i = 0; i < node->len; i++) {
        if (node->deps[i]->status != _JOB_COMPLETED)
            return _JOB_NOT_READY;
    }
    return (node->job->status = _JOB_READY);
}

// deque node
static struct deque_node {
    int val;
    struct deque_node *next;
    struct deque_node *prev;
};

// deque structure
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
static struct Path *audit_project(Project *proj) {
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

/* project_thread: Runs the project. */
static void *project_thread(void *args) {
    // Unpack args
    RunningProject *rproj = (RunningProject *) args;
    Manager *man = rproj->manager;
    Project *proj = rproj->project;
    proj->status = _PROJECT_RUNNING;

    // Add cleanup handlers
    pthread_cleanup_push(status_handler, man);
    pthread_cleanup_push(stop_workers_handler, man);

    // Run project
    running_project_node *curr;
    json_value *obj;
    while (sleep(1) == 0) {
        // Synchronzie project with crew
        sync_project(man);

        // Schedule not ready jobs
        for (curr = rproj->not_ready_jobs.head; curr; curr = curr->next) {
            switch (get_job_status(curr)) {
                case _JOB_READY:
                    add_node(&rproj->ready_jobs, remove_node(&rproj->not_ready_jobs, curr));
                    break;
                default:
                    fprintf(stderr, "manager: project_thread: Warning: Schedule is in inconsitent state\n");
                    break;
            }
        }
        
        // Assign ready jobs
        for (curr = rproj->ready_jobs.head; curr; curr = curr->next) {
            switch (get_job_status(curr)) {
                case _JOB_READY:
                    obj = encode_job(curr->job);
                    assign_job(man->crew, obj);
                    json_builder_free(obj);
                    break;
                case _JOB_RUNNING:
                    add_node(&rproj->running_jobs, remove_node(&rproj->ready_jobs, curr));
                    break;
                default:
                    fprintf(stderr, "manager: project_thread: Warning: Schedule is in inconsitent state\n");
                    break;
            }
        }

        // Check running jobs
        for (curr = rproj->running_jobs.head; curr; curr = curr->next) {
            switch (get_job_status(curr)) {
                case _JOB_COMPLETED:
                    add_node(&rproj->completed_jobs, remove_node(&rproj->incomplete_jobs, curr));
                    unassign_worker(man->crew, curr->worker_id);
                    break;
                case _JOB_INCOMPLETE:
                    add_node(&rproj->incomplete_jobs, remove_node(&rproj->running_jobs, curr));
                    unassign_worker(man->crew, curr->worker_id);
                    break;
                default:
                    fprintf(stderr, "manager: project_thread: Warning: Schedule is in inconsitent state\n");
                    break;             
            }
        }

        // Update project status
        if (rproj->incomplete_jobs.len > 0) {
            proj->status = _PROJECT_INCOMPLETE;
            break;
        } else if (rproj->completed_jobs.len == proj->len) {
            proj->status = _PROJECT_COMPLETED;
            break;
        }
        
        // Sanity check
        if ((rproj->not_ready_jobs.len + rproj->ready_jobs.len + rproj->running_jobs.len + rproj->completed_jobs.len + rproj->incomplete_jobs.len) != proj->len) {
            fprintf(stderr, "manager: project_thread: Error: Missing node\n");
            break;
        }
    }
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    return NULL;
}

/* run_project: Runs the project and returns 0. If the manager is working
    or the project is not ready, it returns -1. */
int run_project(Manager *man, Project *proj) {
    int ret = 0;
    if (sem_wait(&man->lock) == -1) {
        perror("manager: run_project: sem_wait");
        exit(EXIT_FAILURE);
    }

    if (man->status == _MANAGER_WORKING) {
        ret = -1;
        goto unlock;
    }

    if (proj->status != _PROJECT_READY) {
        ret = -1;
        goto unlock;
    }

    // Audit project
    if (audit_project(proj) == -1) {
        fprintf(stderr, "manager: run_project: audit_project: Project has circular dependencies\n");
        proj->status = _PROJECT_NOT_READY;
        ret = -1;
        goto unlock;
    }

    // Assign project to the manager
    if (man->status == _MANAGER_ASSIGNED)
        free_project(unbind_project(man->running_project));
    bind_project(man->running_project, proj);
    
    int old_errno;
    if ((old_errno = pthread_create(&man->running_project->tid, NULL, project_thread, man->running_project)) != 0) {
        fprintf(stderr, "manager: run_project: pthread_create: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    man->status = _MANAGER_WORKING;

    unlock:
    if (sem_post(&man->lock) == -1) {
        perror("manager: run_project: sem_post");
        exit(EXIT_FAILURE);
    }
    return ret;
}

/* start: Starts the running project and returns the project status. */
int start(Manager *man) {
    if (sem_wait(&man->lock) == -1) {
        perror("manager: start: sem_wait");
        exit(EXIT_FAILURE);
    }

    if (man->status == _MANAGER_NOT_ASSIGNED || man->status == _MANAGER_WORKING)
        goto unlock;
    
    if (man->running_project->project->status != _PROJECT_READY)
        goto unlock;

    int old_errno;
    if ((old_errno = pthread_create(&man->running_project->tid, NULL, project_thread, man->running_project)) != 0) {
        fprintf(stderr, "manager: start: %s\n", strerror(old_errno));
        exit(EXIT_FAILURE);
    }
    
    unlock:
    if (sem_post(&man->lock) == -1) {
        perror("manager: start: sem_post");
        exit(EXIT_FAILURE);
    }
    return get_project_status(man);
}

/* stop: Stops the running project and returns the project status. If
    the running project is NULL, stop returns -1. */
int stop(Manager *man) {
    if (sem_wait(&man->lock) == -1) {
        perror("manager: stop: sem_wait");
        exit(EXIT_FAILURE);
    }

    if (man->status != _MANAGER_WORKING)
        goto unlock;
    
    int old_errno;
    if ((old_errno = pthread_cancel(man->running_project->tid)) != 0) {
        fprintf(stderr, "manager: stop: %s\n", strerror(old_errno));
    }

    unlock:
    if (sem_post(&man->lock) == -1) {
        perror("manager: stop: sem_post");
        exit(EXIT_FAILURE);
    }
    return get_project_status(man);
}

/* free_manager: Frees the memory allocated to the manager. */
void free_manager(Manager *man) {
    free_crew(man->crew);
    free_running_project(man->running_project);
    if (sem_destroy(&man->lock) == -1) {
        perror("manager: free_manager: sem_destroy");
        exit(EXIT_FAILURE);
    }
    free(man);
    return;
}
