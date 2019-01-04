#! /usr/bin/env sh
#
# This script is supposed to be run by a continuous integration server only.

set -e
set -u
set -x

sh -x ./scripts/get-msgpack.sh

mkdir -p build/_install && cd build

cmake -DWITH_TESTS=OFF -DCMAKE_INSTALL_PREFIX=_install ..
cmake --build .
cmake --build . --target install
