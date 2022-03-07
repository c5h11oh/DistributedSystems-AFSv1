#!/bin/bash
# mkdir -p build/fail_at_upload
# pushd build/fail_at_upload
# cmake -DCMAKE_BUILD_TYPE=Release -DFAILURE_POINT=1 ../..
# make -j
# popd
mkdir -p build/fail_after_upload
pushd build/fail_after_upload
cmake -DCMAKE_BUILD_TYPE=Release -DFAILURE_POINT=2 ../..
make -j
popd
