#!/usr/bin/elw python3
# LWDIA_COPYRIGHT_BEGIN
#
# Copyright 2019 by LWPU Corporation.  All rights reserved.  All
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
import glob

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
        with open("amodel.jso", "r") as readFile:
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
    """Execute mods on amodel and print summary results for DVS to parse.
"""

    if argv is None:
        argv = sys.argv

    parser = argparse.ArgumentParser(description='DVS wrapper script for running mods.')
    parser.add_argument('chip', metavar='chip', type=str, help='chip selection')
    parser.add_argument('specfile', metavar='specfile', type=str, help='spec filename')
    parser.add_argument('spec', metavar='spec', type=str, help='spec selection')
    parser.add_argument('-j', '--jsfile', type=str, help='JS file to use', default='gputest.js')
    parser.add_argument('-a', '--args', type=str, help='Additional argument to pass to mods', default='')
    args = parser.parse_args(argv[1:])

    elw = os.elwiron.copy()
    elw['OGL_ChipName'] = args.chip
    elw['LD_LIBRARY_PATH'] = '.:/home/toolroot/CCACHE/ccache_3_5_p1/install_dir/GCC/gcc-5.2.0/lib64'

    modsPassed = True
    try:
        command = 'mods {} -json_clobber amodel.jso -readspec {} -spec {} '.format(args.jsfile, args.specfile, args.spec)
        command += args.args
        split_command = command.split()
        subprocess.check_call(split_command, elw=elw)

    except subprocess.CalledProcessError as ex:
        print(ex)
        modsPassed = False
        pass

    glbugFilelist = glob.glob('glbug_*.js')
    if glbugFilelist:
        try:
            subprocess.check_call('zip glbug_all_js.zip glbug_*.js', shell=True)
        except subprocess.CalledProcessError as ex:
            print(ex)

    results = ParseResults(modsPassed)
    printDvsResults(results)

if __name__ == "__main__":
    sys.exit(main())
