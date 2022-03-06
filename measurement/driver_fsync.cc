#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>

int main(int argc, char *argv[])
{
    int fd;

    fd = open(argv[1], O_RDWR, 00777);

    if (fd < 0) {
        perror("open");
        exit(1);
    }

    std::string buf(argv[2]);
    write(fd, buf.c_str(), buf.size());
    fsync(fd);

    int in;
    std::cout << "stop for a while\n";
    std::cin >> in;
    std::cout << "start running\n";

    close(fd);

    return 0;
}
