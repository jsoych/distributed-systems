#ifndef pyoneer_common_h
#define pyoneer_common_h

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define XMALLOC(size)                                               \
    ({                                                              \
        void* _ptr = malloc(size);                                  \
        if (!_ptr) {                                                \
            fprintf(stderr, "%s:%d (%s): malloc(%zu) failed\n",     \
                    __FILE__, __LINE__, __func__, (size_t)(size));  \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
        _ptr;                                                       \
    })

#endif
