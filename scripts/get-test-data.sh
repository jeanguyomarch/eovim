#! /usr/bin/env sh
#
# Retrieve the test data, mandatory to pixel perfect regression test suite

set -e
set -u

# We will work in the .deps/ directory.
mkdir -p .deps
cd .deps

GIT_URL="https://git.enlightenment.org/devs/jayji/eovim-test-data.git"
GIT_BRANCH=$(git branch --no-color --list | grep "^\*\s" | cut -f 2 -d ' ')
if [ "$GIT_BRANCH" = "(HEAD" ]; then
   GIT_BRANCH=master
fi

DIR="test-data"

if [ -d "$DIR" ]; then
   echo "*** $GIT_URL has already been cloned. Doing nothing."
   exit 1
fi

git clone --quiet --depth 1 --branch "$GIT_BRANCH" "$GIT_URL" "$DIR"
echo "Test data are available at .deps/$DIR (branch $GIT_BRANCH)"
