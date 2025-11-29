#include "logger.h"
#include "sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdarg.h>
#include <string.h>

struct Logger {
    FILE* file;
    logger_level_t level;
    pthread_mutex_t lock;
};

// Factory
Logger* logger_create(logger_level_t level, const char* file_path) {
    Logger* logger = malloc(sizeof(Logger));
    PTR_CHECK(logger, "malloc Logger failed");

    logger->level = level;
    if (file_path)
        SYSCALL_LOG(NULL, (logger->file = fopen(file_path, "a")) == NULL ? -1 : 0);
    else
        logger->file = NULL;

    PTHREAD_CHECK(pthread_mutex_init(&logger->lock, NULL));

    return logger;
}

// Destroy
void logger_destroy(Logger* logger) {
    if (logger->file)
        fclose(logger->file);

    PTHREAD_CHECK(pthread_mutex_destroy(&logger->lock));
    free(logger);
}

// Internal logging helper
static int logger_log(Logger* logger, logger_level_t msg_level, const char* fmt, va_list args) {
    PTHREAD_CHECK(pthread_mutex_lock(&logger->lock));

    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");

    if (msg_level == LOGGER_DEBUG)
        vfprintf(stderr, fmt, args), fprintf(stderr, "\n");

    if (logger->file)
        vfprintf(logger->file, fmt, args), fprintf(logger->file, "\n");

    PTHREAD_CHECK(pthread_mutex_unlock(&logger->lock));
    return 0;
}

// Public methods
int logger_info(Logger* logger, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = logger_log(logger, LOGGER_INFO, fmt, args);
    va_end(args);
    return res;
}

int logger_debug(Logger* logger, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int res = logger_log(logger, LOGGER_DEBUG, fmt, args);
    va_end(args);
    return res;
}
