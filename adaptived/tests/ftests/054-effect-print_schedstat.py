#!/usr/bin/env python3
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
# Test the print_schedstat effect
#
#

import filecmp
import adaptived
import consts
import ftests
import sys
import os

CONFIG = '054-effect-print_schedstat.json'
OUT_FILE = '054-effect-print_schedstat.out'
EXPECTED_RET = 62
MAX_LOOPS = 1

def prereqs(config):
    result = consts.TEST_PASSED
    cause = None

    return result, cause


def setup(config):
    pass


def test(config):
    result = consts.TEST_PASSED
    cause = None

    out = adaptived.adaptived(config=CONFIG, max_loops=MAX_LOOPS, expected_ret=EXPECTED_RET)

    with open(OUT_FILE, 'w') as ofile:
        ofile.write(out)
        ofile.write('\n')

    match = filecmp.cmp(OUT_FILE, '054-effect-print_schedstat.expected', shallow=False)
    if not match:
        result = consts.TEST_FAILED
        cause = 'Expected out file differs from received out file'

    return result, cause


def teardown(config):
    pass


def main(config):
    [result, cause] = prereqs(config)
    if result != consts.TEST_PASSED:
        return [result, cause]

    try:
        setup(config)
        [result, cause] = test(config)
    finally:
        teardown(config)

    try:
        os.remove(OUT_FILE)
    except FileNotFoundError:
        pass

    return [result, cause]


if __name__ == '__main__':
    config = ftests.parse_args()
    # this test was invoked directly.  run only it
    config.args.num = int(os.path.basename(__file__).split('-')[0])
    sys.exit(ftests.main(config))

# vim: set et ts=4 sw=4:
