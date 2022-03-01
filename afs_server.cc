// c++ library
#include <iostream>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
// c library
#include <sys/stat.h>
// grpc
#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include "afs.grpc.pb.h"
#include "afs.pb.h"

using namespace cs739;
using namespace grpc;

#define BUFSIZE 65500
std::string AFS_ROOT_DIR;

class AFSImpl final : public cs739::AFS::Service {
public:
    explicit AFSImpl() {}

    Status GetMeta(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Meta* response) {
        std::cout << "GetMeta: this is actually called!"
                  << " NOW YOU SHOULD IMPLEMENT IT LOL\n";
        response->set_file_exists(false);
        return Status::OK;
    }
    Status GetContent(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::grpc::ServerWriter< ::cs739::MetaContent>* writer){
        std::string filepath(AFS_ROOT_DIR + request->filepath());
        std::ifstream file(filepath, std::ios::in);
        MetaContent msg;
        struct stat sb;

        if (!file.is_open()) {
            msg.set_file_exists(false);
            writer->Write(msg);
            return Status::OK;
        }
        msg.set_file_exists(true);
        stat(filepath.c_str(), &sb);
        std::string ts(reinterpret_cast<char *>(&sb.st_mtim), sizeof(sb.st_mtim));
        msg.set_timestamp(ts);
        
        std::string buf(BUFSIZE, '\0');
        while (file.read(&buf[0], BUFSIZE)) {
            msg.set_b(buf);
            if (!writer->Write(msg))
                break;
        }
        if (file.eof()) {
            buf.resize(file.gcount());
            msg.set_b(buf);
            writer->Write(msg);
        }
        return Status::OK;
    }
    Status Ls(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::LsResult* response){
    }
    Status Write(::grpc::ServerContext* context, ::grpc::ServerReader< ::cs739::FilepathContent>* reader, ::cs739::Meta* response){
        FilepathContent msg;
        reader->Read(&msg);
        std::string filepath (AFS_ROOT_DIR + msg.filepath());
        std::ofstream file(filepath, std::ios::trunc | std::ios::out);
        if (!file.is_open()) {
            Status s(StatusCode::NOT_FOUND, "Cannot open file");
            return s;
        }
        file << msg.b();
        while (reader->Read(&msg)) {
            file << msg.b();
        }
        file.close();
        
        return Status::OK;
    }
    Status Stat(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::StatContent* response){
    }
    Status Unlink(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Response* response){
    }
    Status Rmdir(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Response* response){
    }
    Status Mkdir(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Response* response){
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:53706");
    AFSImpl service;
    
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server AFSImpl listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <AFS root directory>\n";
        exit(1);
    }
    struct stat sb;
    if (stat(argv[1], &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        std::cerr << argv[0] << ": AFS root directory does not exist.\n";
        exit(1);
    }
    AFS_ROOT_DIR = argv[1];
    if (AFS_ROOT_DIR.back() != '/') { AFS_ROOT_DIR += '/'; }
    RunServer();
    
    return 0;
}
