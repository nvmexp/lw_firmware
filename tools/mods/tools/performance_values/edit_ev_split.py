# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2016 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END


# PURPOSE:
# For mass changes to the ausvrlXXXX.js files!
# Unfortunately requires python 2.6+ for the json library

import optparse, json, sys, re, os 
from textwrap import dedent
from json import encoder 

#Helper function checks if something is an int.
#Abbreviate to isn't() to make things confusing.
def isInt(i):
    try:
        int(i)
        return True
    except ValueError:
        return False

#Strings for --help
usage="""\
python edit_ev.py [-h] [-t INCLUDED_TEST] [-T EXCLUDED_TEST]
                         [-m INCLUDED_MACHINE] [-M EXCLUDED_MACHINE]
                         [-s INCLUDED_METRIC] [-S EXCLUDED_METRIC]
                         [-p INCLUDED_PSTATE] [-P EXCLUDED_PSTATE]
                         [-o OUTPUT_DIR] [--gpugen] [--gputest]
                         [--meanadd MEANADD] [--devadd DEVADD]
                         [--meanset MEANSET] [--devset DEVSET]
                         [--meanmult MEANMULT] [--devmult DEVMULT]
                         [--priority PRIORITY] [--delete_priority]
                         [--delete] TARGET"""
description="""  This script is for making mass changes to the ausvrlXXXX.js files!

  Describe the entries you want to modify using -t, -m, -s, -p, 
  --gpugen, and --gputest. The uppercase version are negative. Then choose a 
  command and supply the input filepath, etc. This script is not very useful
  for /adding/ data. For that, use make_ev.py and merge_ev.py."""
epilog="""
Examples: 
  python %s -t 8 -M HAL -s ElapsedTime --gputest --meanadd 123.0 machine_dir
    For all ElapsedTimes for test 8's gputest that aren't on machine HAL,
    with any pState, add 123.0 to the mean.

  python %s -T 1 -T 2 --deviationmult 4.0 machine_dir 
    For every metric except those associated with tests one and two, multiply
    the deviation by 4.

Also:
  Some tests (154?) have different parts, both refered to with the same number
  in mods. This poses a problem because they have different metrics. Generally 
  you should just ignore this and put the test number for -t and -T. But if you
  know what you're doing and want to change 154_LwdaL2Mats but not
  154_LwdaL2TestOneSlice, write the full name in with the underscore like that,
  just as it appears in in that ausvrlXXXX.js or stat.jso.

  By default, files are outputted to "./ev_out/". Change to whatever you like. Also,
  if no commands are entered, enters 'debug mode' and prints out all items
  matching the filters. Use this to make sure your args are correct, maybe.
  Your hour of destiny has come. For the sake of us all: Go bravely with MODS!
  """ %  (sys.argv[0], sys.argv[0])

#This is the only way to have python json print truncated floats. 
encoder.FLOAT_REPR = lambda o: format(o, '.2f') 

#Compile our regular expressions
isinfpt = re.compile('^\d+\.[a-zA-Z]+$')

#Array to put warnings in. Displayed at the end because the program prints a
#whole lot of stuff when in debug mode.
warnings = []

#Optparse automatically formats its output for you, and deletes pesky newlines.
#Overriding that here, so the above strings can be used.
class RawDescParser(optparse.OptionParser):
    def format_epilog(self, formatter):
        return self.epilog
    def format_description(self, formatter):
        return self.description

#Make parser
p = RawDescParser(usage=usage, description=description, epilog=epilog)
p.add_option('-t', action='append', dest='included_test', help='include this test', default=[])
p.add_option('-T', action='append', dest='excluded_test', help='exclude this test', default=[])
p.add_option('-m', action='append', dest='included_machine', help='include this machine', default=[])
p.add_option('-M', action='append', dest='excluded_machine', help='exclude this machine', default=[])
p.add_option('-s', action='append', dest='included_metric', help='include this metric', default=[])
p.add_option('-S', action='append', dest='excluded_metric', help='exclude this metric', default=[])
p.add_option('-p', action='append', dest='included_pstate', help='include this pstate', default=[])
p.add_option('-P', action='append', dest='excluded_pstate', help='exclude this pstate', default=[])
p.add_option('-o', action='store', dest='output_dir', help='output results to this directory', default="./ev_out/")
p.add_option('--gpugen', action='store_true', default=False, help='apply only to gpugen')
p.add_option('--gputest', action='store_true', default=False, help='apply only to gputest')
p.add_option('--meanadd', action='store', default=0.0, help='add this to mean of each metric enumerated', type=float)
p.add_option('--devadd', action='store', default=0.0, help='add this to deviation of each metric enumerated', type=float)
p.add_option('--meanmult', action='store', default=1.0, help='multiply mean of each metric enumerated by this', type=float)
p.add_option('--devmult', action='store', default=1.0, help='multiply deviation of each metric enumerated by this', type=float)
p.add_option('--meanset', action='store', help='set the mean of each metric enumerated to this', type=float)
p.add_option('--devset', action='store', help='set the deviation of each metric enumerated to this', type=float)
p.add_option('--priority', action='store', default="", help='change the priority of the metric. Fail, Warn, or Skip.')
p.add_option('--delete_priority', action='store_true', default=False, help='remove any priority override from the metric.')
p.add_option('--delete', action='store_true', default=False, help='remove whole entry for all metrics enumerated')

#Make it print --help when no args are provided. That seems like a good idea.
if len(sys.argv)==1:
    p.print_help()
    sys.exit()

#PARSE ARGS! ALL SYSTEMS GO!
(options, args) = p.parse_args()
if len(args)==0:
    print("TARGET not found. As in, no input directory name was supplied.")
    sys.exit()
if len(args)!=1:
    print(dedent("""
    Too many TARGETs found. This means you probably have
    something taking an argument that doesn't need it.
    Here are the TARGETs found:"""))
    for a in args:
        print("    %s" % (a,))
    sys.exit()
#Get ye directory!
target_directory = args[0]

#WARNING if pstate is weird format:
for infpt in options.included_pstate + options.excluded_pstate:
    if not isinfpt.match(infpt):
        warnings.append("WARNING! Funny (pState) name: %s\n(Expecting '<number>.<string>' format)" % infpt)

#WARNING if --priority is something strange
if(options.priority != "Fail" and
   options.priority != "Warn" and
   options.priority != "Skip" and
   options.priority != "" ):
    warnings.append(dedent("""\
    WARNING! You supplied '%s' as your argument for --priority. This isn't
    the standard "Fail", "Warn", or "Skip", so unless there has been some
    change, it will not be picked up by MODS. Have a nice day.""" % (options.priority)))

#WARNING if more than one command is activated on one datum
mean_commandnum = 0
dev_commandnum = 0
pri_commandnum = 0
mean_commandnum += 1 if options.meanadd != 0.0 else 0
mean_commandnum += 1 if options.meanmult != 1.0 else 0
mean_commandnum += 1 if options.meanset != None else 0
dev_commandnum += 1 if options.devadd != 0.0 else 0
dev_commandnum += 1 if options.devmult != 1.0 else 0
dev_commandnum += 1 if options.devset != None else 0
pri_commandnum += 1 if options.delete_priority else 0
pri_commandnum += 1 if options.priority != "" else 0
if mean_commandnum > 1 or dev_commandnum > 1 or pri_commandnum > 1:
    #You have shamed our family. You must go.
    warnings.append(dedent("""\
    WARNING! You just applied more than one command to either the means or the
      deviations. Or maybe did --priority and --delete_priority at once!
      That's undefined behavior! Please don't do that! Think of the children!
      You don't even know what order it's doing it in!"""))

#Will output files? Otherwise, be in debug mode.
fileaction = (options.meanadd  != 0.0 or options.devadd  != 0.0 or
    options.meanmult != 1.0 or options.devmult != 1.0 or
    options.meanset != None or options.devset != None or
    options.priority != "" or options.delete_priority or
    options.delete)

#Get json data from each file in directory
machines = {}
js_match = "(ausvrl[0-9]+)\.js"
for subdir, dirs, files in os.walk(target_directory):
    for filename in files:
        match = re.match(js_match, filename)
        if match:
            with open(os.path.join(target_directory, filename), 'r') as f:
                inputstr = f.read()
                #remove "TestEVs[machine_name] = "
                inputstr = inputstr[inputstr.index('{'):]
                jsondata = json.loads(inputstr)
                machines[match.group(1)] = jsondata

#Go through and apply the commandline filters!
#machines[machine][infpt][gentest][test][metric]
#note that machines[machine] corresponds to the
#data from a single ausvrlXXXX.js file
for machine in machines.keys():
   if ((not options.included_machine or
        machine in options.included_machine) and
        machine not in options.excluded_machine):
        
        #Proceed if x is in positive filters and not in negative filters or
        #if there are no positive filters and x is not in negative filters.
        for infpt in machines[machine].keys():
            if ((not options.included_pstate or
                infpt in options.included_pstate) and
                infpt not in options.excluded_pstate):

                #Proceed if neither --gputest or --gpugen, or if only one, and
                #it matches gentest.
                for gentest in machines[machine][infpt].keys():
                    if ((options.gputest == False or gentest == "gputest") and
                        (options.gpugen  == False or gentest == "gpugen")):

                        #Test number/name is weird
                        #Consider just number and full name
                        for test in machines[machine][infpt][gentest].keys():
                            if ((not options.included_test or
                                test in options.included_test or
                                test.split("_")[0] in options.included_test) and
                                test not in options.excluded_test and
                                test.split("_")[0] not in options.excluded_test):

                                for metricname in machines[machine][infpt][gentest][test].keys():
                                    if ((not options.included_metric or
                                        metricname in options.included_metric) and
                                        metricname not in options.excluded_metric):

                                        #Shorter name.
                                        metric = machines[machine][infpt][gentest][test][metricname]

                                        #Debug mode print
                                        if fileaction == False:
                                            print(("j[%s][%s][%s][%s][%s]" %
                                            (machine,infpt,gentest,test,
                                            metricname)))
                                            for datum in metric:
                                                print("   %s: %s" % (datum,
                                                metric[datum]))
                                            print("")

                                        #Apply commands!
                                        if "Mean" in metric:
                                            if options.meanset != None:
                                                metric["Mean"] = options.meanset
                                            metric["Mean"] *= options.meanmult
                                            metric["Mean"] += options.meanadd

                                        if "Deviation" in metric:
                                            if options.devset != None:
                                                metric["Deviation"] = options.devset
                                            metric["Deviation"] *= options.devmult
                                            metric["Deviation"] += options.devadd

                                        if "PositiveDev" in metric:
                                            if options.devset != None:
                                                metric["PositiveDev"] = options.devset
                                            metric["PositiveDev"] *= options.devmult
                                            metric["PositiveDev"] += options.devadd

                                        if "NegativeDev" in metric:
                                            if options.devset != None:
                                                metric["NegativeDev"] = options.devset
                                            metric["NegativeDev"] *= options.devmult
                                            metric["NegativeDev"] += options.devadd

                                        if ("Min" in metric and
                                            "Max" in metric):
                                            center = (metric["Max"] + metric["Min"])/2.0
                                            deviation = metric["Max"] - center
                                            #apply to imaginary
                                            if options.meanset != None:
                                                center = options.meanset
                                            center *= options.meanmult
                                            center += options.meanadd
                                            if options.devset != None:
                                                deviation = options.devset
                                            deviation *= options.devmult
                                            deviation += options.devadd
                                            #apply to actual
                                            metric["Max"] = center + deviation
                                            metric["Min"] = center - deviation

                                            #WARNING assumes centered average
                                            mmwarning = ("WARNING! Editing a " +
                                            "metric that uses Min/Max format." +
                                            " Assumes centered mean.")
                                            #If there are many Min/Maxes, this
                                            #could become annoying fast, so:
                                            if mmwarning not in warnings:
                                                warnings.append(mmwarning)

                                        if options.priority != "":
                                            metric["Priority"] = options.priority

                                        if (options.delete_priority == True and
                                            "Priority" in metric):
                                            del metric["Priority"]

                                        if options.delete == True:
                                            del machines[machine][infpt][gentest][test][metricname]
                                    
                                    #Remove empty nodes
                                if not machines[machine][infpt][gentest][test]:
                                    del machines[machine][infpt][gentest][test]
                        if not machines[machine][infpt][gentest]:
                            del machines[machine][infpt][gentest]
                if not machines[machine][infpt]:
                    del machines[machine][infpt]
        if not machines[machine]:
            del machines[machine]

#Output the new and improved Expected Value file
if fileaction == True:
    #make sure the target output directory exists first
    if not os.path.isdir(options.output_dir):
        os.makedirs(options.output_dir)
    for machine in machines.keys():
        with open(os.path.join(options.output_dir, machine + '.js'), 'wt') as f:
            #Still no lwrlies-get-own-line support. :(
            jsonstring = ("TestEVs[%s] = " % machine) + json.dumps(machines[machine], sort_keys = True, indent=4)
            f.write(jsonstring)

#Print warnings
for w in warnings:
    print(w)
