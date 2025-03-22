#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"

/* create_list: Creates a new list. */
static List *create_list() {
    List *l;
    if (l = malloc(sizeof(List)) == -1) {
        perror("project: create_list: malloc");
        exit(EXIT_FAILURE);
    }
    l->head = NULL;
    l->tail = NULL;
    return l;
}

/* free_list: Frees the memory allocated to the list. */
static void free_list(List *l) {
    if (l->head == NULL) {
        free(l);
        return;
    }
    
    // Free nodes
    ProjectNode *curr;
    while (curr) {
        curr = curr->next;
        free(curr->prev);
    }
    free(curr);

    free(l);
    return;
}

// create_project: Creates a new project.
Project *create_project(int id) {
    Project *proj;
    if ((proj = malloc(sizeof(Project))) == NULL) {
        perror("project: create_project: malloc");
        exit(EXIT_FAILURE);
    }

    proj->id = id;
    proj->status = _PROJECT_NOT_READY;
    proj->len = 0;
    proj->jobs_list = create_list();
    for (int i = 0; i < MAXLEN; i++) {
        proj->jobs_table[i] = NULL;
    }

    return proj;
}

// free_project: Frees the memory allocated to the project.
void free_project(Project *proj) {
    free_list(proj->jobs_list);
    free(proj);
}

// add_job: Adds the job to the list with the ids of its dependencies.
void add_job(Project *proj, Job *job, int ids[], int len) {
    ProjectNode *new_node, *ent;
    if ((new_node = malloc(sizeof(ProjectNode))) == -1) {
        perror("project: add_job: malloc");
        exit(EXIT_FAILURE);
    }

    int *deps;
    if ((deps = malloc(sizeof(int))) == -1) {
        perror("project: add_job: malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < len; i++) {
        deps[i] = ids[i];
    }

    new_node->job = job;
    new_node->deps = deps;
    new_node->len = len;
    

    // Add new_node to jobs table
    ent = proj->jobs_table[job->id % MAXLEN];
    if (ent == NULL)
        proj->jobs_table[job->id % MAXLEN] = new_node;
    proj->len++;
}

/* get_node: Gets and returns the node by its job id. Otherwise, NULL. */
static ProjectNode *get_node(Project *proj, int id) {
    ProjectNode *ent = proj->jobs_table[id % MAXLEN];
    while (ent) {
        if (ent->job->id == id)
            break;
        ent = ent->next_ent;
    }
    return ent;
}

/* remove_job: Removes the job from the project by its job id. */
void remove_job(Project *proj, int id) {
    ProjectNode *node = get_node(proj,id);
    if (node == NULL)
        return;

    ProjectNode *ent = proj->jobs_table[id % MAXLEN];
    if (proj->len-- == 1) {
        proj->jobs_list->head = NULL;
        proj->jobs_list->tail = NULL;
        proj->jobs_table[id % MAXLEN] = NULL;
        free(node);
        return;
    }
    
    // Update jobs_table
    if (ent == node)
        proj->jobs_table[id % MAXLEN] = ent->next_ent;
    
    while (ent->next_ent) {
        if (ent->next_ent == node)
            ent->next_ent = node->next_ent;
        ent = ent->next_ent;
    }

    // Update jobs_list
    if (node == proj->jobs_list->head) {
        node->next->prev = NULL;
        proj->jobs_list->head = node->next;
    } else if (node == proj->jobs_list->tail) {
        node->prev->next = NULL;
        proj->jobs_list->tail = node->prev;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }
    
    free(node);
    return;
}

// deque node
static struct node {
    int val;
    struct node *next;
    struct node *prev;
};

// deque
static struct deque {
    int len;
    struct node *head;
    struct node *tail;
};

// create_queue: Creates a new deque.
static struct deque *create_deque() {
    struct deque *d;
    if ((d = malloc(sizeof(struct deque))) == -1) {
        perror("project: create_dequeue: malloc");
        exit(EXIT_FAILURE);
    }

    d->len = 0;
    d->head = NULL;
    d->tail = NULL;

    return d;
}

// free_queue: Frees all of the deque memory.
static void free_deque(struct deque *d) {
    struct node *curr, *prev;
    if (d->len == 0) {
        free(d);
        return;
    }

    curr = d->head->next;
    prev = d->head;
    while (curr) {
        free(prev);
        prev = curr;
        curr = curr->next;
    }
    free(prev);
}

// push: Adds a new node to the head of the deque.
static void push(struct deque *d, int v) {
    struct node *new_node;
    if ((new_node = malloc(sizeof(struct node))) == -1) {
        perror("project: push: malloc");
        exit(EXIT_FAILURE);
    }

    new_node->val = v;
    new_node->next = d->head;
    d->head->prev = new_node;
    d->head = new_node;
    d->len++;
}

/* dequeue: Removes the last node from the deque and returns its value.
    Otherwise, it returns -1. */
static int dequeue(struct deque *d) {
    if (d->len == 0)
        return -1;
    
    int ret = d->tail->val;
    if (d->len-- == 1) {
        free(d->tail);
        d->head = NULL;
        d->tail = NULL;
        return ret;
    }

    d->tail = d->tail->prev;
    free(d->tail->next);
    d->tail->next = NULL;
    return ret;
}

// append: Adds a new node to the tail of the deque.
static void append(struct deque *d, int v) {
    struct node *new_node;
    if ((new_node = malloc(sizeof(struct node))) == -1) {
        perror("project: queue: malloc");
        exit(EXIT_SUCCESS);
    }
    new_node->val = v;
    new_node->next = NULL;
    new_node->prev = d->tail;
    d->tail->next = new_node;
    d->tail = new_node;
    d->len++;
}

// pop: Removes the first node from the deque and returns its value.
static int pop(struct deque *d) {
    if (d->len == 0)
        return -1;

    int ret = d->head->val;
    if (d->len-- == 1) {
        free(d->head);
        d->head = NULL;
        d->tail = NULL;
        return ret;
    }

    d->head = d->head->next;
    free(d->head->prev);
    d->head->prev = NULL;
    return ret;
}

/* find: Searches for the value and returns 0, if the value is in the
    dequeue. Otherwise, it returns -1. */
static int find(struct deque *d, int v) {
    struct node *curr = d->head;
    while (curr) {
        if (curr->val == v)
            return 1;
        curr = curr->next;
    }
    return 0;
}

// hash table
static struct table {
    int len;
    struct deque *buckets[MAXLEN];
};

// create_table: Creates a new hash table.
static struct table *create_table() {
    struct table *t;
    if ((t = malloc(sizeof(struct table))) == -1) {
        perror("project: create_table: malloc");
        exit(EXIT_FAILURE);
    }

    t->len = 0;
    for (int i = 0; i < MAXLEN; i++)
        t->buckets[i] = create_deque();
    
    return t;
}

// free_table: Frees the memory allocated to the table.
static void free_table(struct table *t) {
    for (int i = 0; i < MAXLEN; i++)
        free_dequeue(t->buckets[i]);
    free(t);
}

// insert: Inserts a new entry to the hash table.
static void insert(struct table *t, int v) {
    append(t->buckets[v % MAXLEN],v);
}

/* delete: Deletes the value from the table and returns the number of
    entries deleted. */
static int delete(struct table *t, int v) {
    struct deque *b = t->buckets[v % MAXLEN];
    int count = 0;
    if (b->len == 0)
        return count;
    
    struct node *curr = b->head;
    while (curr) {
        if (curr->val != v) {
            curr = curr->next;
            continue;
        }

        // Decrement lengths
        t->len--;
        b->len--;

        // Remove node from deque
        if (curr->prev)
            curr->prev->next = curr->next;
        else
            b->head = curr->next;
        
        if (curr->next)
            curr->next->prev = curr->prev;
        else
            b->tail = curr->prev;
        
        curr = curr->next;
        free(curr);
        count++;
    }

    return count;
}

/* search: Searches the hash table for the value and returns 1, 
    if successful, and 0 otherwise. */
static int search(struct table *t, int v) {
    return find(t->buckets[v % MAXLEN], v);
}

// path node
static struct pnode {
    int val;
    struct pnode *next;
    struct pnode *prev;
    struct pnode *next_ent;
};

// path object
static struct path {
    int len;
    struct pnode *head;
    struct pnode *tail;
    struct pnode *table[MAXLEN];
};

// create_path: Creates a new path object.
static struct path *create_path() {
    struct path *p;
    if ((p = malloc(sizeof(struct path))) == -1) {
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
static void free_path(struct path *p) {
    if (p->len == 0) {
        free(p);
        return;
    }

    struct pnode *curr = p->head;
    while (curr->next) {
        curr = curr->next;
        free(curr->prev);
    }
    free(curr);
    free(p);
}

// add_vxt: Adds the value to the end of the path.
static void add_vxt(struct path *p, int v) {
    struct pnode *pn;
    if ((pn = malloc(sizeof(struct pnode))) == -1) {
        perror("project: add: malloc");
        exit(EXIT_FAILURE);
    }

    pn->val = v;
    pn->prev = p->tail;
    p->tail->next = pn;
    p->tail = pn;

    pn->next_ent = p->table[v % MAXLEN];
    p->table[v % MAXLEN] = pn;
    p->len++;
}


/* dequeue_vxt: Removes the last vertex from the path and returns its
    value. If the path is empty, dequeue_vxt returns -1. */
static int dequeue_vxt(struct path *p) {
    int ret = -1;
    if (p->len == 0)
        return ret;
    ret = p->tail->val;

    struct pnode *vxt = p->tail;
    struct pnode *ent = p->table[vxt->val % MAXLEN];
    p->len--;
    if (ent == vxt) {
        p->table[vxt->val % MAXLEN] = ent->next_ent;
        free(vxt);
        return ret;
    }

    while (ent->next_ent) {
        if (ent->next_ent = vxt) {
            ent->next_ent = vxt->next_ent;
            break;
        }
        ent = ent->next_ent;
    }
    free(vxt);
    return ret;
}

/* pod_vxt: Removes the first vertex from the path and returns its value.
    If the path is empty, pop_vxt returns -1. */
static int pop_vxt(struct path *p) {
    int val = -1;
    if (p->len == 0)
        return val;
    val = p->head->val; 

    struct pnode *vxt = p->head;
    struct pnode *ent = p->table[val % MAXLEN];
    p->len--;
    if (ent == vxt) {
        p->table[val % MAXLEN] = ent->next_ent;
        free(vxt);
        return val;
    }

    while (ent->next_ent) {
        if (ent->next_ent = vxt) {
            ent->next_ent = vxt->next_ent;
            break;
        }
        ent = ent->next_ent;
    }
    free(vxt);
    return val;
}

/* get_vxt: Gets the vertex from the path with its id and returns a
    pointer to the vertex. If there is no vertex, get_vxt returns NULL. */
static struct pnode *get_vxt(struct path *p, int id) {
    struct pnode *pn = p->table[id % MAXLEN];
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
static int in_path(struct path *p, int v) {
    struct pnode *ent = p->table[v % MAXLEN];
    if (ent == NULL)
        return 0;
    
    while (ent) {
        if (ent->val == v)
            return 1;
        ent = ent->next_ent;
    }
    return 0;
}

/* get_cycle: Get cycle in the project starting from id. */
static void get_cycles(struct project *proj, int id) {
    ProjectNode *node = get_node(proj,id);
    if (node == NULL)
        return;
    
    // Create data structures
    struct path *path = create_path();
    struct table *visited = create_table();
    struct deque *dq = create_deque();

    // Start BFS
    push(dq,node->job->id);
    while (dq->len > 0) {
        insert(visited, id = pop(dq));

        // Check for cycle
        if (in_path(path,id)) {
            // Trim path and create cycle
            while (path->head->val != id)
                if (pop_vxt(path) == -1) {
                    fprintf(stderr,"project: get_cycle: Path is empty");
                    exit(EXIT_FAILURE);
                };
            add_vxt(path,id);
            break;
        }
        add_vxt(path,id);

        node = get_node(proj,id);
        if (node->len == 0) {
            dequeue_vxt(path);
            continue;
        }

        // Check for cycles and add node neightbours to dq
        for (int i = 0; i < node->len; i++) {
            id = node->deps[i];

            // Check for cycle
            if (in_path(path,id)) {
                // Trim path and create cycle
                while (path->head->val !=  id)
                    if (pop_vxt(path) == -1) {
                        fprintf(stderr,"project: get_cycle: Path is empty");
                        exit(EXIT_FAILURE);
                    }
                add_vxt(path,id);
                break;
            }

            if (search(visited,id) == 0)
                push(dq,id);
        }
    }
    
    // Free data structures
    free_deque(dq);
    free_table(visited);

    // Check if path is a cycle and return it
    if ((path->len >= 2) && (path->head->val == path->tail->val))
        return path;
    free_path(path);
    return NULL;
}

/* audit_project: Checks the integrity of the project dependency graph. If
    there is no cycles in the graph, return 0. Otherwise, return -1. */
int audit_project(Project *proj) {
    if (proj->len == 0) {
        fprintf(stderr,"project: audit_project: Warning the project is empty\n");
        return 0;
    }
    // Check for missing jobs

    // Check for cycles

    return 0;
}
