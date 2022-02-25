#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <time.h> // clock_gettime
#include <limits.h> // LONG_MAX
#include <unistd.h> // argument parsing
#include <stdlib.h> // malloc

#include <grpcpp/grpcpp.h>
#include "afs.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::Status;
using namespace cs739;

// #define BUFSIZE 2097152 // 2MB
#define BUFSIZE 12288 // 12KB
/* Protobuf ():
    bytes 	May contain any arbitrary sequence of bytes no longer than 2^32.
*/

class SendFileClient {
public:
    SendFileClient(std::shared_ptr<Channel> channel)
      : stub_(SendFile::NewStub(channel)) {}

    SendFileClient(std::shared_ptr<Channel> channel, char* filepath) 
     : stub_(SendFile::NewStub(channel)), filepath_(filepath) { }

    void send(std::ifstream& file) {
        ClientContext context;
        Reply reply;
        Data d;
        uint64_t count = 0;

        std::unique_ptr<ClientWriter<Data>> writer(
            stub_->Send(&context, &reply));

        // read file to buf and send it
        // https://en.cppreference.com/w/cpp/io/basic_istream/read
        file.seekg(0, std::ios::end);
        uint64_t filesize = file.tellg();
        file.seekg(0);
        
        // buffer to be filled with data and sent by gRPC
        uint64_t bufsize = (uint64_t)BUFSIZE;
        std::string buf(bufsize, '\0');
        
        // array to store all timestamp
        struct timespec tss[(10 + filesize / bufsize)];
        // timestamps = (struct timespec*)malloc(sizeof(struct timespec) * (10 + filesize / bufsize));
        ts_ptr = timestamps = tss;

        clock_gettime(CLOCK_MONOTONIC, ts_ptr++); // start
        while (file.read(&buf[0], bufsize)) {
            d.set_b(buf);
            if (!writer->Write(d))
                break;
            count += d.b().size();
            clock_gettime(CLOCK_MONOTONIC, ts_ptr++);
        }
        if (file.eof()) {
            buf.resize(file.gcount());
            d.set_b(buf);
            writer->Write(d);
            count += d.b().size();
        }
        writer->WritesDone();
        clock_gettime(CLOCK_MONOTONIC, ts_ptr++); // end

        Status status = writer->Finish();
        if (status.ok()) {
            // clock_gettime(CLOCK_MONOTONIC, &u);
            // std::cout << "Success sending data. stop timer and checksum...\n";
            std::stringstream md5cmd; 
            md5cmd << "md5sum " << filepath_;
            // std::string c_md5 = exec(md5cmd.str().c_str()).substr(0, 32);
            std::string s_md5 = reply.md5sum();
            // std::cout << "client: filesize=" << filesize << ", md5sum=" << c_md5 << "\n";
            // std::cout << "server: filesize=" << reply.r() << ", md5sum=" << s_md5 << "\n";
            // if (c_md5 != s_md5 || filesize != reply.r()) {
                // std::cout << "Fail to send correct data.\n";
                // goto cleanup;
            // }
            measure_bandwidth(bufsize, buf.size());
        } else {
            std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
            std::cout << "Fail to send file\n";
            goto cleanup;
        }
cleanup:
        // free(timestamps);
        return;
    }

private:
    std::unique_ptr<SendFile::Stub> stub_;
    char* filepath_;
    struct timespec* timestamps, * ts_ptr;
    void measure_bandwidth(int usual_size, int last_size) {
        std::cout << "Measure bandwidth:\n" <<
                     "size\tduration (ns)\tbandwidth (Bps)\n";
        int count = (ts_ptr - timestamps) - 1;
        int i;
        for (i = 0; i < count; ++i) {
            struct timespec* t = (timestamps + i), *u = (timestamps + i + 1);
            long dur_ns = (u->tv_nsec - t->tv_nsec) + (u->tv_sec - t->tv_sec) * 1000000000;
            double Bps = ((i == count - 1) ? last_size : usual_size) * 1.0 / (dur_ns / 1e9);
            // std::cout << ((i == count - 1) ? last_size : usual_size) << "\t" << dur_ns << "\t" << Bps << "\n";
            printf("%7d\t%12ld\t%11.1f\n", ((i == count - 1) ? last_size : usual_size), dur_ns, Bps);
        }
        
        int size_total = usual_size * (count - 1) + last_size;
        long dur_total_ns = (ts_ptr - 1)->tv_nsec - timestamps->tv_nsec + 
                            ((ts_ptr - 1)->tv_sec - timestamps->tv_sec) * 1000000000;
        double Bps_total = size_total * 1.0 / (dur_total_ns / 1e9);
        std::cout << "------------------ Total ------------------\n" <<
                     size_total << "\t" << dur_total_ns << "\t" << Bps_total << "\n";
    }
};

void print_usage(char* proc_name = nullptr) {
    std::cout << (proc_name ? proc_name : "send_file_client") << " [-s <server_ip:port>] <file_to_be_send>\n";
}

int main(int argc, char** argv) {
    std::string server_addr = "128.105.37.149:53706"; // royal-09=128.105.37.149:53706; royal-##=128.105.37.(140+##)

    int opt;
    while ((opt = getopt(argc, argv, "hs:")) != -1) {
        switch (opt) {
            case 's':
                server_addr = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case '?':
            default:
                std::cerr << argv[0] << ": invalid argument\n";
                print_usage(argv[0]);
                exit(1);
        }
    }
    if (optind >= argc) {
        std::cerr << argv[0] << ": specify file\n";
        print_usage(argv[0]);
        exit(1);
    }
    std::ifstream file(argv[optind], std::ios::binary);
    SendFileClient client(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()), argv[optind]);
    client.send(file);
    file.close();

    return 0;
}
