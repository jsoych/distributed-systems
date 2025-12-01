#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "logger.h"
#include "pyoneer.h"

#define ROLE 1
#define LEVEL 2

int main(int argc, char *argv[]) {
    // Check argument count
    if (argc == 5) {
        fprintf(stderr, "main: Error: Expected 4 arguments, but received %d arguments\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    // Create logger
    Logger* logger = NULL;
    if (strcmp(argv[LEVEL], "--info") == 0)
        logger_create(LOGGER_INFO, NULL);
    else if (strcmp(argv[LEVEL], "--debug") == 0)
        logger_create(LOGGER_DEBUG, NULL);
    
    if (logger == NULL) {
        fprintf(stderr, "main: Error: Unable to create logger\n");
        exit(EXIT_FAILURE);
    }

    // Create pyoneer
    Pyoneer* pyoneer = NULL;
    int id = (int)strtol(argv[argc - 1], NULL, 10);

    if (strcmp(argv[ROLE], "worker") == 0)
        pyoneer = pyoneer_create(id, PYONEER_WORKER);
    else if (strcmp(argv[ROLE], "manager") == 0)
        pyoneer = pyoneer_create(id, PYONEER_MANAGER);

    if (pyoneer == NULL) {
        fprintf(stderr, "main: Error: Unable to create pyoneer\n");
        exit(EXIT_FAILURE);
    }

    // Create API server
    char* socket_path = getenv("PYONEER_SOCKET_PATH");
    if (socket_path == NULL) {
        fprintf(stderr, "main: Error: Missing PYONEER_SOCKET_PATH environment variable\n");
        pyoneer_destroy(pyoneer);
        logger_destroy(logger);
        exit(EXIT_FAILURE);
    }

    API* server = api_create(pyoneer, logger, socket_path);
    if (server == NULL) {
        fprintf(stderr, "main: Error: Unable to create server\n");
        pyoneer_destroy(pyoneer);
        logger_destroy(logger);
        exit(EXIT_FAILURE);
    }

    // Start server
    if (api_start(server) == -1) {
        fprintf(stderr, "main: Error: Unable to start server\n");
        api_destroy(server);
        pyoneer_destroy(pyoneer);
        logger_destroy(logger);
        exit(EXIT_FAILURE);
    }

    // Stop server
    if (api_stop(server) == -1) {
        fprintf(stderr, "main: Error: Unable to start server\n");
        api_destroy(server);
        pyoneer_destroy(pyoneer);
        logger_destroy(logger);
        exit(EXIT_FAILURE);
    }

    // Clean up
    api_destroy(server);
    pyoneer_destroy(pyoneer);
    logger_destroy(logger);
    exit(EXIT_SUCCESS);
}
