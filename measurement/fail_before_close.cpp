#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

#define _512M (512 * 1024 * 1024)

using namespace std;
int main() {
    srand(time(NULL));
    fstream f;
    f.open("/users/c5h11oh/mount/fail_before_close.txt", fstream::out | fstream::app);
    if (!f.is_open()) {
        cerr << "file not opened. exit.\n";
        exit(1);
    }
    // for (int i = 0; i < _512M; ++i)
    //     f << (char)(rand() % 256);
    f << "Hello, world!\n";
    f.flush();
    int n;
    cin >> n;

    f.close();
    return 0;


}