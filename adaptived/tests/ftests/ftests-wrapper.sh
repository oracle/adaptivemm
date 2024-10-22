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

#
# Note: based on libcgroup's ftests-wrapper.sh
#

AUTOMAKE_SKIPPED=77
AUTOMAKE_HARD_ERROR=99

START_DIR=$PWD
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	cp "$SCRIPT_DIR"/*.py "$START_DIR"
	cp "$SCRIPT_DIR"/*.json "$START_DIR"
	cp "$SCRIPT_DIR"/*.token "$START_DIR"
	cp "$SCRIPT_DIR"/*.schedstat "$START_DIR"
	cp "$SCRIPT_DIR"/*.expected "$START_DIR"
fi


./ftests.py -l 10 -L "$START_DIR/ftests.py.log"
RET=$?

if [ -z "$srcdir" ]; then
	# $srcdir is set by automake but will likely be empty when run by hand and
	# that's fine
	srcdir=""
else
	srcdir=$srcdir"/"
fi

if [ "$START_DIR" != "$SCRIPT_DIR" ]; then
	rm -f "$START_DIR"/*.py
	rm -f "$START_DIR"/*.json
	rm -f "$START_DIR"/*.token
	rm -f "$START_DIR"/*.schedstat
	rm -f "$START_DIR"/*.expected
	rm -fr "$START_DIR"/__pycache__
	rm -f ftests.py.log
fi

exit $RET
