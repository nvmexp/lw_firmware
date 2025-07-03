#!/bin/sh
#=======================================================
# IMPTest3 Linux launcher support file. 
# Do not ilwoke it directly. It needs a cmdline parameters
# Usage :
#    ./linuxLauncherSupportScript.sh $imptest3_rmtest_cmd 
#=======================================================

#=========== BEGIN OF USER EDITABLE VARIABLES ==========
#platformType: Platform on which this is to be run. Valid values are:
#    1=fmodel
#    2=emulation
#    3=silicon
#     default=silicon
platformType=3

#modsBuildType: Indicates mods build flavour. Valid values are :
#    1=sim mods
#    2=linux mfg mods
#    3=linux
#    default=sim mods
modsBuildType=1

#chipName :
#    chip name in small case.No spaces.
chipName="gk208"

#vbios2use : helps specify vbios to use
#    for fmodel use " -gpubios 0 "$chipName"_sim_sddr3.rom "
#    for hardware use "-hwbios"
vbios2use=" -hwbios " 

#run_with_traces_enabled : Indicates if to run with traces or not
#    0=no traces - just rmtest in single thread
#    1=with traces in parallel to rmtest. If 1 then specify "trace_filename"
run_with_traces_enabled=0
trace_filename="comp_one_tri.cfg"

#======== END OF USER EDITABLE VARIABLES ===============

#======== BEGIN OF SCRIPT SPECIFIC VARIABLES ===========
#mods cmdline prefix for sim mods and linuxmfg
sim_mods_cmd="./sim.pl -chip $chipName $vbios2use "
linuxmfg_mods_cmd="./mods $vbios2use "
mods_suffix_params=" -poll_interrupts -timeout_ms 0x15000 "

#cmdline to be used when running with / without traces
mods_without_traces=" rmtest.js ";
mods_with_traces=" mrmtestinfra.js -serial -f $trace_filename -trepfile trep.txt -mrm_sync -mrm_rm_slave ";
#======== END OF SCRIPT SPECIFIC VARIABLES =============
E_NOARGS=85
imptest3_rmtest_cmd=$1

if [ -z "$imptest3_rmtest_cmd" ]; then
    echo "ERROR: This script needs 1 cmdline specifying the 'imptest3_rmtest_cmd'."
    exit $E_NOARGS
fi

case $platformType in
    1) platform=" -fmod ";;
    2) platform=" -hwchip ";;
    3) platform=" -hwchip2 ";;
    *) platform=" -hwchip2 ";;
esac

#lets decide which mods prefix to be used depending on the mods flavour.
case $modsBuildType in
    1) mods_cmd=$sim_mods_cmd;;
    2) mods_cmd=$linuxmfg_mods_cmd;;
    3) mods_cmd=$linuxmfg_mods_cmd;;
    *) mods_cmd=$sim_mods_cmd;;
esac

#mods js arguments [rmtest.js / mrmtestinfra.js ... ]
if [ $run_with_traces_enabled -eq 1 ]; then
    mods_js_arguments=$mods_with_traces
else
    mods_js_arguments=$mods_without_traces
fi

cmdLine="$mods_cmd $platform $mods_js_arguments $imptest3_rmtest_cmd $mods_suffix_params"

#lets goto mods runspace
cd ../../../
#======================================================
#Lets print the variables for debug purpose
#======================================================
echo "Current directory =   `pwd`"
echo "platformType      =   $platformType"
echo "modsBuildType     =   $modsBuildType"
echo "chipName          =   $chipName"
echo "run_with_traces_enabled=$run_with_traces_enabled"
echo "trace_filename    =   $trace_filename"
echo "Final Commandline=    $cmdLine"
#========= END OF DEBUG PRINTS VARIABLES ==============

$cmdLine
