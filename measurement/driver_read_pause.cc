#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>

int main(int argc, char *argv[])
{
    std::ifstream file(argv[1], std::ios::in);
    if (file.is_open() == false) {
        std::cerr << "file not opened.\n";
        exit(1);
    }
    std::string buf(100, '\0');
    while (file.read(&buf[0], 100)) {
        std::cout << buf << std::endl;
    }
    if (file.eof()) {
        std::cout << "final" << std::endl;
        buf.resize(file.gcount());
        std::cout << buf << std::endl;
    }

    int in;
    std::cin >> in;

    return 0;
}