#include <stdio.h>
#include <string.h>
#include "unittest.h"


/* print_result: Prints the test result. */
void unittest_print_result(char *suite, unittest_result result, char *name, char *msg) {
    switch (result) {
        case unittest_success:
            printf("%s: success: %s\n", suite, name);
            break;
        case unittest_failure:
            printf("%s: failure: %s: %s\n", suite, name, msg);
            break;
    }
    return;
}
