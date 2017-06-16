#! /usr/bin/env sh

set -e
set -u

VERSION="$(nvim --version | head -n1 | cut -f 2 -d ' ' | sed -e 's/\-.*//g' -e 's/v//g')"
API="$(nvim --api-info | python -c 'import msgpack, sys, yaml; print yaml.dump(msgpack.unpackb(sys.stdin.read()))')"

echo "$API" > "api-$VERSION.yml"
