#define FUSE_USE_VERSION 26
// standard c++
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
// standard c
#include <unistd.h>
#include <stdlib.h> // malloc
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
// openssl library
#include <openssl/sha.h>
// grpc library
#include <grpcpp/grpcpp.h>
// program's header
#include "afs.grpc.pb.h"
#include "afs.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::Status;
using namespace cs739;

#define PATH_MAX 4096
#define BUFSIZE 65500
#define AFS_DATA ((struct afs_data_t *) fuse_get_context()->private_data)
#define LOCAL_CREAT_FILE "locally_generated_file"

std::unique_ptr<AFS::Stub> stub_;

struct afs_data_t {
    grpc::ClientContext context;
    std::string cache_root; // must contain forward slash at the end.
    std::unique_ptr<AFS::Stub> stub_;
    std::unordered_map<std::string, std::string> last_modified; // path to st_mtim
};

std::string cachepath(const char* rel_path) {
    // local cached filename is SHA-256 hash of the path
    // referencing https://stackoverflow.com/questions/2262386/generate-sha256-with-openssl-and-c
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, rel_path, strlen(rel_path));
    SHA256_Final(hash, &sha256);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << ((int)hash[i]);
    }
    std::cout << "hashed hex string is " << ss.str() << std::endl; // debug
    return AFS_DATA->cache_root + ss.str();
}

int afs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    Filepath filepath;
    LsResult ls_result;
    filepath.set_filepath(std::string(path));
    AFS_DATA->stub_->Ls(&(AFS_DATA->context), filepath, &ls_result);
    for (int i = 0; i < ls_result.d_name_size(); ++i) {
        if (filler(buf, ls_result.d_name(i).c_str(), NULL, 0) != 0) {
            return -ENOMEM;
        }
    }
    return 0;
}

int afs_open(const char *path, struct fuse_file_info *fi)
{
    std::string path_str(path);
    Filepath filepath;
    filepath.set_filepath(path_str);
    char fpath[PATH_MAX];
    if (AFS_DATA->last_modified.count(path_str) == 1) {
        // cache exists 
        // check if it is locally created file
        if (AFS_DATA->last_modified[path_str] == LOCAL_CREAT_FILE) {
            fi->fh = open(cachepath(path).c_str(), fi->flags);
            if (fi->fh < 0) {
                return -errno;
            }
            return 0;
        }
        
        StatContent stat_content;
        AFS_DATA->stub_->Stat(&(AFS_DATA->context), filepath, &stat_content);

        if ( stat_content.st_mtim() == AFS_DATA->last_modified[path_str]) {
            // can use cache
            fi->fh = open(cachepath(path).c_str(), fi->flags);
            if (fi->fh < 0) {
                return -errno;
            }
            return 0;
        }
        else {
            // can't use cache. delete last_modified entry
            AFS_DATA->last_modified.erase(path_str);
        }
    }
    MetaContent msg;
    std::unique_ptr<ClientReader<MetaContent>> reader(
        AFS_DATA->stub_->GetContent(&(AFS_DATA->context), filepath));
    reader->Read(&msg);
    if (msg.file_exists()) {
        AFS_DATA->last_modified[path_str] = msg.timestamp();
        // open file with O_TRUNC
        std::ofstream ofile(AFS_DATA->cache_root + path_str,
            std::ios::binary | std::ios::out | std::ios::trunc);
        // TODO: check failure
        ofile << msg.b();
        while (reader->Read(&msg)) {
            ofile << msg.b();
        }
    }
    else {
        AFS_DATA->last_modified[path_str] = LOCAL_CREAT_FILE;
        close(creat(cachepath(path).c_str(), 00777));
    }
    fi->fh = open(cachepath(path).c_str(), fi->flags);
    if (fi->fh < 0)
        return -errno;
    return 0;    
}

int afs_release(const char *path, struct fuse_file_info *fi)
{
    close(fi->fh);
    Meta meta;
    FilepathContent content;
    content.set_filepath(std::string(path));

    auto writer = AFS_DATA->stub_->Write(&(AFS_DATA->context), &meta);
    std::ifstream file(cachepath(path), std::ios::in);
    std::string buf(BUFSIZE, '\0');
    while (file.read(&buf[0], BUFSIZE)) {
        content.set_b(buf);
        if (writer->Write(content))
            break;
    }
    if (file.eof()) {
        buf.resize(file.gcount());
        content.set_b(buf);
        writer->Write(content);
    }
    writer->WritesDone();
    file.close();

    Status status = writer->Finish();
    if (status.ok()) {
        AFS_DATA->last_modified[std::string(path)] = meta.timestamp();
    } else {
        // TODO: handle error
        std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
    }
    return 0;
}

int afs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int rc;

    // creat a local file
    rc = open(cachepath(path).c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
    if (rc >= 0) {
        rc = close(rc);
    }

    if (rc < 0)
        return -errno;

    Meta meta;
    FilepathContent content;
    content.set_filepath(std::string(path));
    auto writer = AFS_DATA->stub_->Write(&(AFS_DATA->context), &meta);
    std::string buf;
    content.set_b(buf);
    writer->Write(content);
    writer->WritesDone();

    Status status = writer->Finish();
    if (status.ok()) {
        AFS_DATA->last_modified[std::string(path)] = meta.timestamp();
    } else {
        // TODO: handle error
        std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
    }

    return 0;
}

int afs_unlink(const char *path)
{
    std::string path_str(path);
    Filepath fp;
    Response res;
    fp.set_filepath(path_str);

    // remove cached file
    unlink(cachepath(path).c_str());
    AFS_DATA->last_modified.erase(std::string(path));    
    AFS_DATA->stub_->Unlink(&(AFS_DATA->context), fp, &res);
    if (res.return_code() < 0) {
        return -res.error_number();
    }
    return res.return_code();
}

int afs_getattr(const char *path, struct stat *stbuf)
{
    Filepath request;
    StatContent response;
    request.set_filepath(std::string(path));
	AFS_DATA->stub_->Stat(&(AFS_DATA->context), request, &response);
    
    int res = 0;

	if (response.return_code() < 0) {
        return -response.error_number();
    }

    stbuf->st_atim = *((timespec *)response.st_atim().c_str());
    stbuf->st_mtim = *((timespec *)response.st_mtim().c_str());
    stbuf->st_ctim = *((timespec *)response.st_ctim().c_str());
    stbuf->st_dev = (dev_t)response.st_dev();
    stbuf->st_ino = (ino_t)response.st_ino();
    stbuf->st_mode = (mode_t)response.st_mode();
    stbuf->st_nlink = (nlink_t)response.st_nlink();
    stbuf->st_uid = (uid_t)response.st_uid();
    stbuf->st_gid = (gid_t)response.st_gid();
    stbuf->st_rdev = (dev_t)response.st_rdev();
    stbuf->st_size = (off_t)response.st_size();
    stbuf->st_blksize = (blksize_t)response.st_blksize();
    stbuf->st_blocks = (blkcnt_t)response.st_blocks();

    return response.return_code();
}

int afs_mkdir(const char *path, mode_t mode)
{
    std::string path_str(path);
    Filepath fp;
    Response res;
    fp.set_filepath(path_str);

    AFS_DATA->stub_->Mkdir(&(AFS_DATA->context), fp, &res);
    if (res.return_code() < 0) {
        return -res.error_number();
    }
    return res.return_code();
}

int afs_rmdir(const char *path)
{
    std::string path_str(path);
    Filepath fp;
    Response res;
    fp.set_filepath(path_str);

    AFS_DATA->stub_->Rmdir(&(AFS_DATA->context), fp, &res);
    if (res.return_code() < 0) {
        return -res.error_number();
    }
    return res.return_code();
}

int afs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    ssize_t rc = pread(fi->fh, buf, size, offset);
    if (rc < 0)
        return -errno;
    return rc;
}

int afs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    ssize_t rc = pwrite(fi->fh, buf, size, offset);
    if (rc < 0)
        return -errno;
    return rc;
}

static struct fuse_operations afs_oper;

int main(int argc, char *argv[])
{
    std::string server_addr = "127.0.0.1:53706";
    ;
    afs_data_t* afs_data = new afs_data_t;
	afs_data->stub_ = AFS::NewStub(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()));
    afs_data->cache_root = argv[1]; // CHECK!
    if (afs_data->cache_root.back() != '/') { afs_data->cache_root += '/'; }

    afs_oper.getattr	= afs_getattr;
    afs_oper.mknod		= afs_mknod;
    afs_oper.mkdir		= afs_mkdir;
    afs_oper.unlink		= afs_unlink;
    afs_oper.rmdir		= afs_rmdir;
    afs_oper.open		= afs_open;
    afs_oper.read		= afs_read;
    afs_oper.write		= afs_write;
    afs_oper.release	= afs_release;
    afs_oper.readdir	= afs_readdir;
    return fuse_main(argc, argv, &afs_oper, afs_data);
}
