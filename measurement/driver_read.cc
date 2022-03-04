#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include "timespec_lib.h"

int main(int argc, char *argv[])
{
    int fd;
    struct timespec start, end, open_diff, close_diff;
    long open_ns, close_ns;

    clock_gettime(CLOCK_MONOTONIC, &start);
    fd = open(argv[1], O_RDONLY);
    clock_gettime(CLOCK_MONOTONIC, &end);
    sub_timespec(start, end, &open_diff);
    open_ns = open_diff.tv_sec * NS_PER_SECOND + open_diff.tv_nsec;

    if (fd < 0) {
        perror("open");
        exit(1);
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    close(fd);
    clock_gettime(CLOCK_MONOTONIC, &end);
    sub_timespec(start, end, &close_diff);
    close_ns = close_diff.tv_sec * NS_PER_SECOND + close_diff.tv_nsec;

    std::cout << "open,close\n";
    std::cout << open_ns << "," << close_ns << std::endl;

    return 0;
}