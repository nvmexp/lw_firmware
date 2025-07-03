#!/usr/bin/python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END

# PURPOSE:
# Takes a directory of MODS performance stat .jso logs and
#     extracts all the info about one test and makes a CSV file.
#     This is super useful for making nice graphs in excel.
#     Requires python 2.6+ for the json library.

import csv, json, sys, os, re
from textwrap import dedent

#Make sure the number of arguments is happy. 
if len(sys.argv) > 4 or len(sys.argv) < 3:
    usage = """\
    Takes a directory of MODS performance stat .jso logs and
        extracts all the info about one test and makes a CSV file.
        This is super useful for making nice graphs in excel.

    Usage: python %s path/to/stat/logs/ test_number where/to/output/CSV/file.csv
           python %s path/to/stat/logs/ test_number

    (Second form defaults to ./stat.csv)
    """ % (sys.argv[0], sys.argv[0])
    print(dedent(usage))
    sys.exit()

#Get filepaths and test number from args.
srcdir = sys.argv[1]
testnum = 0
try:
    testnum = int(sys.argv[2],10)
except ValueError:
    print("Your inputted test number ('%s') was not a number." % sys.argv[2])
    sys.exit()
dest = sys.argv[3] if len(sys.argv) == 4 else "stat.csv"

#Compile regular expressions
isstatjso = re.compile('^.*stat[^\\/]*\.jso$')
ispfunder = re.compile('^pf_.*$')

#Stats is all the data collected. Metrics is a list of the different metrics
#seen thus far. This is so that we can have a complete log even if some tests
#have metrics A, B, and C, while others have C, D, and E.
stats = {}
metrics = []

print("Reading from %s" % srcdir)
for filename in os.listdir(srcdir):
    if(isstatjso.match(filename)): #Contains 'stat' and ends in .jso
        #filename is just the filename. need the full path:
        fullfile = os.path.join(srcdir,filename)
        print("   Opening %s" % fullfile)
        with open(fullfile) as f:
            #get json data
            jsondata = json.loads(f.read())
            #tests might get run more than once. so we have this suffix:
            index_number_suffix = 0
            #get each metric and add it to stats
            for tag in jsondata[0]:
                if tag["__tag__"] == "exit" and tag["tnum"] == testnum:
                    recordeds = {} #Dictionary will get added to stats
                    for subtag in tag:
                        if ispfunder.match(subtag): #if it is 'pf_...'
                            if subtag not in metrics:
                                metrics.append(subtag)
                            recordeds[subtag] = tag[subtag]
                    recordeds["InfPt"] = tag["InfPt"]
                    #Add to stats
                    stats["%s:%d" % (filename,index_number_suffix)] = recordeds
                    index_number_suffix += 1
                    
print("Printing CSV to %s" % dest)
with open(dest, 'wt') as f:
    writer = csv.writer(f) #7
    writer.writerow(["Test", testnum])
    #Top row is Job, InfPt, <All metrics catalogued>
    writer.writerow(["Job:#", "InfPt"] + metrics)
    for job in sorted(stats):
        #Print line of data. the Map() turns the metric catalogue array
        #into an array of the corresponding data for a particular job.
        writer.writerow([job,stats[job]["InfPt"]] + 
            [stats[job][metric] if metric in stats[job] else "" for metric in metrics])

print("Done, thank you for waiting!")
