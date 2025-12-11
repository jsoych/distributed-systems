#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "suite.h"

Suite* suite_create(int capacity) {
    if (capacity <= 1) return NULL;
    Suite* suite = malloc(sizeof(Suite));
    if (suite == NULL) return NULL;
    suite->tests = malloc(capacity*sizeof(Unittest*));
    if (suite->tests == NULL) {
        free(suite);
        return NULL;
    }
    suite->size = 0;
    suite->capacity = capacity;
    return suite;
}

void suite_destroy(Suite* suite) {
    if (suite == NULL) return;
    for (int i = 0; i < suite->size; i++)
        unittest_destroy(suite->tests[i]);
    free(suite->tests);
    free(suite);
}

int suite_add(Suite* suite, Unittest* ut) {
    if (suite->size == suite->capacity) {
        printf("suite_add: Error: Suite at capacity (%d)\n", suite->capacity);
        return -1;
    }
    suite->tests[suite->size++] = ut;
    return 0;
}

int suite_run(Suite* suite) {
    // Run each unittest
    for (int i = 0; i < suite->size; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Unit test process
            if (unittest_run(suite->tests[i]) == -1) exit(UNITTEST_RUN_ERROR);
            
            fflush(stdout);
            fflush(stderr);
            exit(EXIT_SUCCESS);
        }

        int status;
        waitpid(pid, &status, 0);

        fflush(stdout);
        fflush(stderr);

        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code == UNITTEST_RUN_ERROR) {
                printf("suite_run: Error: Unittest %s exited with error code (%d)\n",
                    suite->tests[i]->name, code);
            }
        } else if (WIFSIGNALED(status)) {
            int signo = WTERMSIG(status);
            printf("suite_run: Error: %s\n", strsignal(signo));
        } else {
            printf("hmm...");
            return -1;
        }
    }

    return 0;
}
