#ifndef _LOGGER_H
#define _LOGGER_H

#include <pthread.h>

enum {
    LOGGER_INFO,
    LOGGER_DEBUG
};

typedef struct _logger {
    int level;
    char* format;
    pthread_mutex_t lock;
} Logger;

Logger* logger_create(int level, const char* format);
void logger_destroy(Logger* logger);

int logger_info(Logger* logger, char* message);
int logger_debug(Logger* logger, char* message);

#endif
