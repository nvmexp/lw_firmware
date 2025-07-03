#! /usr/bin/elw python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

import sys
import os
import zipfile
import argparse

def ZipPath(filename, strip):
    for path in strip:
        if filename.startswith(path):
            idx = len(path)
            while filename[idx] == '/' or filename[idx] == '\\':
                idx += 1
            return filename[idx:]
    return filename

def createzip(outzipfile, srcfiles, strip=[]):
    zf = zipfile.ZipFile(outzipfile, "w", zipfile.ZIP_DEFLATED)
    for filename in srcfiles:
        if os.path.isdir(filename):
            for root, dirs, files in os.walk(filename):
                for file in files:
                    filepath = os.path.join(root, file)
                    zf.write(filepath, ZipPath(filepath, strip))
        else:
            zf.write(filename, ZipPath(filename, strip))
    zf.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("outzipfile")
    parser.add_argument("--strip", dest="strip", action="append")
    parser.add_argument("files", nargs="+")
    args = parser.parse_args(sys.argv[1:])

    sys.exit(createzip(args.outzipfile, args.files, args.strip))
