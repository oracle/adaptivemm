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
# Config class for the adaptived functional tests
#
#

import consts
import utils
import os


class Config(object):
    def __init__(self, args):
        self.args = args
        self.skip_list = []

        self.ftest_dir = os.path.dirname(os.path.abspath(__file__))
        self.adaptived_dir = os.path.dirname(self.ftest_dir)

        self.test_suite = consts.TESTS_RUN_ALL_SUITES
        self.test_num = consts.TESTS_RUN_ALL
        self.verbose = False

    def __str__(self):
        out_str = 'Configuration\n'
        out_str += utils.indent('args = {}'.format(self.args), 4)
        out_str += utils.indent('skip_list = {}'.format(self.skip_list.join(',')), 4)

        return out_str


class ConfigError(Exception):
    def __init__(self, message):
        super(ConfigError, self).__init__(message)

    def __str__(self):
        out_str = 'ConfigError:\n\tmessage = {}'.format(self.message)
        return out_str

# vim: set et ts=4 sw=4:
