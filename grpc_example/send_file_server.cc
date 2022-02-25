#include <iostream>
#include <fstream>
#include <memory>

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include "send_file.grpc.pb.h"
#include "send_file.pb.h"

using namespace cs739;
using namespace grpc;


class SendFileImpl final : public cs739::SendFile::Service {
public:
    explicit SendFileImpl() {}
    
    Status Send(ServerContext* context, grpc::ServerReader<Data>* reader, Reply* out) {
        std::cout << "vv [Receiving file]\n";

        Data data;
        uint64_t size = 0;
        std::ofstream file("received_file", std::ios::out | std::ios::binary | std::ios::trunc);
        while (reader->Read(&data)) {
            size += data.b().size();
            // std::cout << "this data.b() size: " << data.b().size() << ", accumulated size: " << size << "\n";
            file << data.b();
        }
        file.close();
        out->set_r(size);
        // out->set_md5sum(exec("md5sum received_file").substr(0, 32));
        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:53706");
    SendFileImpl service;
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server SendFileImpl listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    
    return 0;
}
