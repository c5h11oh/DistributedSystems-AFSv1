#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include <unistd.h>
 
int main() {
    std::stringstream ss;
    ss << "HHello";
    std::string str;
    ss >> str;
    std::cout << ss.tellg() << std::endl;
    ss >> str;
    std::cout << ss.tellg() << std::endl;
}

// int main() {
//     // prepare file for next snippet
//     std::fstream file("/users/c5h11oh/fuse-test/mount_point/opened_file", std::ios::in | std::ios::out | std::ios::trunc );
//     if (file.is_open() == false) {
//         std::cout << "file not opened\n";
//         exit(1);
//     }
//     file << "abs";
//     file.flush();
//     while (true) {
//         sleep(10);
//     }
//     return 0;
// }

// int main()
// {
//     // read() is often used for binary I/O
//     std::string bin = {'\x12', '\x12', '\x12', '\x12'};
//     std::istringstream raw(bin);
//     std::uint32_t n;
//     if(raw.read(reinterpret_cast<char*>(&n), sizeof n))
//         std::cout << std::hex << std::showbase << n << '\n';
 
//     // prepare file for next snippet
//     std::ofstream("test.txt", std::ios::binary) << "abcd1\nabcd2\nabcd3";
 
//     // read entire file into string
//     if(std::ifstream is{"test.txt", std::ios::binary | std::ios::ate}) {
//         auto size = is.tellg();
//         std::string str(size, '\0'); // construct string to stream size
//         is.seekg(0);
//         if(is.read(&str[0], size))
//             std::cout << str << '\n';
//     }
// }

// #include <sys/stat.h>
// #include <sys/types.h>
// #include <openssl/sha.h>
// #include <string>
// #include <memory>
// #include <string.h>
// #include <sstream>
// #include <iomanip>

// static std::string hash_str(const char *src) {
//     auto digest = std::make_unique<unsigned char[]>(SHA256_DIGEST_LENGTH);
//     SHA256(reinterpret_cast<const unsigned char *>(src), strlen(src),
//            digest.get());
//     std::stringstream ss;
//     for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
//         ss << std::hex << std::setw(2) << std::setfill('0')
//            << static_cast<int>(digest[i]);
//     }
//     return ss.str();
// }

// int main() {
//     mkdir("hellodir/", 00777);
//     return 0;
// }