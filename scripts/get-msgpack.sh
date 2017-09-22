#! /usr/bin/env sh
#
# Download and compile a stable and recent version of msgpack-c.
# We end up with a 'sysroot' that contains at least a lib/ and include/
# directories that can be used by the FindMsgpack.cmake file we provide.

set -e
set -u

# We will work in the .deps/ directory.
rm -rf .deps
mkdir -p .deps
cd .deps

RELEASE="2.1.5"
DIR="msgpack-$RELEASE"
TARBALL="$DIR.tar.gz"
URL="https://github.com/msgpack/msgpack-c/releases/download/cpp-2.1.5/$TARBALL"

wget "$URL"
tar -xf "$TARBALL"
rm "$TARBALL"

cd "$DIR"
mkdir -p build
cd build

# Configure msgpack to disable shared libraries, examples and c++ code We will
# keep only C static libraries, and install them in the sysroot aformentioned.
cmake \
   -DMSGPACK_ENABLE_SHARED=OFF \
   -DMSGPACK_ENABLE_CXX=OFF \
   -DMSGPACK_BUILD_EXAMPLES=OFF \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_INSTALL_PREFIX=../../_sysroot \
   ..
cmake --build . --target install
