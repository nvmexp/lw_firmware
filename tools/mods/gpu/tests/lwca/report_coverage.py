#! /usr/bin/elw python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import os, sys, subprocess, re, getopt
from sassreport import parse_sass, ins_base, ins_root, count_strings, get_roots, report_counts

def usage ():
    print('Usage: The following arguments are supported:\n')
    print('           -i, --input [file]')
    print('               [REQUIRED] Specifies the input sass file.\n')
    print('           -o, --output [file]')
    print('               [REQUIRED] Specifies the output report file.\n')
    print('           -r, --revision [revision]')
    print('               [OPTIONAL] Specifies the revision in Perforce that')
    print('               will compare be against.\n')
    print('           -t, --test [test name]')
    print('               [OPTIONAL] Specifies the test name. If no test name is')
    print('               specified, "Unknown" will be used as the test name.\n')
    print('           -h, --help')
    print('               Prints this usage statement.\n')
    return 1

def main ():
    """
    Reports the SASS instructions used.

    This script is based on sassreport.py found in gpu/tests/lwca.
    If the operating environment is setup for Perforce, this script is able
    to pull a previously checked in SASS file for comparison. 

    The script lwrrently supports instruction sets from Kepler and Fermi.
    SASS files from other architectures may be used but the script will 
    not be able to report missed or unexpected instructions.
    """

    argv = sys.argv
    if (len(argv) < 3):
        return usage()

    try:
        opts, args = getopt.getopt(argv[1:], "hi:o:r:t:", ["help", "input=", "output=", "revision=", "test="])
    except getopt.GetoptError as err:
        print(str(err))
        usage()
        sys.exit(2)

    sass = "" 
    report = ""
    revision = None
    test = "Unknown"
    for opt, arg in opts:
        if opt in ("-h", "--help"):
            usage()
            sys.exit(2)
        elif opt in ("-i", "--input"):
            sass = arg
        elif opt in ("-o", "--output"):
            report = arg
        elif opt in ("-r", "--revision"):
            revision = arg
        elif opt in ("-t", "--test"):
            test = arg
        else:
            assert False, "Unhandled option"
    
    if sass == "" or report == "":
        usage()
        sys.exit(2)

    counts = {'numprogs':0, 'roots':set(), 'tests':set(), 'arch':""}
    rev_counts = {'numprogs':0, 'roots':set(), 'tests':set()}

    if revision != None:
        rev_sass = "rev_sass.temp"
        f_rev = open(rev_sass, 'w')
        cmd = ('p4', 'print', '%s#%s' % (sass, revision))
        p = subprocess.Popen(cmd, shell=False, stdout=f_rev, stderr=subprocess.PIPE)
        p.wait()
        outdata, errdata = p.communicate()
        f_rev.close()
        if errdata != "":
            print(errdata)
            print('WARNING: Failed to retrieve specified revision.')
            print('WARNING: Building report without comparison to previous revision.')
            revision = None
        else:
            parse_sass(open(rev_sass, 'r').readlines(), rev_counts, test)
        cmd = ('rm', rev_sass)
        p = subprocess.Popen(cmd, shell=False)
       
    parse_sass(open(sass, 'r').readlines(), counts, test)
    for line in open(sass, 'r').readlines():
        arch = re.search('sm_[0-9]+', line)
        if arch == None:
            arch = re.search('EF_LWDA_(SM([0-9]+))', line)
            if arch != None:
                counts['arch'] = "sm_" + arch.group(2)
                break;
        else:
            counts['arch'] = arch.group(0)
            break;

    report_counts(counts, sass, report, revision, rev_counts)

if __name__ == "__main__":
    sys.exit(main())



