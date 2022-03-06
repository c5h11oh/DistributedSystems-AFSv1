#define FUSE_USE_VERSION 26
// standard c++
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <chrono>
// standard c
#include <unistd.h>
#include <stdlib.h> // malloc
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
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
using grpc::StatusCode;
using namespace cs739;

#define PATH_MAX 4096
#define BUFSIZE 65500
#define AFS_DATA ((struct afs_data_t *) fuse_get_context()->private_data)
#define LOCAL_CREAT_FILE "locally_generated_file"
#define DEFAULT_SERVER "127.0.0.1:53706"
#define LAST_MODIFIED_FILE "last_modified"

std::string hashpath(const char* rel_path) {
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
    return ss.str();
}

class is_dirty_t {
public:
    is_dirty_t(std::string& cache_root) : cache_root(cache_root) {
        snapshot_frequency = 10; // after how many logs, make a snapshot and clear the log
        
        // read the snapshot and log to get the most recent state
        std::vector<std::string> filenames{std::string(cache_root).append("/is_dirty_snapshot.txt"), std::string(cache_root).append("/is_dirty_log.txt")};
        for (std::string& filename : filenames) {
            std::cout << filename << std::endl;
            if (std::ifstream is{filename, std::ios::in | std::ios::ate}) {
                auto size = is.tellg();
                is.seekg(0);
                std::string buf(100, '\0');
                if (size > 0)
                    while (is.tellg() != -1 && is.tellg() != size) {
                        std::getline(is, buf);
                        if (buf.size() != 66) {
                            std::cout << "offset=" << is.tellg() << std::endl;
                            assert(false);
                        }
                        switch (buf.back()) {
                            case '2':
                                table.erase(buf.substr(0, 64));
                                break;
                            case '1':
                                table[buf.substr(0, 64)] = true;
                                break;
                            case '0':
                                table[buf.substr(0, 64)] = false;
                                break;
                            default:
                                std::cerr << "error in reading is_dirty snapshot and log\n";

                        }
                    }
            }
        }
        log.open((filenames[1]), std::ios::out | std::ios::app);
        std::cout << "[log] log is open? " << (log.is_open() ? "true" : "false") << std::endl;
    }

    bool get(const char* filename) {
        return get(std::string(filename));
    }
    
    bool get(std::string filename) {
        // return true if filename exists in table and is_dirty is true
        std::string hashed_fn = hashpath(filename.c_str());
        return table.count(hashed_fn) && table[hashed_fn];
    }

    void set(const char* filename, bool state) {
        set(std::string(filename), state);
    }

    void set(std::string filename, bool state) {
        std::string hashed_fn = hashpath(filename.c_str());
        // if nothing is changed, do nothing
        if (table.count(hashed_fn) && table[hashed_fn] == state) { return; }

        // otherwise, set it in memory and flush log
        table[hashed_fn] = state;
        log << hashed_fn << " " << (state ? '1' : '0') << std::endl;
        log.flush();

        // if there are a lot of logs, make snapshot
        if ((counter = (counter + 1) % snapshot_frequency) == 0)
            do_snapshot();
    }

    void erase(const char* filename) {
        erase(std::string(filename));
    }

    void erase(std::string filename) {
        std::string hashed_fn = hashpath(filename.c_str());
        if (table.count(hashed_fn)) {
            table.erase(hashed_fn);
            log << hashed_fn << " 2" << std::endl; // 2 means erase the entry
            log.flush();
        }
    }
private:
    bool do_snapshot() {
        std::cout << "[log] enter snapshot" << std::endl;
        std::string old_name(std::string(cache_root) + "/is_dirty_snapshot.txt.tmp");
        std::string new_name(std::string(cache_root) + "/is_dirty_snapshot.txt");

        if (std::ofstream os{old_name, std::ios::out | std::ios::trunc}) {
            for (auto& entry : table) {
                os << entry.first << " " << (entry.second ? "1" : "0") << std::endl;
                // os.write(hashpath(entry.first.c_str()).c_str(), 32);
                // os.write(entry.second ? "1" : "0", 1);
            }
            os.close();
            rename(old_name.c_str(), new_name.c_str());
            // truncate the log
            log.close();
            log.open((std::string(cache_root).append("/is_dirty_log.txt")), std::ios::out | std::ios::trunc);
            return true;
        }
        return false;
    }
    std::unordered_map<std::string, bool> table;
    std::fstream log;
    std::string cache_root;
    int counter;
    int snapshot_frequency;
};

class last_modified_t {
public:
    last_modified_t(std::string& cache_root) : cache_root(cache_root) {
        snapshot_frequency = 10; // after how many logs, make a snapshot and clear the log
        
        // read the snapshot and log to get the most recent state
        std::vector<std::string> filenames{std::string(cache_root).append("/last_modified_snapshot.txt"), std::string(cache_root).append("/last_modified_log.txt")};
        for (std::string& filename : filenames) {
            std::cout << filename << std::endl;
            if (std::ifstream is{filename, std::ios::in | std::ios::ate}) {
                auto size = is.tellg();
                is.seekg(0);
                std::string fn(64, '\0'), ts(sizeof(struct timespec), '\0'); // filename, timespec
                if (size > 0)
                    while (is.tellg() != -1 && is.tellg() != size) {
                        std::cout << is.tellg() << std::endl;
                        std::getline(is, fn);
                        if (fn.size() != 64) {
                            std::cout << "incorrect last_modified persistent file -- fn" << is.tellg() << std::endl;
                            assert(false);
                        }
                        is.read(&ts[0], sizeof(struct timespec));
                        std::cout << is.get() << std::endl; // strip '\n'. should print 10
                        
                        if (ts == std::string(sizeof(struct timespec), 255) && table.count(fn)) 
                            table.erase(fn);
                        else if (ts.size() != sizeof(struct timespec)) {
                            std::cout << "incorrect last_modified persistent file -- ts" << is.tellg() << std::endl;
                            assert(false);
                        }
                        else
                            table[fn] = ts;
                    }
            }
            print_table();
        }
        log.open((filenames[1]), std::ios::out | std::ios::app);
    }

    std::string get(const char* filename) {
        return get(std::string(filename));
    }
    
    std::string get(std::string filename) {
        // return "not found" if not found in table
        std::string hashed_fn = hashpath(filename.c_str());
        return (table.count(hashed_fn) ? table[hashed_fn] : "not found");
    }

    void set(const char* filename, std::string state) {
        set(std::string(filename), state);
    }

    void set(std::string filename, std::string state) {
        std::string hashed_fn = hashpath(filename.c_str());
        // if nothing is changed, do nothing
        if (table.count(hashed_fn) && table[hashed_fn] == state) { return; }

        // otherwise, set it in memory and flush log
        table[hashed_fn] = state;
        log << hashed_fn << std::endl << state << std::endl;
        log.flush();

        // if there are a lot of logs, make snapshot
        if ((counter = (counter + 1) % snapshot_frequency) == 0)
            do_snapshot();
    }

    void erase(const char* filename) {
        erase(std::string(filename));
    }

    void erase(std::string filename) {
        std::string hashed_fn = hashpath(filename.c_str());
        if (table.count(hashed_fn)) {
            table.erase(hashed_fn);
            log << hashed_fn << std::endl << std::string(sizeof(struct timespec), 255) << std::endl; // 0xFF means erase
            log.flush();
        }
    }

    std::size_t count(std::string filename) {
        return table.count(hashpath(filename.c_str()));
    }

    std::size_t size() {
        return table.size();
    }

    void print_table() {
        std::cout << "=========================\n";
        std::cout << "last_modified table\n";
        for (auto item : table) {
            const struct timespec* ts = (const struct timespec*)item.second.c_str();
            std::cout << "\t" << item.first << " = {" << ts->tv_sec << ", " << ts->tv_nsec << "}\n";
        }
        std::cout << "=========================\n";
    }
private:
    bool do_snapshot() {
        std::cout << "[log] last_modified snapshot start" << std::endl;
        std::string old_name(std::string(cache_root) + "/last_modified_snapshot.txt.tmp");
        std::string new_name(std::string(cache_root) + "/last_modified_snapshot.txt");

        if (std::ofstream os{old_name, std::ios::out | std::ios::trunc}) {
            for (auto& entry : table) {
                os << entry.first << std::endl << entry.second << std::endl;
                // os.write(hashpath(entry.first.c_str()).c_str(), 32);
                // os.write(entry.second ? "1" : "0", 1);
            }
            os.close();
            rename(old_name.c_str(), new_name.c_str());
            // truncate the log
            log.close();
            std::cout << "[log] log closed" << std::endl;
            log.open((std::string(cache_root).append("/last_modified_log.txt")), std::ios::out | std::ios::trunc);
            std::cout << "[log] log opened? " << (log.is_open() ? "true" : "false") << std::endl;
            std::cout << "[log] last_modified snapshot finish" << std::endl;
            return true;
        }
        return false;
    }
    std::unordered_map<std::string, std::string> table;
    std::fstream log;
    std::string cache_root;
    int counter;
    int snapshot_frequency;
};

class afs_data_t {
public:
    afs_data_t(std::string cache_root) : cache_root(cache_root), is_dirty{cache_root}, last_modified{cache_root} {}
    std::string cache_root; // must contain forward slash at the end.
    std::unique_ptr<AFS::Stub> stub_;
    // std::unordered_map<std::string, std::string> last_modified; // path to st_mtim
    last_modified_t last_modified;
    is_dirty_t is_dirty;
};

std::string cachepath(const char* rel_path) {
    return AFS_DATA->cache_root + hashpath(rel_path);
}

std::string cachepath(std::string cache_root, const char* rel_path) {
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
    return cache_root + ss.str();
}

std::unordered_map<std::string, std::string> readFileIntoMap(std::string cache_root) {
    std::ifstream file(cachepath(cache_root, LAST_MODIFIED_FILE));         // TODO
    std::unordered_map<std::string, std::string> map;
    std::string key, value;
    while ( file >> key >> value ) {
        map[key] = value; 
    }
    file.close();
    return map;
}

// void writeMapIntoFile() {
//     std::ofstream ofile(cachepath(AFS_DATA->cache_root, LAST_MODIFIED_FILE), std::ios::trunc);
//     for(const auto& kv : AFS_DATA->last_modified) {
//         ofile << kv.first << kv.second << '\n';
//     }
//     ofile.close();
// }

void set_deadline(ClientContext &context) {
    // std::chrono::system_clock::time_point tp = std::chrono::system_clock::now() + 
    //     std::chrono::seconds(5);
    // context.set_deadline(tp);
}

int afs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
           struct fuse_file_info *fi)
{
    Filepath filepath;
    LsResult ls_result;
    ClientContext context;
    
    filepath.set_filepath(std::string(path));
    set_deadline(context);
    AFS_DATA->stub_->Ls(&context, filepath, &ls_result);
    for (int i = 0; i < ls_result.d_name_size(); ++i) {
        if (filler(buf, ls_result.d_name(i).c_str(), NULL, 0) != 0) {
            return -ENOMEM;
        }
    }
    return 0;
}

int afs_open(const char *path, struct fuse_file_info *fi)
{
    struct timespec t, u;
    clock_gettime(CLOCK_MONOTONIC, &t);
    std::cout << "[log] afs_open: start\n";
    std::string path_str(path);
    Filepath filepath;
    filepath.set_filepath(path_str);
    char fpath[PATH_MAX];
    if (AFS_DATA->last_modified.count(path_str) == 1) {
        // cache exists 
        // check if it is locally created file
        if (AFS_DATA->last_modified.get(path_str) == LOCAL_CREAT_FILE) {
            fi->fh = open(cachepath(path).c_str(), fi->flags);
            if (fi->fh < 0) {
                return -errno;
            }
            std::cout << "[log] afs_open: is locally created file\n";
            return 0;
        }
        
        StatContent stat_content;
        ClientContext context;
        set_deadline(context);
        AFS_DATA->stub_->Stat(&context, filepath, &stat_content);

        if ( stat_content.st_mtim() == AFS_DATA->last_modified.get(path_str)) {
            // can use cache
            std::cout << "[log] afs_open: can use local cache\n";
            fi->fh = open(cachepath(path).c_str(), fi->flags);
            if (fi->fh < 0) {
                std::cout << "[err] afs_open: failed to open cache file\n";
                return -errno;
            }
            clock_gettime(CLOCK_MONOTONIC, &u);
            std::cout << "[log] afs_open: end. took " << 
                      ((u.tv_sec - t.tv_sec) * 1000000000 + (u.tv_nsec - t.tv_nsec)) << "ns.\n";
            return 0;
        }
        else {
            // can't use cache.
            AFS_DATA->last_modified.erase(path_str); // delete last_modified entry. 
            unlink(cachepath(path).c_str()); // delete cached file
        }
    }
    
    // get file from server, or create a new one
    std::cout << "[log] afs_open: start downloading from server." << std::endl;
    MetaContent msg;
    ClientContext context;
    set_deadline(context);
    std::unique_ptr<ClientReader<MetaContent>> reader(
        AFS_DATA->stub_->GetContent(&context, filepath));
    if (!reader->Read(&msg)) {
        std::cout << "[err] afs_open: failed to download from server." << std::endl;
        return -EIO;
    }
    if (msg.file_exists()) {
        // open file with O_TRUNC
        std::ofstream ofile(cachepath(path),
            std::ios::binary | std::ios::out | std::ios::trunc);
        // TODO: check failure
        ofile << msg.b();
        while (reader->Read(&msg)) {
            ofile << msg.b();
        }

        Status status = reader->Finish();
        if (!status.ok()) {
            std::cout << "[err] afs_open: failed to download from server." << std::endl;
            return -EIO;
        }
        ofile.close(); // the cache is persisted
        
        AFS_DATA->last_modified.set(path_str, msg.timestamp());
        // writeMapIntoFile();
        std::cout << "[log] afs_open: finish download from server\n";
    }
    else {
        std::cout << "[log] afs_open: create a new file\n";
        AFS_DATA->last_modified.set(path_str, LOCAL_CREAT_FILE);
        // writeMapIntoFile();
        close(creat(cachepath(path).c_str(), 00777));
    }

    // set is_dirty to clean
    AFS_DATA->is_dirty.set(path, 0);

    // give user the file
    fi->fh = open(cachepath(path).c_str(), fi->flags);
    if (fi->fh < 0) {
        std::cout << "[err] afs_open: error open downloaded cache file.\n";
        return -errno;
    }
    clock_gettime(CLOCK_MONOTONIC, &u);
    std::cout << "[log] afs_open: end. took " << 
                ((u.tv_sec - t.tv_sec) * 1000000000 + (u.tv_nsec - t.tv_nsec)) << "ns.\n";
    return 0;    
}

int afs_release(const char *path, struct fuse_file_info *fi)
{
    struct timespec t, u;
    clock_gettime(CLOCK_MONOTONIC, &t);
    std::cout << "[log] afs_release start\n";
    close(fi->fh);
    if (AFS_DATA->is_dirty.get(path)) {
        std::cout << "[log] afs_release dirty file. upload\n";
        Meta meta;
        FilepathContent content;
        ClientContext context;
        content.set_filepath(std::string(path));
        set_deadline(context);
        auto writer = AFS_DATA->stub_->Write(&context, &meta);
        std::ifstream file(cachepath(path), std::ios::in);
        if (file.is_open() == false) {
            std::cerr << "[log] afs_release: file not opened.\n";
            exit(1);
        }
        std::string buf(BUFSIZE, '\0');
        while (file.read(&buf[0], BUFSIZE)) {
            content.set_b(buf);
            if (!writer->Write(content))
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
        // int pause;
        // std::cout << "release crash point (after uploding data)\n";
        // std::cin >> pause;
        if (status.ok()) {
            AFS_DATA->is_dirty.set(path, false); // now the copy is "clean"
            AFS_DATA->last_modified.set(std::string(path), meta.timestamp());
            // writeMapIntoFile();
        } else {
            std::cout << "[log] afs_release: error during upload:\n"; // took " << 
            std::cout << "[log]              " << status.error_code() << ": " << status.error_message() << std::endl;
            return EIO;
        }

        clock_gettime(CLOCK_MONOTONIC, &u);
        // AFS_DATA->last_modified.print_table();
        std::cout << "[log] afs_release: dirty file upload finished. took " << 
        ((u.tv_sec - t.tv_sec) * 1000000000 + (u.tv_nsec - t.tv_nsec)) << "ns.\n";
        return 0;
    }
    else {
        clock_gettime(CLOCK_MONOTONIC, &u);
        std::cout << "[log] afs_release: clean file. direct return. took " << 
        ((u.tv_sec - t.tv_sec) * 1000000000 + (u.tv_nsec - t.tv_nsec)) << "ns.\n";
        return 0;

    }
}

int afs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int rc = fsync(fi->fh);
    if (rc < 0)
        return -errno;
    return rc;
}

int afs_mknod(const char *path, mode_t mode, dev_t dev)
{
    Filepath filepath;
    filepath.set_filepath(std::string(path));
    StatContent stat_content;
    ClientContext context;
    set_deadline(context);
    AFS_DATA->stub_->Stat(&context, filepath, &stat_content);

    if(stat_content.return_code() == 0) {
        std::cerr << "File already exists." << std::endl;
        return EEXIST;
    }

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
    ClientContext context2;
    content.set_filepath(std::string(path));
    set_deadline(context2);
    auto writer = AFS_DATA->stub_->Write(&context2, &meta);
    std::string buf;
    content.set_b(buf);
    writer->Write(content);
    writer->WritesDone();

    Status status = writer->Finish();
    if (status.ok()) {
        AFS_DATA->last_modified.set(std::string(path), meta.timestamp());
        // writeMapIntoFile();
    } else {
        std::cerr << status.error_code() << ": " << status.error_message() << std::endl;
        return EIO;
    }

    return 0;
}

int afs_unlink(const char *path)
{
    std::string path_str(path);
    Filepath fp;
    Response res;
    ClientContext context;
    fp.set_filepath(path_str);

    // cache: remove file, last_modified, is_dirty
    unlink(cachepath(path).c_str()); // remove cached file
    AFS_DATA->last_modified.erase(std::string(path));
    AFS_DATA->is_dirty.erase(path);

    // grpc unlink
    set_deadline(context);
    AFS_DATA->stub_->Unlink(&context, fp, &res);
    if (res.return_code() < 0) {
        return -res.error_number();
    }
    return res.return_code();
}

int afs_getattr(const char *path, struct stat *stbuf)
{
    Filepath request;
    StatContent response;
    ClientContext context;
    request.set_filepath(std::string(path));
    set_deadline(context);
    AFS_DATA->stub_->Stat(&context, request, &response);

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
    ClientContext context;
    fp.set_filepath(path_str);

    set_deadline(context);
    AFS_DATA->stub_->Mkdir(&context, fp, &res);
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
    ClientContext context;
    fp.set_filepath(path_str);

    set_deadline(context);
    AFS_DATA->stub_->Rmdir(&context, fp, &res);
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
    AFS_DATA->is_dirty.set(path, true);
    ssize_t rc = pwrite(fi->fh, buf, size, offset);
    if (rc < 0)
        return -errno;
    return rc;
}

int afs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int rc = ftruncate(fi->fh, offset);
    if (rc < 0)
        return -errno;
    
    return 0;
}

int afs_truncate(const char *path, off_t newsize)
{
    int rc = truncate(cachepath(path).c_str(), newsize);
    if (rc < 0)
        return -errno;
    return 0;
}

static struct fuse_operations afs_oper;

void print_usage(char* prog_name) {
    std::cout << prog_name
              << " [-s <serverhostname:port>] -c <cached files directory> "
              << "-m <mount point directory> | -h" <<std::endl;
    std::cout << "[-h] shows this usage info." << std::endl
              << "If \"-s <serverhostname:port>\" is not present, " << DEFAULT_SERVER
              << " is used." << std::endl;
}

int main(int argc, char *argv[])
{
    int opt;
    char** fuse_main_argv = new char*[10];
    if (!fuse_main_argv) {
        std::cerr << "C++ new failed.\n";
        exit(1);
    }
    fuse_main_argv[0] = argv[0];
    memset(&fuse_main_argv[1], 0, sizeof(char *) * 9);
    // fuse_main_argv[1] = fuse_main_argv[2] = fuse_main_argv[3] = NULL;
    int fuse_main_argc = 1, mount_dir_arg_index = -1;
    char* cache_root = NULL;
    std::string server_addr = DEFAULT_SERVER;
    while ((opt = getopt(argc, argv, "hs:c:m:df")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                exit(0);
            case 's':
                server_addr = optarg;
                break;
            case 'c':
                cache_root = optarg;
                break;
            case 'm':
                mount_dir_arg_index = fuse_main_argc;
                fuse_main_argv[fuse_main_argc++] = optarg;
                break;
            case 'd':
                fuse_main_argv[fuse_main_argc++] = "-d";
                break;
            case 'f':
                fuse_main_argv[fuse_main_argc++] = "-f";
                break;
            case '?':
            default:
                std::cerr << "opt: " << opt << "\n";
                std::cerr << argv[0] << ": invalid argument\n";
                print_usage(argv[0]);
                exit(1);
        }
    }
    if (!cache_root) {
        std::cerr << argv[0] << ": use -c to specify cached files directory.\n";
        std::cerr << "type -h for usage.\n";
        exit(1);
    }
    if (mount_dir_arg_index < 0) {
        std::cerr << argv[0] << ": use -m to specify mount point directory.\n";
        std::cerr << "type -h for usage.\n";
        exit(1);
    }
    
    // check cache_root and mount_point[1] directory exists
    struct stat sb;
    if (stat(cache_root, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        std::cerr << argv[0] << ": cached files directory does not exist.\n";
        std::cerr << "type -h for usage.\n";
        exit(1);
    }
    if (stat(fuse_main_argv[mount_dir_arg_index], &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        std::cerr << argv[0] << ": mount point directory does not exist.\n";
        std::cerr << "type -h for usage.\n";
        exit(1);
    }

    afs_data_t* afs_data = new afs_data_t(std::string(cache_root));
    afs_data->stub_ = AFS::NewStub(grpc::CreateChannel(server_addr, grpc::InsecureChannelCredentials()));
    if (afs_data->cache_root.back() != '/') { afs_data->cache_root += '/'; }
    // afs_data->last_modified = readFileIntoMap(cache_root);
    std::cout<<"[STARTUP-------------------]"<< afs_data->last_modified.size() <<"\n"; 

    afs_oper.getattr    = afs_getattr;
    afs_oper.mknod      = afs_mknod;
    afs_oper.mkdir      = afs_mkdir;
    afs_oper.unlink     = afs_unlink;
    afs_oper.rmdir      = afs_rmdir;
    afs_oper.open       = afs_open;
    afs_oper.read       = afs_read;
    afs_oper.write      = afs_write;
    afs_oper.release    = afs_release;
    afs_oper.fsync      = afs_fsync;
    afs_oper.readdir    = afs_readdir;
    afs_oper.ftruncate  = afs_ftruncate;
    afs_oper.truncate   = afs_truncate;
    int rc = fuse_main(fuse_main_argc, fuse_main_argv, &afs_oper, afs_data);
    delete fuse_main_argv;
    return rc;
}

