#! /usr/bin/env sh
#
# This script is supposed to be run by a continuous integration server only.

set -e
set -u
set -x

sh -x ./scripts/get-msgpack.sh
sh -x ./scripts/get-test-data.sh

mkdir -p build/_install && cd build

install_dir="$(pwd)/_install"

# Install exactness
git clone https://git.enlightenment.org/tools/exactness.git
cd exactness
git checkout 62298eb8ffd50f9b470dd69e97e7a718d31b9fa9
autoreconf -fi
./configure --prefix "$install_dir"
make install
cd ..

# Make sure exactness is available
export PATH="$PATH:$install_dir/bin"
export LD_LIBRARY_PATH="$install_dir/lib"

cmake -DWITH_TESTS=ON -DCMAKE_INSTALL_PREFIX=_install ..
cmake --build .
cmake --build . --target install
#ctest --output-on-failure # XXX For now
