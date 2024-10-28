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
# Log class for the adaptived functional tests
#
# Note: based on libcgroup's ftests.py
#
#

import datetime
import consts

log_level = consts.DEFAULT_LOG_LEVEL
log_file = consts.DEFAULT_LOG_FILE
log_fd = None


class Log(object):

    @staticmethod
    def log(msg, msg_level=consts.DEFAULT_LOG_LEVEL):
        global log_level, log_file, log_fd

        if log_level >= msg_level:
            if log_fd is None:
                Log.open_logfd(log_file)

            timestamp = datetime.datetime.now().strftime('%b %d %H:%M:%S')
            log_fd.write('{}: {}\n'.format(timestamp, msg))

    @staticmethod
    def open_logfd(log_file):
        global log_fd

        log_fd = open(log_file, 'a')

    @staticmethod
    def log_critical(msg):
        Log.log('CRITICAL: {}'.format(msg), consts.LOG_CRITICAL)

    @staticmethod
    def log_warning(msg):
        Log.log('WARNING: {}'.format(msg), consts.LOG_WARNING)

    @staticmethod
    def log_debug(msg):
        Log.log('DEBUG: {}'.format(msg), consts.LOG_DEBUG)

# vim: set et ts=4 sw=4:
