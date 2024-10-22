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
# Main entry point for the adaptived functional tests
#
# Note: based on libcgroup's ftests.py
#
#

from config import Config
from run import Run
import datetime
import argparse
import consts
import time
import log
import sys
import os

setup_time = 0.0
teardown_time = 0.0

Log = log.Log


def parse_args():
    parser = argparse.ArgumentParser("Adaptived Functional Tests")
    parser.add_argument(
                            '-l', '--loglevel',
                            help='log level',
                            required=False,
                            type=int,
                            default=None
                        )
    parser.add_argument(
                            '-L', '--logfile',
                            help='log file',
                            required=False,
                            type=str,
                            default=None
                        )

    parser.add_argument(
                            '-N', '--num',
                            help='Test number to run.  If unspecified, all '
                                 'tests are run',
                            required=False,
                            default=consts.TESTS_RUN_ALL,
                            type=int
                        )
    parser.add_argument(
                            '-S', '--skip',
                            help="Test number(s) to skip.  If unspecified, all"
                                 " tests are run. To skip multiple tests, "
                                 "separate them via a ',', e.g. '5,7,12'",
                            required=False,
                            default='',
                            type=str
                        )
    parser.add_argument(
                            '-s', '--suite',
                            help='Test suite to run, e.g. cpuset',
                            required=False,
                            default=consts.TESTS_RUN_ALL_SUITES,
                            type=str
                        )
    parser.add_argument(
                            '-v', '--verbose',
                            help='Print all information about this test run',
                            default=True,
                            required=False,
                            action="store_false"
                        )

    config = Config(parser.parse_args())

    if config.args.skip is None or config.args.skip == '':
        pass
    elif config.args.skip.find(',') < 0:
        config.skip_list.append(int(config.args.skip))
    else:
        # multiple tests are being skipped
        for test_num in config.args.skip.split(','):
            config.skip_list.append(int(test_num))

    if config.args.loglevel:
        log.log_level = config.args.loglevel
    if config.args.logfile:
        log.log_file = config.args.logfile

    return config


def setup(config, do_teardown=True, record_time=False):
    global setup_time

    start_time = time.time()
    if do_teardown:
        # belt and suspenders here.  In case a previous run wasn't properly
        # cleaned up, let's try and clean it up here
        try:
            teardown(config)
        except Exception as e:
            # log but ignore all exceptions
            Log.log_debug(e)

    if record_time:
        setup_time = time.time() - start_time


def run_tests(config):
    passed_tests = []
    failed_tests = []
    skipped_tests = []
    filename_max = 0

    for root, dirs, filenames in os.walk(config.ftest_dir):
        for filename in filenames:
            if os.path.splitext(filename)[-1] != ".py":
                # ignore non-python files
                continue

            filenum = filename.split('-')[0]

            try:
                filenum_int = int(filenum)
            except ValueError:
                # D'oh.  This file must not be a test.  Skip it
                Log.log_debug(
                                'Skipping {}.  It doesn\'t start with an int'
                                ''.format(filename)
                             )
                continue

            try:
                filesuite = filename.split('-')[1]
            except IndexError:
                Log.log_critical(
                                 'Skipping {}.  It doesn\'t conform to the '
                                 'filename format'
                                 ''.format(filename)
                                )
                continue

            if config.args.suite == consts.TESTS_RUN_ALL_SUITES or \
               config.args.suite == filesuite:
                if config.args.num == consts.TESTS_RUN_ALL or \
                   config.args.num == filenum_int:

                    if config.args.suite == consts.TESTS_RUN_ALL_SUITES and \
                       filesuite == 'sudo':
                        # Don't run the 'sudo' tests if all tests have been specified.
                        # The sudo tests must be run as sudo and thus need to be separately
                        # invoked.
                        continue

                    if filenum_int in config.skip_list:
                        continue

                    if len(filename) > filename_max:
                        filename_max = len(filename)

                    test = __import__(os.path.splitext(filename)[0])

                    failure_cause = None
                    start_time = time.time()
                    try:
                        Log.log_debug('Running test {}.'.format(filename))
                        [ret, failure_cause] = test.main(config)
                    except Exception as e:
                        # catch all exceptions.  you never know when there's
                        # a crummy test
                        failure_cause = e
                        Log.log_debug(e)
                        ret = consts.TEST_FAILED
                    finally:
                        run_time = time.time() - start_time
                        if ret == consts.TEST_PASSED:
                            passed_tests.append([filename, run_time])
                        elif ret == consts.TEST_FAILED:
                            failed_tests.append([
                                                 filename,
                                                 run_time,
                                                 failure_cause
                                                 ])
                        elif ret == consts.TEST_SKIPPED:
                            skipped_tests.append([
                                                  filename,
                                                  run_time,
                                                  failure_cause
                                                  ])
                        else:
                            raise ValueError('Unexpected ret: {}'.format(ret))

    passed_cnt = len(passed_tests)
    failed_cnt = len(failed_tests)
    skipped_cnt = len(skipped_tests)

    print("-----------------------------------------------------------------")
    print("Test Results:")
    date_str = datetime.datetime.now().strftime('%b %d %H:%M:%S')
    print(
            '\t{}{}'.format(
                                '{0: <35}'.format("Run Date:"),
                                '{0: >15}'.format(date_str)
                            )
         )

    test_str = "{} test(s)".format(passed_cnt)
    print(
            '\t{}{}'.format(
                                '{0: <35}'.format("Passed:"),
                                '{0: >15}'.format(test_str)
                            )
         )

    test_str = "{} test(s)".format(skipped_cnt)
    print(
            '\t{}{}'.format(
                                '{0: <35}'.format("Skipped:"),
                                '{0: >15}'.format(test_str)
                            )
         )

    test_str = "{} test(s)".format(failed_cnt)
    print(
            '\t{}{}'.format(
                                '{0: <35}'.format("Failed:"),
                                '{0: >15}'.format(test_str)
                            )
         )

    for test in failed_tests:
        print(
                "\t\tTest:\t\t\t\t{} - {}"
                ''.format(test[0], str(test[2]))
             )
    print("-----------------------------------------------------------------")

    global setup_time
    global teardown_time
    if config.args.verbose:
        print("Timing Results:")
        print(
                '\t{}{}'.format(
                                    '{0: <{1}}'.format("Test", filename_max),
                                    '{0: >15}'.format("Time (sec)")
                                )
             )
        print(
                # 15 is padding space of "Time (sec)"
                '\t{}'.format('-' * (filename_max + 15))
             )
        time_str = "{0: 2.2f}".format(setup_time)
        print(
                '\t{}{}'.format(
                                    '{0: <{1}}'.format('setup', filename_max),
                                    '{0: >15}'.format(time_str)
                                )
             )

        all_tests = passed_tests + skipped_tests + failed_tests
        all_tests.sort()
        for test in all_tests:
            time_str = "{0: 2.2f}".format(test[1])
            print(
                    '\t{}{}'.format(
                                        '{0: <{1}}'.format(test[0],
                                                           filename_max),
                                        '{0: >15}'.format(time_str)
                                    )
                  )
        time_str = "{0: 2.2f}".format(teardown_time)
        print(
                '\t{}{}'
                ''.format(
                            '{0: <{1}}'.format('teardown', filename_max),
                            '{0: >15}'.format(time_str)
                          )
             )

        total_run_time = setup_time + teardown_time
        for test in passed_tests:
            total_run_time += test[1]
        for test in failed_tests:
            total_run_time += test[1]
        total_str = "{0: 5.2f}".format(total_run_time)
        print('\t{}'.format('-' * (filename_max + 15)))
        print(
                '\t{}{}'
                ''.format(
                            '{0: <{1}}'
                            ''.format("Total Run Time", filename_max),
                            '{0: >15}'.format(total_str)
                         )
              )

    return [passed_cnt, failed_cnt, skipped_cnt]


def teardown(config, record_time=False):
    global teardown_time
    start_time = time.time()

    if record_time:
        teardown_time = time.time() - start_time


def main(config):
    AUTOMAKE_SKIPPED = 77
    AUTOMAKE_HARD_ERROR = 99
    AUTOMAKE_PASSED = 0

    try:
        setup(config, record_time=True)
        [passed_cnt, failed_cnt, skipped_cnt] = run_tests(config)
    finally:
        teardown(config, record_time=True)

    if failed_cnt > 0:
        return failed_cnt
    if passed_cnt > 0:
        return AUTOMAKE_PASSED
    if skipped_cnt > 0:
        return AUTOMAKE_SKIPPED

    return AUTOMAKE_HARD_ERROR


if __name__ == '__main__':
    config = parse_args()
    sys.exit(main(config))

# vim: set et ts=4 sw=4:
