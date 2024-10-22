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
# Run class for the adaptived functional tests
#
# Note: based on libcgroup's ftests.py
#
#

from subprocess import TimeoutExpired
from log import Log
import subprocess
import time


class Run(object):
    @staticmethod
    def __run(command, shell, timeout):
        if shell:
            if isinstance(command, str):
                # nothing to do.  command is already formatted as a string
                pass
            elif isinstance(command, list):
                command = ' '.join(command)
            else:
                raise ValueError('Unsupported command type')

        subproc = subprocess.Popen(command, shell=shell,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE)

        if timeout:
            try:
                out, err = subproc.communicate(timeout=timeout)
                ret = subproc.returncode

                out = out.strip().decode('UTF-8')
                err = err.strip().decode('UTF-8')
            except TimeoutExpired as te:
                if te.stdout:
                    out = te.stdout.strip().decode('UTF-8')
                else:
                    out = ''
                if te.stderr:
                    err = te.stderr.strip().decode('UTF-8')
                else:
                    err = ''

                if len(err):
                    ret = -1
                else:
                    ret = 0
        else:
            out, err = subproc.communicate()
            ret = subproc.returncode

            out = out.strip().decode('UTF-8')
            err = err.strip().decode('UTF-8')

        if shell:
            Log.log_debug(
                            'run:\n\tcommand = {}\n\tret = {}\n\tstdout = {}'
                            '\n\tstderr = {}'
                            ''.format(command, ret, out, err)
                         )
        else:
            Log.log_debug(
                            'run:\n\tcommand = {}\n\tret = {}\n\tstdout = {}'
                            '\n\tstderr = {}'
                            ''.format(' '.join(command), ret, out, err)
                         )

        return ret, out, err

    @staticmethod
    def run(command, shell=False, timeout=None, ignore_profiling_errors=True):
        ret, out, err = Run.__run(command, shell, timeout)

        if ret != 0:
            raise RunError("Command '{}' failed".format(''.join(command)),
                           command, ret, out, err)
        if ret != 0 or len(err) > 0:
            if ignore_profiling_errors and err.find('profiling') >= 0:
                pass
            else:
                raise RunError("Command '{}' failed".format(''.join(command)),
                               command, ret, out, err)

        return out


class RunError(Exception):
    def __init__(self, message, command, ret, stdout, stderr):
        super(RunError, self).__init__(message)

        self.command = command
        self.ret = ret
        self.stdout = stdout
        self.stderr = stderr

    def __str__(self):
        out_str = 'RunError:\n\tcommand = {}\n\tret = {}'.format(
                  self.command, self.ret)
        out_str += '\n\tstdout = {}\n\tstderr = {}'.format(self.stdout,
                                                           self.stderr)
        return out_str

# vim: set et ts=4 sw=4:
