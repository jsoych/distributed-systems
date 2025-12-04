/*
    The following tests unittest against trivial examples so that we can
    sure that unittest does indeed work as expected...
*/
#include <stdio.h>
#include <string.h>

#include "unittest.h"
#include "json.h"
#include "json-builder.h"

#define CMPINT(a, b) unittest_compare_int((a), (b))
#define CMPSTR(a, b) unittest_compare_string((a), (b))
#define CMPJSON(a, b) unittest_compare_json_value((a), (b))
#define TEST_CMP(actual, expected)  do {    \
        if ((actual) == (expected))         \
            printf("success\n");            \
        else                                \
            printf("failure\n");            \
    } while (0)

int main() {
    // Test compare integers
    printf("test compare integers\n");
    const int a = 1;
    const int b = 1;
    const int c = 2;
    TEST_CMP(CMPINT(&a, &b), 0);
    TEST_CMP(CMPINT(&a, NULL), 1);
    TEST_CMP(CMPINT(NULL, &b), 1);
    TEST_CMP(CMPINT(NULL, NULL), 0);
    TEST_CMP(CMPINT(&a, &c), 1);

    // Test compare string
    printf("\ntest compare string\n");
    const char* sa = "hello";
    const char* sb = "hello";
    const char* sc = "there";
    TEST_CMP(CMPSTR(a, b), 0);
    TEST_CMP(CMPSTR(a, NULL), 1);
    TEST_CMP(CMPSTR(NULL, b), 1);
    TEST_CMP(CMPSTR(NULL, NULL), 0);
    TEST_CMP(CMPSTR(sa, sc), 1);

    // Test compare JSON values
    printf("\njson compare JSON value\n");
    json_value* ja = json_null_new();
    json_value* jb = json_null_new();
    TEST_CMP(CMPJSON(ja, jb), 0);
    TEST_CMP(CMPJSON(a, NULL), 1);
    TEST_CMP(CMPJSON(NULL, b), 1);
    TEST_CMP(CMPJSON(NULL, NULL), 0);
    
    json_value* dbl = json_double_new(-42.0);
    TEST_CMP(CMPJSON(a, dbl), 1);
    
    json_value_free(ja);
    json_value_free(jb);
    json_value_free(dbl);
    return 0;
}