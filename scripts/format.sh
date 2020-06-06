#! /usr/bin/env sh

set -e
set -u
set -x

find src/ include/ -type f \( -name "*.c" -o -name "*.h" \) -print | xargs clang-format -i
