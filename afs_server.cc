// c++ library
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <memory>

// c library
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

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

class AFSServiceImpl final : public cs739::AFS::Service {
public:
    explicit AFSServiceImpl() {}

    Status GetMeta(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Meta* response) {
        // std::cout << "Meta call received for filepath: " << request->filepath() << std::endl;

        // struct stat stat_content;
        // int res = stat(request->filepath().c_str(), &stat_content);

        // if(res == -1) {
        //     response->set_file_exists(false);
        // } else {
        //     response->set_file_exists(true);
        //     std::string ts(reinterpret_cast<char *>(&stat_content.st_mtim), sizeof(stat_content.st_mtim));
        //     response->set_timestamp(ts);
        // }
        // return Status::OK;
        std::cout << "GetMeta: this is actually called!"
                  << " NOW YOU SHOULD IMPLEMENT IT LOL\n";
        response->set_file_exists(false);
        return Status::OK;
    }
    Status GetContent(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::grpc::ServerWriter< ::cs739::MetaContent>* writer){
        log("GetContent");
        
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
        log("Ls");
        
        DIR *dir;
        dirent *entry;

        dir = opendir(getServerFilepath(request->filepath()).c_str());
        while(entry = readdir(dir)) {
            const char *d_name = entry->d_name;
            std::string* s = response->add_d_name();
            s->assign(entry->d_name);
        }
        closedir(dir);
        return Status::OK;
    }
    Status Write(::grpc::ServerContext* context, ::grpc::ServerReader< ::cs739::FilepathContent>* reader, ::cs739::Meta* response){
        log("Write");
        
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
        log("Stat");

        std::cout << "Stat call received for filepath: " << request->filepath() << std::endl;

        struct stat stat_content;
        int res = stat(getServerFilepath(request->filepath()).c_str(), &stat_content);

        if(res == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        } else {
            response->set_return_code(0);

            response->set_st_dev(stat_content.st_dev);
            response->set_st_ino(stat_content.st_ino);
            response->set_st_mode(stat_content.st_mode);
            response->set_st_nlink(stat_content.st_nlink);
            response->set_st_uid(stat_content.st_uid);
            response->set_st_gid(stat_content.st_gid);
            response->set_st_rdev(stat_content.st_rdev);
            response->set_st_size(stat_content.st_size);
            response->set_st_blksize(stat_content.st_blksize);
            response->set_st_blocks(stat_content.st_blocks);

            std::string st_atim_ts(reinterpret_cast<char *>(&stat_content.st_atim), sizeof(stat_content.st_atim));
            response->set_st_atim(st_atim_ts);
            std::string st_mtim_ts(reinterpret_cast<char *>(&stat_content.st_mtim), sizeof(stat_content.st_mtim));
            response->set_st_mtim(st_mtim_ts);
            std::string st_ctim_ts(reinterpret_cast<char *>(&stat_content.st_ctim), sizeof(stat_content.st_ctim));
            response->set_st_ctim(st_ctim_ts);
        }
        return Status::OK;
    }
    Status Unlink(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Response* response){
        log("Unlink");
        response->set_return_code(0);
        if(unlink(getServerFilepath(request->filepath()).c_str()) == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        }
        return Status::OK;
    }
    Status Rmdir(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Response* response){
        log("Rmdir");
        response->set_return_code(0);
        if(rmdir(getServerFilepath(request->filepath()).c_str()) == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        }
        return Status::OK;
    }
    Status Mkdir(::grpc::ServerContext* context, const ::cs739::Filepath* request, ::cs739::Response* response){
        log("Mkdir");
        response->set_return_code(0);
        if(mkdir(getServerFilepath(request->filepath()).c_str(), 00777) == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        }
        return Status::OK;
    }
private:
    const std::string getServerFilepath(std::string filepath) {
        return (AFS_ROOT_DIR + filepath);
    }
    const std::string getServerFilepath(const char* filepath) {
        return getServerFilepath(std::string(filepath));
    }
    void log(char* msg) {
        std::cout << "[log] " << msg << std::endl;
    }
};

void RunServer(std::string port) {
    std::string server_address(std::string("0.0.0.0:") + port);
    AFSServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server AFSServiceImpl listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    if (argc < 2) {
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
    std::string port;
    port = (argc > 2) ? argv[2] : "53706";
    RunServer(port);
    
    return 0;
}