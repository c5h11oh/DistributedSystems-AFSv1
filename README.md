## How to setup a node
- Before cloning the repo, download (via HTTP or copy/paste) `init.sh` and run it.
    - depending on which login you use, you may want to change `sudo chown c5h11oh /p2` to your own login 
    - and also git name and email, but that doesn't affect script correctness.
- It installs gRPC, FUSE, and download the repository to `/p2/repo`.
- `/p2/repo` will be the working directory.

## gRPC: proto => C++ .h & .cc (.cpp)
- RPC calls are defined in `protos/afs.proto`.
- Run `compile.sh` to 
    1. compile `afs.proto` to C++ RPC files (`.h` and `.cc` files). These files are in `build/O3`. Take a look can help
    2. compile `afs_client.cc`, `afs_server.cc` with the above C++ files and link with appropriate libraries.

## How to write afs_client.cc and afs_server.cc
- You can reference files in `grpc_example/`. This is the file sending service I wrote for project 1.
- [Official C++ tutorial](https://www.grpc.io/docs/languages/cpp/basics/)
- To see the exact grpc methods, run `compile.sh` first, and go to `build/O3` to check out the header files.

