#ifndef _UNITTEST_H
#define _UNITTEST_H

typedef enum {
    unittest_success,
    unittest_failure
} unittest_result;

void unittest_print_result(char *, unittest_result, char *, char *);

#endif
