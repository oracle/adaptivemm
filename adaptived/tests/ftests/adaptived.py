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
# Interface into the adaptived daemon for the adaptived functional tests
#
#

from run import Run, RunError
import consts
import utils
import os


def adaptived(config=None, bhelp=False, interval=None, log_location=None,
           log_level=None, max_loops=None, expected_ret=None):
    """run the adaptived daemon
    """
    cmd = list()
    out = None

    cmd.append(os.path.join(consts.ADAPTIVED_MOUNT_POINT, 'src/adaptived'))

    if config:
        tmp_config = config
        token = config.find('.token')
        if token > 0:
            tmp_config = config[:token]
            utils.config_find_and_replace(config, tmp_config)

        cmd.append('-c')
        cmd.append(tmp_config)

    if bhelp:
        cmd.append('-h')

    if interval:
        cmd.append('-i')
        cmd.append(str(interval))

    if log_location:
        cmd.append('-L')
        cmd.append(log_location)

    if log_level:
        cmd.append('-l')
        cmd.append(str(log_level))

    if max_loops:
        cmd.append('-m')
        cmd.append(str(max_loops))

    try:
        out = Run.run(cmd)
    except RunError as re:
        if re.ret == expected_ret:
            out = re.stdout
        else:
            raise re
    finally:
        if tmp_config != config:
            os.remove(tmp_config)

    return out
