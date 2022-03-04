#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <string.h>

int main(int argc, char *argv[])
{
    int fd;
    char buf[256] = "foo bar";


    if (fd < 0) {
        std::cout << "open failed\n";
        exit(1);
    }

    write(fd, buf, strlen(buf));

    return 0;
}
