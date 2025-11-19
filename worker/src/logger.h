#ifndef pyoneer_logger_h
#define pyoneer_logger_h

typedef enum {
    LOGGER_INFO,
    LOGGER_DEBUG
} logger_level_t;

typedef struct Logger Logger;

Logger* logger_create(logger_level_t level, const char* file_path);
void logger_destroy(Logger* logger);

int logger_info(Logger* logger, char* format, ...);
int logger_debug(Logger* logger, char* format, ...);

#endif
