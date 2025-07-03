# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END


# PURPOSE:
# Takes an updated PvsStatExp.js file and merge changes into an older one.
# Overwrites the old file unless an output file is provided. Unfortunately
# requires python2.6+ for the json library.

import optparse, shutil, json, sys, os, re
from textwrap import dedent
from json import encoder 

#Strings for --help
usage="python merge_ev.py [-h] NEW_FILE OLD_FILE [-o OUTPUT_FILE]"
description="""\
 Takes an updated PvsStatExp.js file and merge changes into an older one.
 Overwrites the old file unless an output file is provided. NEW_FILE is the
 updated file. OLD_FILE is where you are merging the changes into. This is
 probably diag/mods/gpu/js/pvsstatexp.js. It backs up and overwrites OLD_FILE
 unless an alternate output is provided with -o.
"""
epilog="\n  Best of luck. Code on!\n\n"

#Optparse automatically formats its output for you, and deletes pesky newlines.
#This is a bit bothersome, so it is being overridden:
class RawDescParser(optparse.OptionParser):
    def format_epilog(self, formatter):
        return self.epilog
    def format_description(self, formatter):
        return self.description

#This is the only way to have python json print truncated floats. 
encoder.FLOAT_REPR = lambda o: format(o, '.2f') 

#Make parser
p = RawDescParser(usage=usage, description=description, epilog=epilog)
p.add_option('-o', action='store', dest='output_file', default="",
    help="instead of overwriting the OLD_FILE, output to this filepath.")

#If no arguments are provided, print the help page.
if len(sys.argv)==1:
    p.print_help()
    sys.exit()

#PARSE ARGS! ALL SYSTEMS GO!
(options, args) = p.parse_args()
if len(args)!=2:
    print(dedent("""
    ---------------------------------------------------------
    ERROR! Number of non-optional args should be 2. Found %d.
    ---------------------------------------------------------
    """ %(len(args),)))
    p.print_help()
    sys.exit()

#Get filepaths from arguments.
new_file = args[0]
old_file = args[1]

#Excellent. Load the JSON
with open(new_file) as f:
    inputstr = f.read()
    #remove "var TestEVs = "
    inputstr = inputstr[inputstr.index('{'):]
    new_j = json.loads(inputstr)
with open(old_file) as f:
    inputstr = f.read()
    #remove "var TestEVs = "
    inputstr = inputstr[inputstr.index('{'):]
    old_j = json.loads(inputstr)

#Make a backup if no alternate output file.
if options.output_file == "":
    shutil.copy(old_file,old_file + ".bak")

#Relwrsively copy entries from new into old, overwriting.
def merge(new, old):
    for i in list(new.keys()):
        if i in old and isinstance(new[i], dict):
            merge(new[i], old[i])
        else:
            old[i] = new[i]
    #If Priority tags are removed, merge that
    for i in list(old.keys()):
        if i not in new and i == "Priority" and isinstance(old[i], str):
            del old[i]

merge(new_j, old_j)

#Output now.
output_filepath = options.output_file if options.output_file != "" else old_file
with open(output_filepath, 'wt') as f:
    jsonstring = "var TestEVs = " + json.dumps(old_j, sort_keys = True, indent=4)
    f.write(jsonstring)

#It's over!
