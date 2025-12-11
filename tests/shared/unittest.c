#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "unittest.h"

#define CAPACITY 8

/* unittest_create: Creates a new unittest. */
Unittest* unittest_create(const char* name) {
    int len = strlen(name);
    Unittest* ut = malloc(sizeof(Unittest) + (len + 1)*sizeof(char));
    if (ut == NULL) {
        perror("unittest_create: malloc");
        return NULL;
    }

    strcpy(ut->name, name);
    ut->name[len] = '\0';
    ut->size = 0;
    ut->capacity = CAPACITY;
    ut->tests = malloc(ut->capacity*sizeof(unittest_test));
    if (ut->tests == NULL) {
        perror("unittest_create: malloc");
        free(ut);
        return NULL;
    }

    ut->cases = malloc(ut->capacity*sizeof(unittest_case*));
    if (ut->cases == NULL) {
        perror("unittest_create: malloc");
        free(ut->tests);
        free(ut);
        return NULL;
    }

    for (int i = 0; i < ut->capacity; i++) {
        ut->tests[i] = NULL;
        ut->cases[i] = NULL;
    }

    return ut;
}

/* unittest_destroy: Destroys the unittest and frees all its resources. */
void unittest_destroy(Unittest* ut) {
    if (ut == NULL) return;
    free(ut->tests);
    for (int i = 0; i < ut->size; i++) {
        unittest_case* args = ut->cases[i];
        switch (args->type) {
            case CASE_NONE:
            case CASE_INT:
            case CASE_NUM:
                break;
            case CASE_STR:
                free(args->as.string);
                break;
            case CASE_JSON:
                json_value_free(args->as.json);
                break;
            case CASE_TASK:
                task_destroy(args->as.task);
                break;
            case CASE_JOB:
                job_destroy(args->as.job);
                break;
        }

        free(args);
    }

    free(ut);
}

/* unittest_add: Adds a new test case to the unittest. */
int unittest_add(Unittest* ut, const char* name, unittest_test tc, 
    case_t type, void* expected) {
    if (ut->size == ut->capacity) {
        int capacity = 2*ut->capacity;
        unittest_test* tests = malloc(capacity*sizeof(unittest_test));
        if (tests == NULL) {
            perror("unittest_add: malloc");
            return -1;
        }

        unittest_case** cases = malloc(capacity*sizeof(unittest_case*));
        if (cases == NULL) {
            perror("unittest_add: malloc");
            free(tests);
            return -1;
        }

        memcpy(tests, ut->tests, ut->size*sizeof(unittest_test));
        free(ut->tests);
        ut->tests = tests;

        memcpy(cases, ut->cases, ut->size*sizeof(unittest_case*));
        free(ut->cases);
        ut->cases = cases;

        for (int i = ut->size; i < capacity; i++) {
            ut->tests[i] = NULL;
            ut->cases[i] = NULL;
        }

        ut->capacity = capacity;
    }

    int len = strlen(name);
    unittest_case* args = malloc(sizeof(unittest_case) + (len + 1)*sizeof(char));
    if (args == NULL) {
        perror("unittest_add: malloc");
        return -1;
    }

    args->type = type;

    switch (type) {
        case CASE_NONE:
            break;
        case CASE_INT:
            args->as.integer = *((int*)expected);
            break;
        case CASE_NUM:
            args->as.number = *((double*)expected);
            break;
        case CASE_JSON:
            args->as.json = (json_value*)expected;
            break;
        case CASE_TASK:
            args->as.task = (Task*)expected;
            break;
        case CASE_JOB:
            args->as.job = (Job*)expected;
            break;
        default:
            fprintf(stderr, "unittest_add: Error: Unknown type (%d)\n", type);
            free(args);
            return -1;
    }
    
    strcpy(args->name, name);
    args->name[len] = '\0';

    ut->tests[ut->size] = tc;
    ut->cases[ut->size++] = args;
    return 0;
}

// Ring buffer
struct ring_buffer {
    int start;
    int size;
    char buf[BUFSIZE];
};

// ring_buf_init: Initializes ring buffer.
void ring_buf_init(struct ring_buffer* rb) {
    rb->start = 0;
    rb->size = 0;
    memset(rb->buf, 0, BUFSIZE);
}

// ring_buf_write: Writes to ring buffer.
void ring_buf_write(struct ring_buffer* rb, const char* data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        size_t end = (rb->start + rb->size) % BUFSIZE;
        rb->buf[end] = data[i];

        if (rb->size < BUFSIZE) {
            rb->size++;
        } else {
            // Buffer full, overwrite oldest
            rb->start = (rb->start + 1) % BUFSIZE;
        }
    }
}

// ring_buf_read: Reads from ring buffer.
void ring_buf_read(struct ring_buffer* rb, char* out) {
    for (int i = 0; i < rb->size; i++) {
        out[i] = rb->buf[(rb->start + i) % BUFSIZE];
    }
    out[rb->size] = '\0';
}

// Errors list
struct node {
    const char* name;
    int result;
    char buf[BUFSIZE];
    struct node* next;
};

struct list {
    int len;
    struct node* head;
    struct node* tail;
};

// list_init: Initializes the list.
void list_init(struct list* lst) {
    lst->len = 0;
    lst->head = NULL;
    lst->tail = NULL;
}

// list_free: Frees the list.
void list_free(struct list* lst) {
    struct node* curr = lst->head;
    while (curr) {
        struct node* prev = curr;
        curr = curr->next;
        free(prev);
    }
    
    lst->head = NULL;
    lst->tail = NULL;
    lst->len = 0;
}

// list_add: Adds a result and its buffer to the list.
void list_add(struct list* lst, struct node* n) {
    n->next = NULL;
    if (lst->len++ == 0) {
        lst->head = n;
        lst->tail = n;
        return;
    }

    lst->tail->next = n;
    lst->tail = n;
}

// list_print: Prints the buffer of each node.
void list_print(struct list* lst) {
    printf("\n----------------------------------------------------------------------\n");
    struct node* curr = lst->head;
    while (curr) {
        const char* result_type;
        switch (curr->result) {
            case UNITTEST_FAILURE:
                result_type = "FAIL";
                break;
            case UNITTEST_ERROR:
                result_type = "ERROR";
                break;
            default:
                result_type = "UNKNOWN";
                break;
        }
        
        printf("%s) %s\n", curr->name, result_type);
        printf("%s\n", curr->buf);
        printf("\n----------------------------------------------------------------------\n");

        curr = curr->next;
    }
}

/* unittest_run: Runs all the test cases and stores its results. */
int unittest_run(Unittest* ut) {
    printf("----- Running unit test %s -----\n\n", ut->name);
    #define SUCCESS 0
    #define FAILURE 1
    #define ERROR 2
    #define UNKNOWN 3
    int summary[4] = {0};

    struct list lst;
    list_init(&lst);

    char buf[BUFSIZE];
    for (int i = 0; i < ut->size; i++) {
        int test_pipe[2];
        pipe(test_pipe);

        int saved_out = dup(STDOUT_FILENO);
        int saved_err = dup(STDERR_FILENO);

        // Redirect stdout/stderr to pipe
        dup2(test_pipe[1], STDOUT_FILENO);
        dup2(test_pipe[1], STDERR_FILENO);
        close(test_pipe[1]);

        dprintf(saved_out, "(%s) ...", ut->cases[i]->name);

        // Run test case
        int result = ut->tests[i](ut->cases[i]);

        switch (result) {
            case UNITTEST_SUCCESS:
                dprintf(saved_out, "ok\n");
                summary[SUCCESS]++;
                break;
            case UNITTEST_FAILURE:
                dprintf(saved_out, "FAIL\n");
                summary[FAILURE]++;
                break;
            case UNITTEST_ERROR:
                dprintf(saved_out, "ERROR\n");
                summary[ERROR]++;
                break;
            default:
                dprintf(saved_out, "UNKNOWN\n");
                summary[UNKNOWN]++;
        }

        fflush(stdout);
        fflush(stderr);

        // Restore stdout/stderr
        dup2(saved_out, STDOUT_FILENO);
        dup2(saved_err, STDERR_FILENO);
        close(saved_out);
        close(saved_err);


        struct ring_buffer rb;
        ring_buf_init(&rb);

        // Read all bufferred out to the ring buffer
        int n = read(test_pipe[0], buf, BUFSIZE);
        while (n > 0) {
            ring_buf_write(&rb, buf, n);
            n = read(test_pipe[0], buf, BUFSIZE);
        }

        if (result != UNITTEST_SUCCESS && rb.size > 0) {
            struct node* node = malloc(sizeof(struct node));
            node->name = ut->cases[i]->name;
            node->result = result;
            ring_buf_read(&rb, node->buf);
            list_add(&lst, node);
        }

        close(test_pipe[0]);
    }
    
    // Print summary
    printf("\n----------------------------------------------------------------------\n");
    printf("Ran %d tests\n\n", ut->size);

    if (summary[FAILURE] > 0 || summary[ERROR] > 0 || summary[UNKNOWN] > 0) {
        printf("FAILED (");
        if (summary[FAILURE]) printf("failures=%d", summary[FAILURE]);
        if (summary[FAILURE] && summary[ERROR]) printf(", ");
        if (summary[ERROR]) printf("errors=%d", summary[ERROR]);
        if ((summary[FAILURE] + summary[ERROR]) && summary[UNKNOWN]) printf(", ");
        if (summary[UNKNOWN]) printf("unknown=%d", summary[UNKNOWN]);
        printf(")\n");

        list_print(&lst);
        list_free(&lst);
    } else {
        printf("OK\n");
        list_free(&lst);
    }

    return 0;
}

/* compare_task: Compares tasks. */
int unittest_compare_task(const Task* a, const Task* b) {
    if (a == b) return 0;
    if (a == NULL || b == NULL) return 1;
    if (a->status == b->status && strcmp(a->name, a->name) == 0)
        return 0;
    return 1;
}

int unittest_compare_job(const Job* a, const Job* b) {
    if (a == b) return 0;
    if (a == NULL || b == NULL) return 1;
    if (a->id != b->id || a->status != b->status ||
        a->size != b->size) return 1;
    
    // Compare tasks
    job_node* cura = a->head;
    job_node* curb = b->head;
    while (cura && curb) {
        if (unittest_compare_task(cura->task, curb->task)) return 1;
        cura = cura->next;
        curb = curb->next;
    }

    if (cura != curb) return -1;
    return 0;
}

#undef CAPACITY
