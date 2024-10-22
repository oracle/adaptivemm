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
# Constants for the libcgroup functional tests
#
# Note: based on libcgroup's ftests.py
#
#

import os

DEFAULT_LOG_FILE = 'adaptived-ftests.log'

LOG_CRITICAL = 1
LOG_WARNING = 5
LOG_DEBUG = 8
DEFAULT_LOG_LEVEL = 5

tests_dir = os.path.dirname(os.path.abspath(__file__))
ADAPTIVED_MOUNT_POINT = os.path.dirname(os.path.dirname(tests_dir))

TESTS_RUN_ALL = -1
TESTS_RUN_ALL_SUITES = 'allsuites'
TEST_PASSED = 'passed'
TEST_FAILED = 'failed'
TEST_SKIPPED = 'skipped'

# vim: set et ts=4 sw=4:
