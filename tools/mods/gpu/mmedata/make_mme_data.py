#! /usr/bin/elw python
# 
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2009,2012,2017 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

# Generate MME macro methods header from the HW tree.
# This file should be run within the gpu/mmedata directory
#

import os, sys, getopt, shutil, datetime

# Add data from the input file to the mme data file
#
# The format of the input file (which lives in the hardware tree) is a
# sequence of class/method pairs with one pair per line, comma seperated.
# C style comments are also present in the file
#
# Each of the class/method pairs is translated into a macro call for 
# inclusion in an appropriate MODS file
def AddMmeData(outputFile, inputFileName, macroName):
    inputFile = open(inputFileName, 'r')
    for line in inputFile:
        line = line.strip()
        if not line.startswith("//"):
            mmeClassMethod = [val.strip() for val in line.split(',') if len(val)]
            if len(mmeClassMethod) == 2:
                outputFile.write(macroName + "(" + mmeClassMethod[0] + ", " + mmeClassMethod[1] + ");\n")
            if len(mmeClassMethod) == 3:
                outputFile.write(macroName + "(" + mmeClassMethod[0] + ", " + mmeClassMethod[1] + ");\n")
    inputFile.close()

# Print usage string
def usage ():
    print("Usage: make_mme_data -c <chip> -h <hw path> [-l changelist]")

def main ():
    # Parse the options forom the command line
    try:
        opts, args = getopt.getopt(sys.argv[1:], "c:h:l:", ["chip=", "hwtree=", "changelist="])
    except getopt.GetoptError as err:
        print(str(err))
        usage()
        sys.exit(2)
    chip = ""
    hwtree = ""
    changelist = ""
    for o, a in opts:
        if o in ("-c", "--chip"):
            chip = a
        elif o in ("-h", "--hwtree"):
            hwtree = a
        elif o in ("-l", "--changelist"):
            changelist = a
        else:
            usage()
            sys.exit(2)
    if chip == "" or hwtree == "":
        usage()
        sys.exit(2)

    # Output the data to a temp file
    modsMmeMethodsTmp = os.path.join("./", chip + "mme.h.tmp")

    if not os.path.isabs(hwtree):
        hwtree = abspath(hwtree)
    chipDir = os.path.join(hwtree, "include", chip)
    if not os.path.isfile(os.path.join(chipDir, "mme_shadow_methods")):
        chipDir = os.path.join(hwtree, "include", "gr", "fe", chip)
    mmeMethods = os.path.join(chipDir, "mme_shadow_methods")
    mmeSharedMethods = os.path.join(chipDir, "mme_shared_shadow_always_methods")

    try:
        copyrightYear = str(datetime.date.today().year)
        mmeFile = open(modsMmeMethodsTmp, 'w')
        mmeFile.write("/* _LWRM_COPYRIGHT_BEGIN_\n");
        mmeFile.write("*\n")
        mmeFile.write(" * Copyright " + copyrightYear + " by LWPU Corporation.  All rights reserved.  All\n")
        mmeFile.write(" * information contained herein is proprietary and confidential to LWPU\n")
        mmeFile.write(" * Corporation.  Any use, reproduction, or disclosure without the written\n")
        mmeFile.write(" * permission of LWPU Corporation is prohibited.\n")
        mmeFile.write(" *\n")
        mmeFile.write(" * _LWRM_COPYRIGHT_END_\n")
        mmeFile.write(" */\n\n")
        mmeFile.write("/* This file is autogenerated by make_mme_data.py.  Do not edit */\n")
        mmeFile.write("// AUTO GENERATED -- DO NOT EDIT\n\n")
        AddMmeData(mmeFile, mmeMethods, "ADD_MME_SHADOW_METHOD")
        AddMmeData(mmeFile, mmeSharedMethods, "ADD_MME_SHARED_SHADOW_METHOD")
        mmeFile.close()
    except IOError as err:
        print(str(err))
        sys.exit(3)

    # Mme methods file for the submitted CL
    modsMmeMethods = os.path.join("./", chip + "mme.h")

    # Edit into a specific CL if requested
    p4opts = ""
    if len(changelist):
        p4opts += "-c" + changelist + " "

    # if the MODS file already exists add it, otherwise edit it
    if os.path.isfile(modsMmeMethods):
        if os.system("p4 edit " + p4opts + modsMmeMethods):
            print("Unable to p4 edit output file: ", modsMmeMethods)
        else:
            shutil.copy(modsMmeMethodsTmp, modsMmeMethods)
    else:
        os.rename(modsMmeMethodsTmp, modsMmeMethods)
        if os.system("p4 add " + p4opts + modsMmeMethods):
            print("Unable to p4 add output file: ", modsMmeMethods)

    if os.path.isfile(modsMmeMethodsTmp):
        os.remove(modsMmeMethodsTmp)

if __name__ == "__main__":
    main()
