# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END


# PURPOSE:
# Takes an the files in an updated Pvsstatvals/ directory  and merge it's files'
# changes into an older one. (files are generally of form ausvrlXXXX.js)
# Overwrites the old file unless an output file is provided. Unfortunately
# requires python2.6+ for the json library.

import optparse, shutil, json, sys, os, re
from textwrap import dedent
from json import encoder 

#Strings for --help
usage="python merge_ev.py [-h] NEW_DIR OLD_DIR [-o OUTPUT_DIR]"
description="""\
 Takes an updated directory of ausvrlXXXX.js files and merge changes into an older set.
 Overwrites the old files unless an output file is provided. NEW_DIR is the
 directory that contains updated versions of some machine files. OLD_DIR is 
 where you are merging the changes into. Note that you all files in NEW_DIR must 
 have corresponding files (of the same name) in OLD_DIR.  This is probably diag/mods/gpu/js/pvsstatvals/.
 It backs up and overwrites the files in OLD_DIR unless an alternate output is provided with -o.
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
p.add_option('-o', action='store', dest='output_dir', default="",
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
new_dir = args[0]
old_dir = args[1]

#Excellent. Load the JSON
new_machines = {}
js_match = "(ausvrl[0-9]+)\.js"
for subdir, dirs, files in os.walk(new_dir):
    for filename in files:
        match = re.match(js_match, filename)
        if match:
            with open(os.path.join(new_dir, filename), 'r') as f:
                inputstr = f.read()
                #remove "TestEVs[machine_name] = "
                inputstr = inputstr[inputstr.index('{'):]
                jsondata = json.loads(inputstr)
                new_machines[match.group(1)] = jsondata

old_machines = {}
for subdir, dirs, files in os.walk(old_dir):
    for filename in files:
        match = re.match(js_match, filename)
        if match:
            with open(os.path.join(old_dir, filename), 'r') as f:
                inputstr = f.read()
                #remove "TestEVs[machine_name] = "
                inputstr = inputstr[inputstr.index('{'):]
                jsondata = json.loads(inputstr)
                old_machines[match.group(1)] = jsondata

#Make a backup if no alternate output file.
if options.output_dir == "":
        for machine in list(old_machines.keys()):
            shutil.copy(os.path.join(old_dir, machine + '.js'),
                        os.path.join(old_dir, machine + '.js.bak'))

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

for machine in list(new_machines.keys()):
    merge(new_machines[machine], old_machines[machine])

#Output now.
output_dir = options.output_dir if options.output_dir != "" else old_dir
#make the output_dir if needed
if not os.path.isdir(output_dir):
    os.makedirs(output_dir)
for machine in list(old_machines.keys()):
    with open(os.path.join(output_dir, machine + '.js'), 'wt') as f:
        #Still no lwrlies-get-own-line support. :(
        jsonstring = ('TestEVs["%s"] = ' % machine) + json.dumps(old_machines[machine], sort_keys = True, indent=4)
        f.write(jsonstring)

#It's over!
