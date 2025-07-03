#!/usr/bin/python

# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END


# PURPOSE:
#For taking a directory of MODS performance stat .jso logs and
#    turning them into an EV table that can be chunked into
#    diag/mods/gpu/js/pvsstatvals/ausvrlXXXX.js files. This is best done with
#    //sw/mods/scripts/performance_values/merge_ev.py.
#    Unfortunately, requires python 2.6+ for json library.

import json, sys, os, re
from textwrap import dedent
from json import encoder 

#FOR EV CALLWLATION:
#Right now it is Min - Max with a 15% buffer on each end.
#So if Min = 100 and Max = 200, our EV would be 85 - 230.
bufferSpace = 0.20
#Bandwidth should be more stable:
bufferSpaceGbPerSec = 0.1
#Power should be more stable:
powerSpace = 0.05
#Flops should be stable with deviation ~10%
gflopsSpace = 0.1
#Also for ElapsedTime, the deviation must be at least this:
minTimeDeviation = 0.1

#Make sure the number of arguments is happy. 
if len(sys.argv) > 3 or len(sys.argv) < 2:
    usage = """\
    For taking a directory of MODS performance stat .jso logs and
        turning them into an EV table that can be chunked into
        diag/mods/gpu/js/pvsstatvals/ausvrlXXXX.js. This is best done with
        //sw/mods/scripts/performance_values/merge_ev_split.py.

    Usage: python %s path/to/stat/logs/ where/to/output/EV/directory/
           python %s path/to/stat/logs/ 

    (Second form defaults to ./exp/)
    """ % (sys.argv[0], sys.argv[0])
    print(dedent(usage))
    sys.exit()

#This is the only way to have python json print truncated floats. 
encoder.FLOAT_REPR = lambda o: format(o, '.2f') 

#Grab filepaths
srcdir = sys.argv[1]
dest = sys.argv[2] if len(sys.argv) == 3 else "./exp/"

#Make all the regular expressions for later:
isstatjso = re.compile('^.*stat[^\\/]*\.jso$')
ispfunder = re.compile('^pf_.*$')
getgpugen = re.compile('gpugen\.js')
getgputest = re.compile('gputest\.js')
getmachine = re.compile('-pvs_machine (\S+)')

#Here we will keep our aggregated data. Eventually this will become the EV table
#stats[machine][InfPt][gentest][test][metric] = array of data
stats = {} 

print("Reading from %s" % srcdir)
#For each file in the directory that is a stat.jso file
for filename in os.listdir(srcdir):
    if isstatjso.match(filename): #contains stat in the filename, ends in .jso
        #Get the full filepath for opening it.
        fullfile = os.path.join(srcdir,filename)
        print("    Opening %s" % fullfile)
        with open(fullfile) as f:
            #Extract json data.
            try:
                jsondata = json.loads(f.read())
            except ValueError:
                print("        Error opening %s file!" % fullfile)
            machine_name = ""
            gpugen_gputest = ""
            last_infpt = ""
            #Scan through...
            for tag in jsondata[0]:
                #First collect test information from sw tag
                if tag["__tag__"] == "sw":
                    match = getmachine.search(tag["command_line"])
                    if match:
                        machine_name = match.group(1)
                    if getgputest.search(tag["command_line"]):
                        gpugen_gputest = "gputest"
                    elif getgpugen.search(tag["command_line"]):
                        gpugen_gputest = "gpugen"
                elif tag["__tag__"] == "exit":
                    #If sw tag hasn't been read yet, fail.
                    if machine_name == "" or gpugen_gputest == "":
                        print("        Error! No sw tag found before first exit tag.")
                        sys.exit()
                    infpt = ""
                    #Some background-run tests don't get the InfPt tag. :(
                    try:
                        infpt = tag["InfPt"]
                        last_infpt = infpt
                    except KeyError:
                        if last_infpt == "":
                            print("        Error! Missing InfPt data.")
                            sys.exit()
                        infpt = last_infpt
                    tname = str(tag["tnum"]) + "_" + tag["tname"]
                    #Add nodes to stats if necessary so we can insert datum
                    if machine_name not in stats:
                        stats[machine_name] = {}
                    if infpt not in stats[machine_name]:
                        stats[machine_name][infpt] = {}
                    if gpugen_gputest not in stats[machine_name][infpt]:
                        stats[machine_name][infpt][gpugen_gputest] = {}
                    if tname not in stats[machine_name][infpt][gpugen_gputest]:
                        stats[machine_name][infpt][gpugen_gputest][tname] = {}
                    #Add every pf_<Performance Stat> datum now, please.
                    for subtag in tag:
                        if ispfunder.match(subtag):
                            metric = subtag[3:]
                            # Careful: subtag is the name of a key in tag.
                            #          metric is the actual name of the metric
                            #          with the pf_ removed. Use metric as an
                            #          index in stats, and subtag as an index in
                            #          tag.
                            if metric not in stats[machine_name][infpt][gpugen_gputest][tname]:
                                stats[machine_name][infpt][gpugen_gputest][tname][metric] = []
                            #Add performance datum to leaf array.
                            stats[machine_name][infpt][gpugen_gputest][tname][metric].append(tag[subtag])

#Now do something with all the data
for machine in stats:
    for infpt in stats[machine]:
        for gentest in stats[machine][infpt]:
            for test in stats[machine][infpt][gentest]:
                for metric in stats[machine][infpt][gentest][test]:
                    #Callwlate the EVs.

                    # Lwrrently we have a big dictionary tree with an array of
                    # data at the leaves. Here we have a formula that does some
                    # numbers and then replaces the array leaf with an object
                    # containing "Mean" and "Deviation"; "Min" and "Max"; or
                    # "Mean", "PositiveDev", and "NegativeDev". The data tree
                    # becomes the EV tree. 

                    #Get array into smaller-named variable for more easy access.
                    datalist = stats[machine][infpt][gentest][test][metric]

                    newEvObject = {}

                    #callwlate
                    if metric == "NumCrcs":
                        newEvObject["Min"] = min(datalist)
                        newEvObject["Max"] = max(datalist)
                    elif metric == "ElapsedTime":
                        low_margin = bufferSpace*min(datalist)
                        if low_margin < minTimeDeviation:
                            low_margin = minTimeDeviation
                        low  = min(datalist) - low_margin
                        if low < 0:
                            low = 0

                        high_margin = bufferSpace*max(datalist)
                        if high_margin < minTimeDeviation:
                            high_margin = minTimeDeviation
                        high = max(datalist) + high_margin

                        newEvObject["Min"] = low
                        newEvObject["Max"] = high
                    elif metric == "GbPerSec":
                        newEvObject["Min"] = (1-bufferSpaceGbPerSec)*min(datalist)
                        newEvObject["Max"] = (1+bufferSpaceGbPerSec)*max(datalist)
                    elif metric == "AveragePowermW":
                        newEvObject["Min"] = (1-powerSpace)*min(datalist)
                        newEvObject["Max"] = (1+powerSpace)*max(datalist)
                    elif metric == "GFlops":
                        newEvObject["Min"] = (1-gflopsSpace)*min(datalist)
                        newEvObject["Max"] = (1+gflopsSpace)*max(datalist)
                    else:
                        newEvObject["Min"] = (1-bufferSpace)*min(datalist)
                        newEvObject["Max"] = (1+bufferSpace)*max(datalist)

                    #Overwrite with EV data
                    stats[machine][infpt][gentest][test][metric] = newEvObject

print("Printing EVs to %s" % dest)
#make target directory if it doesn't already exist
if not os.path.isdir(dest):
    os.makedirs(dest)
for machine in stats:
    machine_file = os.path.join(dest, "%s.js" % machine)
    with open(machine_file, 'wt') as f:
        print("Writing %s" % machine_file)
        #There is no option for lwrlies getting their own line. This makes me sad.
        jsonstring = ('TestEVs["%s"] = ' % machine) + json.dumps(stats[machine], sort_keys = True, indent=4)
        f.write(jsonstring)
