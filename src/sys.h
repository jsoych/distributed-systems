#ifndef pyoneer_sys_h
#define pyoneer_sys_h

#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#define XSYSCALL(call)                                              \
    do {                                                            \
        if ((call) == -1) {                                         \
            fprintf(stderr, "%s:%d: %s failed: %s\n",               \
                    __FILE__, __LINE__, #call, strerror(errno));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while (0)


#define BIND(fd, addr, size)                                        \
    do {                                                            \
        int _ret = bind(fd, (struct sockaddr*)addr, size);          \
        if (_ret == -1) {                                           \
            fprintf(stderr, "%s:%d: %s failed: %s\n",               \
                    __FILE__, __LINE__, #call, strerror(errno));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while (0)

#endif
