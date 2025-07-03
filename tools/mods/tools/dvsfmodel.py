#!/usr/bin/elw python3
# LWDIA_COPYRIGHT_BEGIN
#
# Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import argparse
import subprocess
import sys
import os
import json
import shlex

def printDvsResults(info):
    """Display the score information to allow DVS to parse them correctly.
    """
    score = 0.0
    total = (info["passed"] + info["failed"])
    if total != 0:
        score = (float(info["passed"]) / float(total)) * 100.0
    with open("summarylog.txt", "w") as logfile:
        logfile.write("RESULT\n")
        logfile.write("Passes   : " + str(info["passed"]) + "\n")
        logfile.write("Failures : " + str(info["failed"]) + "\n")
        logfile.write("SANITY SCORE: %.2f\n" % score)

def ParseResults(modsPassed):
    """Parse the results from the MODS run and return the number of passes and failures.
"""

    passed = 0
    failed = 0

    # If any tests were run go through all of the JSON test exit nodes and count
    # passes/failures based on the RC of each node
    data = []
    try:
        with open("fmodel.jso", "r") as readFile:
            data = json.load(readFile)
    except:
        pass

    if len(data) and len(data[0]):
        passed = len([node['rc'] for node in data[0] if node['__tag__'] == 'exit' and 'rc' in node and node['rc'] == 0])
        failed = len([node['rc'] for node in data[0] if node['__tag__'] == 'exit' and 'rc' in node and node['rc'] != 0])

        # If there were no tests detected as being run yet MODS exited successfully
        # it must have been a "-notest" so dont flag a failure, otherwise flag the
        # fact that no tests were run as a failure as well
        if passed == 0 and failed == 0 and not modsPassed:
            failed = 1

    # The return code from MODS itself is part of the pass/failure
    if modsPassed:
        passed = passed + 1
    else:
        failed = failed + 1

    return { 'passed' : passed, 'failed' : failed }

def main (argv=None):
    """Execute mods on fmodel and print summary results for DVS to parse.
"""

    if argv is None:
        argv = sys.argv

    parser = argparse.ArgumentParser(description='DVS wrapper script for running mods.')
    parser.add_argument('chiplib', metavar='chiplib', type=str, help='chiplib filenames seperated by colon')
    parser.add_argument('biosfile', metavar='biosfile', type=str, nargs='?', help='vbios filenames seperated by colon')
    parser.add_argument('specfile', metavar='specfile', type=str, help='mods test spec filename')
    parser.add_argument('test', metavar='test', type=str, help='test to run')
    parser.add_argument('-j', '--jsfile', type=str, help='JS file to use', default='gputest.js')
    parser.add_argument('-a', '--args', type=str, help='Additional argument to pass to mods', default='')
    args = parser.parse_args(argv[1:])

    command = './mods'

    chiplibs = args.chiplib.split(':')
    for chiplib in chiplibs:
        command += ' -chip {}'.format(chiplib)

    if args.biosfile:
        biosfiles = args.biosfile.split(':')
        for i,biosfile in enumerate(biosfiles):
            command += ' -gpubios {} {}'.format(i, args.biosfile)

    command += ' {}'.format(args.jsfile)

    if len(chiplibs) > 1:
        command += ' -multichip_paths {}'.format(':'.join(chiplibs))

    command += ' -readspec {} -spec {} -json_clobber fmodel.jso'.format(args.specfile, args.test)

    modsPassed = True
    try:
        subprocess.check_call(shlex.split(command) + args.args.split())
    except subprocess.CalledProcessError as ex:
        print(ex)
        modsPassed = False
        pass

    results = ParseResults(modsPassed)
    printDvsResults(results)

if __name__ == "__main__":
    sys.exit(main())
