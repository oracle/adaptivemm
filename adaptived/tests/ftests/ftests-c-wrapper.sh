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


BASEDIR=$(dirname $0)

FAILED_TESTS=()
RET=0

# Sort the files in reverse order so that the sudo tests are run last.  They
# can mess up the permissions on the code coverage files.
FILES=$(ls | sort -r)

for FILE in $FILES; do
	CMD=""
	if [[ $FILE =~ ^test[0-9]{3} ]]; then
		EXE=$FILE

		if [[ -x "./$EXE" ]]; then
			CMD="./$EXE"
		else
			CMD=${srcdir}/$EXE
		fi
	elif [[ $FILE =~ ^sudo1[0-9]{3} ]]; then
		EXE=$FILE

                if [[ -x "./$EXE" ]]; then
                        CMD="sudo ./$EXE"
                else
                        CMD=sudo ${srcdir}/$EXE
		fi
	fi
	if [ -z "$CMD" ] ; then
		echo "Missing command for $FILE."
		continue
	fi

	echo "Running $CMD $BASEDIR"
	$CMD $BASEDIR
	TMP_RET=$?
	echo "$CMD returned $TMP_RET"

	if [[ $TMP_RET -gt 0 ]]; then
		FAILED_TESTS+=($FILE)
	fi
	if [[ $TMP_RET -gt $RET ]]; then
		RET=$TMP_RET
	fi
done

if [[ ${#FAILED_TESTS[@]} > 0 ]]; then
	echo -e "\nThe following tests did not pass:"
	echo "---------------------------------"
	for FAILED_TEST in "${FAILED_TESTS[@]}"; do
		echo "$FAILED_TEST"
	done
fi

exit $RET
