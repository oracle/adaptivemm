#!/bin/bash
#

git submodule update --init --recursive

# configure googletest
pushd googletest/googletest
git checkout release-1.8.0
cmake -DBUILD_SHARED_LIBS=ON .
make
popd

test -d m4 || mkdir m4
autoreconf -fi
rm -rf autom4te.cache
