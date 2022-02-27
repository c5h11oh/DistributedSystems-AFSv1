#define FUSE_USE_VERSION 26
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unistd.h>
#include <stdlib.h> // malloc
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <grpcpp/grpcpp.h>
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

std::unique_ptr<AFS::Stub> stub_;

struct afs_data_t {
    grpc::ClientContext context;
    std::string cache_path; // must contain forward slash at the end.
    std::unique_ptr<AFS::Stub> stub_;
    std::unordered_map<std::string, std::string> last_modified; // path to st_mtim
};

std::string fullpath(const char* rel_path) {
    return AFS_DATA->cache_path + std::string(rel_path);
}

int afs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    Filepath filepath;
    LsResult ls_result;
    filepath.set_filepath(fullpath(path));
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
        // check 
        StatContent stat_content;
        AFS_DATA->stub_->Stat(&(AFS_DATA->context), filepath, &stat_content);

        if ( stat_content.st_mtim() == AFS_DATA->last_modified[path_str]) {
            // can use cache
            fi->fh = open((AFS_DATA->cache_path + path_str).c_str(), fi->flags);
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
        std::ofstream ofile(AFS_DATA->cache_path + path_str,
            std::ios::binary | std::ios::out | std::ios::trunc);
        // TODO: check failure
        ofile << msg.b();
        while (reader->Read(&msg)) {
            ofile << msg.b();
        }
    }
    else {
        AFS_DATA->last_modified[path_str] = "local_generated_file";
        close(creat((AFS_DATA->cache_path + path_str).c_str(), 00777));
    }
    fi->fh = open((AFS_DATA->cache_path + path_str).c_str(), fi->flags);
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
    std::ifstream file(fullpath(path), std::ios::in);
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

// int afs_mknod(const char *path, mode_t mode, dev_t dev)
// {
//     int retstat;
//     char fpath[PATH_MAX];
    
//     log_msg("\nbb_mknod(path=\"%s\", mode=0%3o, dev=%lld)\n",
// 	  path, mode, dev);
//     bb_fullpath(fpath, path);
    
//     // On Linux this could just be 'mknod(path, mode, dev)' but this
//     // tries to be be more portable by honoring the quote in the Linux
//     // mknod man page stating the only portable use of mknod() is to
//     // make a fifo, but saying it should never actually be used for
//     // that.
//     if (S_ISREG(mode)) {
// 	retstat = log_syscall("open", open(fpath, O_CREAT | O_EXCL | O_WRONLY, mode), 0);
// 	if (retstat >= 0)
// 	    retstat = log_syscall("close", close(retstat), 0);
//     } else
// 	if (S_ISFIFO(mode))
// 	    retstat = log_syscall("mkfifo", mkfifo(fpath, mode), 0);
// 	else
// 	    retstat = log_syscall("mknod", mknod(fpath, mode, dev), 0);
    
//     return retstat;
// }

// int afs_unlink(const char *path)
// {
//     char fpath[PATH_MAX];
    
//     log_msg("bb_unlink(path=\"%s\")\n",
// 	    path);
//     bb_fullpath(fpath, path);

//     return log_syscall("unlink", unlink(fpath), 0);
// }

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

// int afs_mkdir(const char *path, mode_t mode)
// {
//     char fpath[PATH_MAX];
    
//     log_msg("\nbb_mkdir(path=\"%s\", mode=0%3o)\n",
// 	    path, mode);
//     bb_fullpath(fpath, path);

//     return log_syscall("mkdir", mkdir(fpath, mode), 0);
// }

// int afs_rmdir(const char *path)
// {
//     char fpath[PATH_MAX];
    
//     log_msg("bb_rmdir(path=\"%s\")\n",
// 	    path);
//     bb_fullpath(fpath, path);

//     return log_syscall("rmdir", rmdir(fpath), 0);
// }

// int afs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
// {
//     int retstat = 0;
    
//     log_msg("\nbb_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
// 	    path, buf, size, offset, fi);
//     // no need to get fpath on this one, since I work from fi->fh not the path
//     log_fi(fi);

//     return log_syscall("pread", pread(fi->fh, buf, size, offset), 0);
// }

// int afs_write(const char *path, const char *buf, size_t size, off_t offset,
// 	     struct fuse_file_info *fi)
// {
//     int retstat = 0;
    
//     log_msg("\nbb_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
// 	    path, buf, size, offset, fi
// 	    );
//     // no need to get fpath on this one, since I work from fi->fh not the path
//     log_fi(fi);

//     return log_syscall("pwrite", pwrite(fi->fh, buf, size, offset), 0);
// }

static struct fuse_operations afs_oper = {
	.readdir	= afs_readdir,
	.open		= afs_open,
	.release	= afs_release,
	.mknod		= afs_mknod,
	.unlink		= afs_unlink,
	.getattr	= afs_getattr,
	.mkdir		= afs_mkdir,
	.rmdir		= afs_rmdir,
	.read		= afs_read,
	.write		= afs_write, 
};

int main(int argc, char *argv[])
{
    std::string server_addr = "127.0.0.1:53706";
    ;
    afs_data_t* afs_data = new afs_data_t;
	afs_data->stub_ = AFS::NewStub(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()));
    afs_data->cache_path = argv[1]; // CHECK!
    if (afs_data->cache_path.back() != '/') { afs_data->cache_path += '/'; }

    return fuse_main(argc, argv, &afs_oper, afs_data);
}
