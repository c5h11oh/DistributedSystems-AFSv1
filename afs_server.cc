#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include <errno.h>

using namespace cs739;
using namespace grpc;

#define SERVER_DIR "/tmp/afs"

class AFSServiceImpl final : public cs739::AFS::Service {
public:
    explicit AFSServiceImpl() {}

    Status Stat(ServerContext* context, Filepath* request, StatContent* response) {
        std::cout << "Stat call received for filepath: " << request->filepath() << std::endl;

        struct stat stat_content;
        int res = stat(request->filepath().c_str(), &stat_content);

        if(res == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        } else {
            response->set_return_code(1);
            response->set_error_number();

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

    Status GetMeta(ServerContext* context, Filepath* request, Meta* response) {
        std::cout << "Meta call received for filepath: " << request->filepath() << std::endl;

        struct stat stat_content;
        int res = stat(request->filepath().c_str(), &stat_content);

        if(res == -1) {
            response->set_file_exists(-1);
        } else {
            response->set_file_exists(1);
            std::string ts(reinterpret_cast<char *>(&stat_content.st_mtim), sizeof(stat_content.st_mtim));
            response->set_timestamp(ts);
        }
        return Status::OK;
    }

    Status Mkdir(ServerContext* context, Filepath* request, Response* response) {

        int mode=1; // TODO 
        if(mkdir(getServerFilepath(request->filepath().c_str()), mode) == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        }
        response->set_return_code(1);
        return Status::OK;
    }

    Status Rmdir(ServerContext* context, Filepath* request, Response* response) {

        if(rmdir(getServerFilepath(request->filepath().c_str())) == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        }
        response->set_return_code(1);
        return Status::OK;
    }

    Status Unlink(ServerContext* context, Filepath* request, Response* response) {

        if(unlink(getServerFilepath(request->filepath().c_str())) == -1) {
            response->set_return_code(-1);
            response->set_error_number(errno);
        }
        response->set_return_code(1);
        return Status::OK;
    }

    Status Ls(ServerContext* context, Filepath* request, LsResult* response) {

        DIR *dir;
        dirent *entry;

        dir = opendir(getServerFilepath(request->filepath().c_str()));
        while(entry = readdir(dir)) {
            const char *d_name = entry->d_name;
            response->add_d_name()->str::string(d_name);
        }
        closedir(dir);
        return Status::OK;
    }

    const std::string getServerFilepath(std::string filepath) {
        return new std::string(std::string(SERVER_DIR)+filepath);
    }


};

void RunServer() {
    std::string server_address("0.0.0.0:53706");
    AFSServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server AFSServiceImpl listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    
    return 0;
}