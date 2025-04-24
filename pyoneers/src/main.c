#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "json.h"
#include "pyoneer.h"

#define BUFFSIZE 4096
char buff[BUFFSIZE];

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "main: incorrect argument count\n");
        exit(EXIT_FAILURE);
    }

    // open config file
    int fd;
    if ((fd = open(argv[1], O_RDONLY)) == -1) {
        perror("main: open");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // read config file
    int n;
    if ((n = read(fd, buff, BUFFSIZE)) == -1) {
        perror("main: read");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (close(fd) == -1) {
        perror("main: close");
        exit(EXIT_FAILURE);
    }

    // parse config file
    json_value *config;
    if ((config = json_parse(buff, BUFFSIZE)) == NULL) {
        fprintf(stderr, "main: unable to parse config file\n");
        json_value_free(config);
        exit(EXIT_FAILURE);
    }

    // link binaries

    // create controller (tool)

    // create resource (blueprint)

    // close config file

    // create socket

    // listen

    // serve requests

    // exit

}
