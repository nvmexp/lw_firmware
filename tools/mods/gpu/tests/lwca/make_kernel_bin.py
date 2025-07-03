#! /usr/bin/elw python
#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# Compile the LWCA gpu code to lwbin modules that can
# be loaded by mods at runtime.
#
#  NOTE : This python script is *Linux ONLY*

import getopt
import os
import fcntl
import re
import subprocess
import sys
import urllib.request, urllib.error, urllib.parse

# These variables must be provided by the command line, lwdaarchs.inc
# uses the settings provided in the modular branch file to call this script.
g_DVSReleaseToolkitBranch = ""
g_DevToolkitPath = ""
g_RelToolkitPath = ""
g_ToolkitBuildCfg = 'release'
g_DriverRoot = ""
g_ToolchainPath = ""
g_MakeJobs = "1"

g_Python3 = sys.exelwtable
g_Python2 = os.elwiron.get("PYTHON")  or "python2"
g_Perl    = os.elwiron.get("PERL")    or "perl"
g_Unifdef = os.elwiron.get("UNIFDEF") or "unifdef"

g_DVSToolkitBranch = "gpu_drv_module_compiler%20Debug%20Linux%20AMD64%20GPGPU%20COMPILER"
g_UseDevLwcc = False
g_UseArmCompiler = False
g_MultiFormat = False
g_MerlwryFlow = False
g_UseLibsToolkit = False
g_ToolkitArtifacts = os.elwiron.get("TOOLKIT_ARTIFACTS")
g_LwdartHeaders = ""
g_LwConfigPath = ""
g_CompilerVersion = ""
g_CompilerReleaseX100 = ""
g_CompilerRelease = ""

def getBuildName():
    if g_UseArmCompiler:
        return f"aarch64_Linux_{g_ToolkitBuildCfg}"
    else:
        return f"x86_64_Linux_{g_ToolkitBuildCfg}"

def getCompilerVersion(lwcc_path):
    # Search the lwcc output below for the release number * 100
    # For reference here is the string being searched.
    #lwcc: LWPU (R) Lwca compiler driver
    #Copyright (c) 2005-2012 LWPU Corporation
    #Built on Tue_Feb__5_00:49:12_CST_2013
    #Lwca compilation tools, release 5.5, V5.5.0
    global g_CompilerVersion
    global g_CompilerReleaseX100
    global g_CompilerRelease
    text = subprocess.check_output(lwcc_path,shell=True).decode("utf8");
#    rel = re.compile(r'.*release\s+(\d+(\.\d+)+).*')
    rel = re.compile(r'release\s+(\d+(\.\d+)+),\s+(V\d+(\.\d+)+)')
    m = rel.search(text);

    if m != None:
        g_CompilerReleaseX100 = str(int(float(m.group(1)) * 100))
        g_CompilerRelease = str(m.group(1))
        if m.lastindex >= 3:
            g_CompilerVersion = str(m.group(3))
        return True
    return False

def fetch_latest_package_CL(packageBuildName):
    # When debugging lwca kernels its a real pain to keep fetching newer versions for the DVS
    # binaries and rebuilding the compiler. So allow the user to set the elwrionment variable
    # REL_DVS_COMPILER_BUILD_CL and/or ARCH_DVS_COMPILER_BUILD_CL to specify the CL number
    fetchCL = None
    if g_UseDevLwcc:
        fetchCL = os.getelw('ARCH_DVS_COMPILER_BUILT_CL', fetchCL)
    else:
        fetchCL = os.getelw('REL_DVS_COMPILER_BUILT_CL', fetchCL)

    # If no CL is specified, fetch the latest CL
    QUERY_URL = "http://ausdvs.lwpu.com/Query/PackageURLForCL?"
    if fetchCL == None:
        queryURL = QUERY_URL + "&latest=true" + "&successfulPkg=true" + "&pkg=" + packageBuildName
    # Otherwise fetch the most recent CL at or before 'fetchCL'
    else:
        queryURL = QUERY_URL + "&sw_cl=" + fetchCL + "&pkg=" + packageBuildName

    print("Querying DVS package from " + queryURL)
    try:
        queryFile = urllib.request.urlopen(queryURL)
    except IOError:
        print("Error: Cannot read " + queryURL)
        sys.exit(1)
    queryStr = queryFile.read().decode("utf8")

    # FORMAT:
    # retcode,retdesc
    # packageName,packageURL
    #
    # match anything that isn't a comma or newline
    # extract the CL from the packageURL
    # use regex101.com for help if you need to modify or understand this
    parse = re.compile(r'^([^,\n]*),([^,\n]*)$\n^([^,\n]*),([^,\n]*SW_(\d+)\.0[^,\n]*)$\n', re.MULTILINE)
    result = parse.match(queryStr)
    if result == None:
        print("Error: Compiler CL query did not return well-formatted result:\n" + queryStr)
        sys.exit(1)

    retcode, retdesc, packageName, packageURL, packageCL = result.groups()
    if retcode != "SUCCESS" or packageCL == None or packageURL == None:
        print("Error: Unable to parse compiler CL query:\n" + queryStr)
        sys.exit(1)

    return packageCL, packageURL

def parse_latest_package_CL(packagesStr, packageBuildName):
    CLList = re.findall(r"SW_([0-9]+)\.0_" + packageBuildName.replace("%20", "_"), packagesStr)
    CLList.sort(reverse=True)
    if len(CLList) == 0:
        print("Error: TOOLKIT_BUILT is set but unable to find DVS package in artifacts")
        sys.exit(1)
    return CLList[0]

def fetch_and_extract_dvs_package(url, packageName, artifactsDir, extractDir):

    errorcode = cmd_run("wget -lw -nc " + url + " -P " + artifactsDir)
    downloadPath = os.path.join(artifactsDir, packageName + ".tgz")
    if errorcode:
        print("Unable to fetch DVS package. Removing partially downloaded files...")
        cmd_run("rm " + downloadPath);
        sys.exit(errorcode)
    print("Extracting DVS package...")

    cmd_str = (f"mkdir -p {extractDir} && " +
               f"tar -xzf {downloadPath} -C {extractDir} && " +
               f"cd {extractDir} && " +
               f"for file in `find . -regex '.*\.tar\|.*\.tgz'`; do tar -xf $file; done")

    errorcode = cmd_run(cmd_str)
    if errorcode:
        print("Unable to extract package properly, removing extracted files...")
        cmd_run("rm -r " + extractDir)

def fetch_toolkit_from_dvs():
    global g_ToolkitArtifacts
    artifactsDir = os.elwiron.get('LWDA_ARTIFACTS_DIR') or os.getcwd()

    # For lwbins builds use the specified branch
    if g_UseDevLwcc:
        DVSBranch = f"{g_DVSToolkitBranch}%20mods%20lwdart"
    else:
        DVSBranch = f"{g_DVSReleaseToolkitBranch}%20mods%20lwdart"

    # For ARM use aarch64 instead of AMD64
    if g_UseArmCompiler:
        DVSBranch = DVSBranch.replace("AMD64", "aarch64")

    # Use correct toolkit type
    if g_ToolkitBuildCfg == 'release':
        DVSBranch = DVSBranch.replace("Debug", "Release")
    else:
        DVSBranch = DVSBranch.replace("Release", "Debug")

    # If the toolkit has already been downloaded (TOOLKIT_BUILT is set)
    # use the latest LWCA toolkit in the artifacts directory.
    if os.getelw("TOOLKIT_BUILT"):
        packagesStr = " ".join(os.listdir(artifactsDir))
        DVSBuiltCL = parse_latest_package_CL(packagesStr, DVSBranch)
        fetchUrl = None
    # Otherwise query the url of the latest package from DVS
    else:
        DVSBuiltCL, fetchUrl = fetch_latest_package_CL(DVSBranch)

    # Set toolkit artifacts
    DVSBuild = "SW_" + DVSBuiltCL + ".0_" + DVSBranch.replace("%20", "_")
    if g_UseLibsToolkit:
        extractDir = os.path.join(artifactsDir, "mods_lwdart")
    else:
        extractDir = os.path.join(artifactsDir, DVSBuild)
    g_ToolkitArtifacts = extractDir

    # Exit early if we are using the latest local LWCA toolkit
    if fetchUrl == None:
        print("TOOLKIT_BUILT is set, using toolkit package " + extractDir)
        return True

    # Create dir if not already existing
    errorcode = cmd_run(f"mkdir -p {extractDir}")
    if errorcode:
        print(f"Unable create directory {extractDir}")
        sys.exit(errorcode)

    # Acquire lock to download and extract
    # This allows the script to be exelwted in parallel on different kernels
    try:
        fd = open(os.path.join(extractDir, f"lock"), 'w')
        fcntl.flock(fd, fcntl.LOCK_EX)

        # Fetch toolkit from DVS and extract
        print("Fetching DVS package from " + fetchUrl)
        print("extractDir: " + extractDir)
        lwcc_file = os.path.join(extractDir, f"bin/{getBuildName()}/lwcc")
        lwdart_file = os.path.join(extractDir, f"bin/{getBuildName()}/liblwdart_static.a")
        if os.path.exists(lwcc_file) and os.path.exists(lwdart_file):
            print("Toolkit package already extracted, skipping")
        else:
            fetch_and_extract_dvs_package(fetchUrl, DVSBuild, artifactsDir, extractDir)
    finally:
        fcntl.flock(fd, fcntl.LOCK_UN)
        fd.close()

def verify_compiler():
    if g_CompilerPath == None:
        return False

    lwcc_file = os.path.join(g_CompilerPath, "lwcc")
    if not os.path.exists(lwcc_file) or not os.access(lwcc_file, os.X_OK):
        return False

    return getCompilerVersion(lwcc_file + " --version")

def verify_and_fetch_toolkit():
    global g_LwdartHeaders
    global g_CompilerPath

    if g_ToolkitArtifacts == None:
        fetch_toolkit_from_dvs()
    g_LwdartHeaders = os.path.join(g_ToolkitArtifacts, "lwca/tools/lwdart")
    g_CompilerPath  = os.path.join(g_ToolkitArtifacts, f"bin/{getBuildName()}")

    lwdart_file = os.path.join(g_CompilerPath, "liblwdart_static.a")
    if not os.path.exists(lwdart_file):
        return False

    return True

def fetch_all():
    if not verify_and_fetch_toolkit():
        if g_ToolkitArtifacts != None:
            print(f"Failed to locate toolkit using TOOLKIT_ARTIFACTS={g_ToolkitArtifacts}")
            print("Unset TOOLKIT_ARTIFACTS to download a new toolkit or set it to the path of an existing toolkit")
        else:
            print("Failed to Fetch toolkit from DVS.")
        sys.exit(4)
    if not verify_compiler():
        print("Failed to locate compiler in DVS toolkit build")
        sys.exit(5)

def make_lwbin(longname, shortname, testname, archnum, machine, lwccopts="", disasmopts="", lwasmopts="", useLwdisasmInternal=True, useLwasmInternal=True, reportCoverage=True):
    global g_UseArmCompiler
    global g_LwConfigPath
    
    libLwdaCxxHeaders = os.path.join(g_ToolkitArtifacts, "liblwdacxx/include")
    cgHeaders = os.path.join(g_ToolkitArtifacts, "lwca/tools/cooperative_groups")
    lwccarch  = "sm_" + archnum
    lwasmarch = "SM"  + archnum
    lwbinArtifactsDir = os.elwiron.get("LWBIN_ARTIFACTS_DIR") or os.getcwd()
    cmd_run("mkdir -p " + lwbinArtifactsDir)

    lwbin  = lwbinArtifactsDir + "/" + shortname + archnum + ".lwbin"
    ptxbin = lwbinArtifactsDir + "/" + longname + "." + lwccarch + ".ptx"
    sass   = lwbinArtifactsDir + "/" + longname + "." + lwccarch + ".sass"
    report = lwbinArtifactsDir + "/" + longname + "." + lwccarch + ".report"
    injectErrs = os.elwiron.get("INJECT_ERRORS")

    if (machine == "none"):
        machine = " "
    else:
        machine = " --machine " + machine

    lwccargs  = f"{machine} -DSM_VER={archnum} -arch {lwccarch} --ptxas-options=-v {lwccopts}"
    lwccargs += f" -DLWDA_COMPILER_RELEASE={g_CompilerReleaseX100}"

    if useLwasmInternal and has_exe("lwasm_internal"):
        assembler = " lwasm_internal "
    else:
        assembler = " lwasm "

    lwasm_args = lwasmopts;

    if injectErrs == '1':
        lwccargs += " -DINJECT_ERRORS=1"
    else:
        lwccargs += " -DINJECT_ERRORS=0"

    # Turn off the profile (which forces linking against the lwca runtime)
    lwccargs += " -noprof"

    # lwca device lambda function flag
    lwccargs += " -expt-extended-lambda"

    # Since the profile is off, need to manually force some include paths
    lwccargs += f" --include-path={g_LwdartHeaders},{g_LwConfigPath},{g_CompilerPath},{libLwdaCxxHeaders},{cgHeaders}"
    lwccargs += " -DLWDACC"

    if ('EXTRA_LWCC_ARGS' in os.elwiron):
        # Hack for systems where the installed gcc is very new:
        # export EXTRA_LWCC_ARGS="--include-path=/usr/include/x86_64-linux-gnu -D__x86_64__"
        lwccargs += " " + os.elwiron['EXTRA_LWCC_ARGS']
        
    # Don't enable optimization without careful study!
    # We want coverage, not speed.
    # lwccargs = lwccargs + " -O"   # enable optimization

    lwccargs += f" --library-path {g_CompilerPath}"
    lwccargs += f" --libdevice-directory {g_CompilerPath}/lwvm/libdevice"
    lwccelw   = f" PATH={get_os_path()} "

    if g_UseArmCompiler:
        lwccargs += f" -ccbin {g_ToolchainPath}/bin/aarch64-unknown-linux-gnu-c++"
        lwccargs += f" -Xcompiler --sysroot={g_ToolchainPath}/aarch64-unknown-linux-gnu"
    else:
        if g_ToolchainPath:
            lwccargs += f" -ccbin {g_ToolchainPath}/bin"
            lwccargs += f" -Xcompiler --sysroot={g_ToolchainPath}"

    if (g_MultiFormat):
        lwdaname = longname + ".lw"
        sassname = longname + ".sm" + archnum + ".lwasm"

        if not os.path.isfile( lwdaname ):
            print(("Source file " + lwdaname + " not found"))
            sys.exit(1)
        if not os.path.isfile( sassname ):
            print(("Source file " + sassname + " not found"))
            sys.exit(1)

        tmp1 = lwbinArtifactsDir + "/tmp1_" + longname + archnum + ".lwbin"
        err = print_and_run(
            lwccelw + assembler + lwasm_args + " -arch " + lwasmarch + " -o " + tmp1 + " " + sassname )
        if err:
            sys.exit(err)
        if not os.path.isfile( tmp1 ):
            sys.exit(1)
        print("lwasm done")

        tmp2 = lwbinArtifactsDir + "/tmp2_" + longname + archnum + ".lwbin"
        err = print_and_run(
            lwccelw + " lwcc " + lwccargs + " -o " + tmp2 + " -lwbin -dc " + lwdaname )
        if err:
            sys.exit(err)
        if not os.path.isfile( tmp2 ):
            sys.exit(1)
        print("lwcc done")
 
        err = print_and_run(
            lwccelw + " lwcc " + lwccargs + " -o " + lwbin + " -lwbin -dlink " + tmp1 + " " + tmp2 )
        if err:
            sys.exit(err)
        if not os.path.isfile( lwbin ):
            sys.exit(1)
        print("link done")
        
        if reportCoverage:
            report_coverage(sass, lwbin, report, testname, useLwdisasmInternal, disasmopts)
    
    elif (g_MerlwryFlow):
        mercname = f"{longname}.postcpp.merc"
        
        print(os.getcwd())
        if not os.path.isfile(mercname):
            print(f"Source file {mercname} not found")
            sys.exit(1)
        
        tmp1 = os.path.join(lwbinArtifactsDir, f"tmp1_{longname}{archnum}.mercbin")
        # mercasm transformation from textual merlwry to merlwry binary
        err = print_and_run(
            f"{lwccelw} mercasm -o {tmp1} {mercname}")
        if err:
            sys.exit(err)
        if not os.path.isfile(tmp1):
            sys.exit(1)
        print("mercasm done")
        
        # doing the finalizer step to colwert merlwry binary to sass lwbin
        err = print_and_run(
            f"{lwccelw} finalizer -arch {lwccarch} -o {lwbin} {tmp1}")
        if err:
            sys.exit(err)
        if not os.path.isfile(lwbin):
            sys.exit(1)
        print("merlwry finalizer done")

    else:
        if os.path.isfile( longname + ".lw" ):
            suffix = ".lw"
        elif os.path.isfile( longname + ".ptx" ):
            suffix = ".ptx"
        elif os.path.isfile( longname + ".lwasm" ):
            suffix = ".lwasm"
        elif os.path.isfile( longname + ".lwasm" ):
            suffix = ".lwasm"
        else:
            print(("Source file " + longname + ".lw, " + longname +
                  ".ptx, or " + longname + ".lwasm/.lwasm not found"))
            sys.exit(1)
        source = longname + suffix

        if os.path.isfile( lwbin ):
            os.remove( lwbin )
        if source.endswith(".lwasm") or source.endswith(".lwasm"):
            errorcode = print_and_run(lwccelw + assembler + lwasm_args + " -o " + lwbin + " " + source )
        else:
            errorcode = print_and_run(lwccelw + " lwcc " + lwccargs + " -o " + lwbin + " -lwbin " + source )
        if not os.path.isfile( lwbin ):
            sys.exit(1)
        if errorcode:
            sys.exit(errorcode)

        if int(archnum) < 20 or source[-6:] == ".lwasm" or source[-6:] == ".lwasm":
            useLwdisasmInternal = False
        
        if reportCoverage:
            if source[-3:] == ".lw":
                print_and_run( lwccelw + " lwcc " + lwccargs + " -o " + ptxbin + " -ptx " + source)

            report_coverage(sass, lwbin, report, testname, useLwdisasmInternal, disasmopts)

# Dump coverage report unto sass file with appropriate .exe
def report_coverage(sass, lwbin, report, testname, useLwdisasmInternal=True, disasmopts=""):
    # Uncomment to get a .ptx assembly file to look at.
    if useLwdisasmInternal and has_exe("lwdisasm_internal"):
        sassdumpexe = has_exe("lwdisasm_internal")
        sassdump_cmd = f"{sassdumpexe} {disasmopts} -hex {lwbin} >> {sass}"
        #sassdump_cmd = "LWIDIA_SHOW_INTERNAL_INSTS=1 " + sassdumpexe + " " + disasmopts + " " + lwbin + " >> " + sass
    elif has_exe("fatdump"):
        sassdumpexe = has_exe("fatdump")
        sassdump_cmd = f"{sassdumpexe} --dump-sass {lwbin} >> {sass}"
    elif has_exe("lwobjdump"):
        sassdumpexe = has_exe("lwobjdump")
        sassdump_cmd = f"{sassdumpexe} --dump-sass {lwbin} >> {sass}"
    else:
        print("Could not find appropriate exelwtable for coverage report.",
              "Try running with --no-coverage-report.")
        sys.exit(1)

    write_compiler_info(sassdumpexe, sass)
    errorcode = print_and_run(sassdump_cmd)
    if errorcode != 0:
        sys.exit(1)
    errorcode = print_and_run(f"{g_Python3} ../report_coverage.py -i {sass} -o {report} -t {testname} -r head")
    if errorcode != 0:
        sys.exit(1)



# Writes compiler info to sass file
def write_compiler_info(sassdumpexe, sass):
    global g_CompilerRelease
    global g_CompilerVersion
    os.system("echo /*COMPILERVER: Lwca compilation tools, release " + g_CompilerRelease + ", " + g_CompilerVersion + "*/ > " + sass)
    if sassdumpexe != None:
        os.system("echo /*DUMPEXE: " + sassdumpexe + "*/ >> " + sass)

# Looks for the given program in the PATH
# Returns the path to the program if it is found
def has_exe(program):
    for path in get_os_path().split(os.pathsep):
        exe_file = os.path.join(path, program)
        if os.path.exists(exe_file) and os.access(exe_file, os.X_OK):
            return exe_file
    return None
        
def print_and_run(system_str):
    print()
    print(system_str)
    print()
    return os.system(system_str)

def cmd_run(system_str):
    return os.system(system_str)
    
def get_os_path ():
    return g_CompilerPath + ":" + \
           g_CompilerPath + "/lwvm/bin:" + \
           os.elwiron['PATH']

# Print usage string
def usage ():
    print(( "Usage: make_kernel_bin.py -l <longfilename> -s <shortfilename> -a <archnum> -m <machine> " +
            "--dvs-rel-compiler <dvscompiler>  " +
            "--driver-root <driverpath> --toolchain-path <rootgccpath> --jobs <nummakejobs>"))
    print(("e.g.: make_kernel_bin.py -l lwdamats -s lwmats -a 10 -m 32 " +
           "--dvs-rel-compiler gpu_drv_chips_Release_Linux_AMD64_GPGPU_COMPILER " +
           "--driver-root /home/scratch/sw/dev/gpu_drv/chips_a " +
           "--toolchain-path /home/scratch/sw/tools/linux/mods/gcc-7.3.0-x86_64"))

def main ():
    if os.name != "posix":
        print("LWCA kernels must be compiled on a Linux system")
        sys.exit(3)

    # Parse the options from the command line
    try:
        opts, args = getopt.getopt(sys.argv[1:], "l:s:t:a:m:n:d:j",
                                   ["longname=", "shortname=", "testname=",
                                    "archnum=", "machine=", "lwcc-options=",
                                    "disasm-options=", "jobs=", "lwasm-options=",
                                    "no-lwdisasm-internal", "no-lwasm-internal",
                                    "force-dev-compiler", "no-coverage-report",
                                    "multi-format", "dvs-rel-compiler=",
                                    "use-arm-compiler", "debug-toolkit",
                                    "driver-root=", "toolchain-path=",
                                    "fetch-libs-toolkit", "merlwry-flow",
                                    "lwconfig-path="])
    except getopt.GetoptError as err:
        print(str(err))
        usage()
        sys.exit(2)
    longname = ""
    shortname = ""
    testname = "Unknown"
    archnum = ""
    machine = ""
    lwccopts = ""
    lwasmopts = ""
    disasmopts = ""
    useLwdisasmInternal = True
    useLwasmInternal = True
    fetchToolkit = False
    # The report coverage script lwrrently does not honor LWBINS_DIR
    reportCoverage = os.elwiron.get("LWBINS_DIR") == None
    if reportCoverage:
        os.elwiron["REPORT_COVERAGE"] = "true"
    else:
        os.elwiron["REPORT_COVERAGE"] = "false"

    global g_DVSReleaseToolkitBranch
    global g_UseDevLwcc
    global g_UseArmCompiler
    global g_DevToolkitPath
    global g_RelToolkitPath
    global g_LwConfigPath
    global g_ToolkitBuildCfg
    global g_DriverRoot
    global g_ToolchainPath
    global g_MultiFormat
    global g_UseLibsToolkit
    global g_MerlwryFlow

    for o, a in opts:
        if o in ("-l", "--longname"):
            longname = a
        elif o in ("-s", "--shortname"):
            shortname = a
        elif o in ("-t", "--testname"):
            testname = a
        elif o in ("-a", "--archnum"):
            archnum = a
        elif o in ("-m", "--machine"):
            machine = a
        elif o in ("-n", "--lwcc-options"):
            lwccopts = a
        elif o in ("-d", "--disasm-options"):
            disasmopts = a
        elif o in ("-j", "--jobs"):
            global g_MakeJobs
            g_MakeJobs = a
        elif o in ("--lwasm-options"):
            lwasmopts = a
        elif o in ("--no-lwdisasm-internal"):
            useLwdisasmInternal = False
        elif o in ("--no-lwasm-internal"):
            useLwasmInternal = False
        elif o in ("--force-dev-compiler"):
            g_UseDevLwcc = True
        elif o in ("--no-coverage-report"):
            reportCoverage = False
        elif o in ("--multi-format"):
            g_MultiFormat = True
        elif o in ("--dvs-rel-compiler"):
            g_DVSReleaseToolkitBranch = a
        elif o in ("--use-arm-compiler"):
            g_UseArmCompiler = True
        elif o in ("--debug-toolkit"):
            g_ToolkitBuildCfg = 'debug'
        elif o in ("--driver-root"):
            g_DriverRoot = a
        elif o in ("--toolchain-path"):
            g_ToolchainPath = a
        elif o in ("--fetch-libs-toolkit"):
            g_UseLibsToolkit = True
            fetchToolkit = True
        elif o in ("--lwconfig-path"):
            g_LwConfigPath = a
        elif o in ("--merlwry-flow"):
            g_MerlwryFlow = True
        else:
            usage()
            sys.exit(2)

    # Check the general arguments that are always required
    if (g_DVSReleaseToolkitBranch == "" or g_ToolchainPath == "" or g_DriverRoot == ""):
        usage()
        sys.exit(2)

    # Fetch toolkit
    fetch_all()

    # If explicitly requested to fetch toolkit, exit after completion
    if fetchToolkit:
        sys.exit(0)

    # Check the arguments that are needed for lwbin builds
    if (longname == "" or shortname == "" or archnum == "" or machine == ""):
        usage()
        sys.exit(2)

    print("lwccopts=" + lwccopts + " disasmopts=" + disasmopts + "lwasmopts=" + lwasmopts + "\n")
    make_lwbin(longname, shortname, testname, archnum, machine, lwccopts, disasmopts, lwasmopts, useLwdisasmInternal, useLwasmInternal, reportCoverage)
if __name__ == "__main__":
    main()
