#!/bin/bash
# Copyright (c) 2024, Oracle and/or its affiliates.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.

# clean up from a previous run
make clean
find . -name "*.gcno" -exec sudo rm '{}' \;
find . -name "*.gcda" -exec sudo rm '{}' \;

./autogen.sh
RET=$?

if [[ $RET -ne 0 ]]; then
	echo "./autogen failed: " $RET
	exit $RET
fi

CFLAGS="$CFLAGS -g -O0 -Werror" ./configure
RET=$?

if [[ $RET -ne 0 ]]; then
	echo "./configure failed: " $RET
	exit $RET
fi

make distcheck
RET=$?

if [[ $RET -ne 0 ]]; then
	echo "make distcheck failed: " $RET
	exit $RET
fi

CFLAGS="$CFLAGS -g -O0 -Werror" ./configure --enable-code-coverage
RET=$?

if [[ $RET -ne 0 ]]; then
	echo "./configure w/ code coverage failed: " $RET
	exit $RET
fi

# I'm not fond of running all of the tests as root, but the sd-bus tests
# need administrative privileges.  Merging the non-sudo and sudo code
# coverage files is difficult.  Let's avoid that.
sudo make check
RET=$?

if [[ $RET -ne 0 ]]; then
	echo "make check failed: " $RET
	exit $RET
fi

gcovr -s --filter src/
exit 0
