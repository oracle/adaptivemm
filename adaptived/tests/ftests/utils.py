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
# Utility functions for the adaptived functional tests
#
#

from run import Run
import datetime

NOW_TOKEN = '<< now'


# function to indent a block of text by cnt number of spaces
def indent(in_str, cnt):
    leading_indent = cnt * ' '
    return ''.join(leading_indent + line for line in in_str.splitlines(True))


def __parse_now_token(token):
    math_op = int(token[len(NOW_TOKEN) : len(token) - len('>>')].replace(' ', ''))

    token_time = datetime.datetime.now() + datetime.timedelta(seconds=math_op)
    token_time_str = token_time.strftime('%H:%M:%S')
    return token_time_str


# Find and replace magic tokens in config files.  The caller is expected to
# delete the out_filename during cleanup.
def config_find_and_replace(in_filename, out_filename):
    tokens = [NOW_TOKEN]
    parsers = ['__parse_now_token']

    with open(in_filename) as ifile:
        with open(out_filename, 'w') as ofile:

            for line in ifile.readlines():
                for idx, token in enumerate(tokens):
                    token_idx = line.find(token)
                    if token_idx >= 0:
                        end_idx = token_idx + line[token_idx:].index('>>') + 2
                        parsed_val = eval(parsers[idx])(line[token_idx : end_idx])

                        ofile.write('{}{}{}'.format(line[:token_idx], parsed_val,
                                                    line[end_idx:]))
                    else:
                        ofile.write(line)


# vim: set et ts=4 sw=4:
