#!/usr/bin/elw python3
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2020 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

"""
This program has two functions, which are designed to obfuscate the
names of MODS source files in release (customer) builds.

(1) Generate a fileids.txt file from a list of source files passed
    into stdin, one file per line.  Each source file is assigned a
    unique hexadecimal file ID.  Each line of fileids.txt is of the
    form "FILEID SOURCE".
(2) Generate a g_*-lwlog.h header file, where "*" is the basename of
    one of the source files in fileids.txt (sans directory or suffix).
    This header is included by the source file when it is compiled, so
    that the source file knows its fileid.

At run time, the program will print a fileid instead of a filename in
breakpoint messages.  Engineers can use the internal fileids.txt file
to translate the fileids into filenames.
"""

import argparse
import os
import sys

######################################################################
# Read the fileids.txt file, and return the contents as a list of the
# form [[fileid, filename], [fileid2, filename2], ...].  Return None
# if fileids.txt cannot be read.
#
def ReadFileids(args):
    if not os.path.isfile(args.fileids):
        return None
    retval = []
    with open(args.fileids) as istr:
        for line in istr:
            retval.append(line.strip().split(" ", 1))
    return retval

######################################################################
# Generate the fileids.txt file, with a unique fileid for each source
# file passed into stdin.
# If fileids.txt already exists and the contents are correct, then
# do not touch the file.  The Makefile should realize that the
# timestamp did not change, and not rebuild the dependencies.
#
def GenFileids(args):
    oldFileids = ReadFileids(args)

    newFileids = []
    fileid = 0
    for line in sys.stdin:
        fileid += 1
        newFileids.append(["0x{:06x}".format(fileid),
                           os.path.relpath(line.strip(), args.root)])

    if oldFileids != newFileids:
        with open(args.fileids, "w") as ostr:
            for (fileid, source) in newFileids:
                print("{} {}".format(fileid, source), file = ostr)

######################################################################
# Generate a g_<filename>-lwlog.h header file.
#
def GenHeader(args):
    # Read fileids.txt
    #
    fileids = ReadFileids(args)
    if fileids is None:
        raise Exception("Cannot read {}".format(args.fileids))

    # Extract <filename> from the g_<filename>-lwlog.h file
    #
    basename = os.path.basename(args.header)
    if basename.startswith("g_") and basename.endswith("-lwlog.h") and len(basename) > 10:
        basename = basename[2:-8]
    else:
        raise Exception("{} must have naming convention g_*-lwlog.h")

    # Get the fileid from fileids.txt, by searching fileids.txt for a
    # line of the form "<fileid> <directory>/<filename>.<suffix>",
    # using the <filename> extracted above.
    #
    fileid = None
    for ii in fileids:
        if basename == os.path.splitext(os.path.basename(ii[1]))[0]:
            fileid = ii[0]
            break
    if fileid is None:
        raise Exception("Cannot find {}.* in {}".format(basename, args.fileids))

    # Write the header file
    #
    with open(args.header, "w") as ostr:
        print("#define LWLOG_FILEID {}".format(fileid),    file = ostr)
        print("#ifdef LW_FILE",                            file = ostr)
        print("#undef LW_FILE",                            file = ostr)
        print("#endif",                                    file = ostr)
        print("#define LW_FILE LWLOG_FILEID",              file = ostr)
        print("#ifdef LW_FILE_STR",                        file = ostr)
        print("#undef LW_FILE_STR",                        file = ostr)
        print("#endif",                                    file = ostr)
        print("#define LW_FILE_STR \"{}\"".format(fileid), file = ostr)

######################################################################
# Main program
#
def Main(prog, argv):
    # Parse the command line
    #
    parser = argparse.ArgumentParser(
            prog = prog,
            description = __doc__.strip(),
            formatter_class = argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--header",
            help = "Header file to generate from fileids.txt.")
    parser.add_argument("--root", default = os.lwrdir,
            help = "When writing fileids.txt, all source files are relative to this dir.")
    parser.add_argument("fileids",
            help = "The name of the fileids.txt file.")
    args = parser.parse_args(argv)

    # Generate the fileids.txt or g_<filename>-lwlog.h header file
    #
    if args.header:
        GenHeader(args)
    else:
        GenFileids(args)
    return 0

######################################################################
# Wrapper around main program that handles error messages (Exceptions)
# and control-C interrupts
#
if __name__ == "__main__":
    prog = os.path.basename(sys.argv[0])
    argv = sys.argv[1:]
    try:
        rc = Main(prog, argv)
    except Exception as err:
        if str(err):
            print("{}: {}".format(prog, err))
        rc = 1
    except KeyboardInterrupt:
        print("")
        rc = 1
    sys.exit(rc)
