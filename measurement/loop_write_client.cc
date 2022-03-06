#include <iostream>
#include <fstream>
#include <numeric>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <cstdlib>
#include <ctime>
using namespace std;

int main() {
    srand(time(NULL));
    struct timespec t, u;
    struct stat sb;
    if (stat("/users/c5h11oh/mount/writefile", &sb) != 0) {
        cerr << "file not open. exit.\n";
        exit(1);
    }
    int i = 1;
    char buf[65536];
    memset(buf, 157, 65536);
    while (i) {
        // fstream f;
        clock_gettime(CLOCK_MONOTONIC, &t);
        int fd = open("/users/c5h11oh/mount/writefile", O_WRONLY);
        // f.open("/users/c5h11oh/mount/writefile", ios::out | ios::app);
        for (int i = 0; i < 10; ++i) {
            lseek(fd, rand() % 209600000, 0);
            if (write(fd, buf, 65536) != 65536) {
                std::cerr << "does not write full length\n.";
                exit(1);
            }
        }

        close(fd);
        clock_gettime(CLOCK_MONOTONIC, &u);
        long dur = (u.tv_sec - t.tv_sec) * 1000000000 + (u.tv_nsec - t.tv_nsec); 
        cout << dur << endl;    

        i--;   
    }
    
    return 0;
}