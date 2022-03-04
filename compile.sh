#!/bin/bash
mkdir -p build/no_opt
pushd build/no_opt
cmake -DCMAKE_BUILD_TYPE=Debug ../..
make -j
popd

mkdir -p build/O3
pushd build/O3
cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j
popd
