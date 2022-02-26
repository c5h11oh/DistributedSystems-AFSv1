cmake_minimum_required(VERSION 3.2)
project(GRPC_CS739 VERSION 0.1.0)

# https://stackoverflow.com/questions/41361631/optimize-in-cmake-by-default
add_compile_options(-std=c++11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-O -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS "-D_FILE_OFFSET_BITS=64")

# https://github.com/IvanSafonov/grpc-cmake-example/blob/master/CMakeLists.txt

# GRPC and Protocol Buffers libraries location
list(APPEND CMAKE_PREFIX_PATH "/p2/installs")

# Cmake find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# include Protobuf and gRPC -- copied from grpc/examples/cpp/cmake/common.cmake
    # This branch assumes that gRPC and all its dependencies are already installed
    # on this system, so they can be located by find_package().

    # Find Protobuf installation
    # Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
    set(protobuf_MODULE_COMPATIBLE TRUE)
    find_package(Protobuf CONFIG REQUIRED)
    message(STATUS "Using protobuf ${Protobuf_VERSION}")

    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    set(_REFLECTION gRPC::grpc++_reflection)
    if(CMAKE_CROSSCOMPILING)
        find_program(_PROTOBUF_PROTOC protoc)
    else()
        set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
    endif()

    # Find gRPC installation
    # Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
    find_package(gRPC CONFIG REQUIRED)
    message(STATUS "Using gRPC ${gRPC_VERSION}")

    set(_GRPC_GRPCPP gRPC::grpc++)
    if(CMAKE_CROSSCOMPILING)
        find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
    else()
        set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
    endif()
# -- end of include Protobuf and gRPC

# -- compile .proto to .cc and .h 
#    modified from grpc/examples/cpp/helloworld/CMakeLists.txt
foreach(_service_name afs)
    # Proto file
    get_filename_component(hw_proto "protos/${_service_name}.proto" ABSOLUTE)
    get_filename_component(hw_proto_path "${hw_proto}" PATH)
    # Generated sources
    set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/${_service_name}.pb.cc")
    set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${_service_name}.pb.h")
    set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${_service_name}.grpc.pb.cc")
    set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${_service_name}.grpc.pb.h")
    add_custom_command(
        OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
        COMMAND ${_PROTOBUF_PROTOC}
        ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
            --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
            -I "${hw_proto_path}"
            --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
            "${hw_proto}"
            DEPENDS "${hw_proto}")

# -- end of compile .proto to .cc and .h 
            
# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# hw_grpc_proto
add_library(${_service_name}_hw_grpc_proto
  ${hw_grpc_srcs}
  ${hw_grpc_hdrs}
  ${hw_proto_srcs}
  ${hw_proto_hdrs})
target_link_libraries(${_service_name}_hw_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

foreach(_target client server)
    add_executable(${_service_name}_${_target} "${_service_name}_${_target}.cc")
    target_link_libraries(${_service_name}_${_target}
        ${_service_name}_hw_grpc_proto
        ${_REFLECTION}
        ${_GRPC_GRPCPP}
        ${_PROTOBUF_LIBPROTOBUF}
    )
endforeach(_target)

endforeach(_service_name)
