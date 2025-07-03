#!/bin/sh
#=======================================================
# IMPTest3 Linux launcher support file. 
# Do not ilwoke it directly. It needs a cmdline parameters
# Usage :
#    ./linuxLauncherSupportScript.sh $imptest3_rmtest_cmd 
#=======================================================

#=========== BEGIN OF USER EDITABLE VARIABLES ==========
#platformType: Platform on which this is to be run. Valid values are:
#    1=fmodel 64bit
#    2=emulation
#    3=silicon
#    4=fmodel 32bit
#     default=silicon
platformType=$2

#modsBuildType: Indicates mods build flavour. Valid values are :
#    1=sim mods
#    2=linux mfg mods
#    3=linux
#    default=sim mods
modsBuildType=1

#chipName :
#    chip name in small case.No spaces.
chipName=$3

#vbios2use : helps specify vbios to use
#    for fmodel use " -gpubios 0 "$chipName"_sim_sddr3.rom "
#    for hardware use "-hwbios"
vbios2use=" -gpubios 0 ${chipName}_emu_sddr3_dpcfg1_nonsplitsor1-pstate_busfix_dp.rom" 

log_file=$4
#run_with_traces_enabled : Indicates if to run with traces or not
#    0=no traces - just rmtest in single thread
#    1=with traces in parallel to rmtest. If 1 then specify "trace_filename"
run_with_traces_enabled=1
E_NOARGS=84
if [ -e "dispinfradata/imptest3/configs/imp_traffic_$chipName.cfg" ]; then
  echo "BAD TRACE files. dispinfradata/imptest3/configs/imp_traffic_$chipName.cfg. Populate it before a rerun"
  exit $E_NOARGS
fi
trace_filename="dispinfradata/imptest3/configs/imp_traffic_$chipName.cfg"

#======== END OF USER EDITABLE VARIABLES ===============
case $platformType in
    1) engr="";;
    2) engr="";;
    3) engr="-engr";;
    4) engr="";;
    *) engr="-engr";
esac
#======== BEGIN OF SCRIPT SPECIFIC VARIABLES ===========
#mods cmdline prefix for sim mods and linuxmfg
sim_mods_cmd="./sim.pl -debug -chip $chipName $vbios2use "
linuxmfg_mods_cmd="./mods -d $vbios2use"
#Added -allow_max_perf as suggested in bug 1424749. It causes Mclk switch failure to be reported as warning and not ERROR in Query 
mods_suffix_params=" -enable_clk 0x10 -poll_interrupts -timeout_ms 0xFFF000 -devid_check_ignore -cvb_check_ignore -ignore_tesla -allow_max_perf 0x200 -rmmsg :"

#cmdline to be used when running with / without traces
mods_without_traces=" rmtest.js ";
mods_with_traces=" -l $log_file mrmtestinfra.js -serial -f $trace_filename -trepfile trep.txt $engr -dmacheck_ce -mrm_sync -mrm_rm_slave ";
#mods_with_traces=" mdiag.js -f $trace_filename ";
#======== END OF SCRIPT SPECIFIC VARIABLES =============
E_NOARGS=85
imptest3_rmtest_cmd=$1

if [ -z "$imptest3_rmtest_cmd" ]; then
    echo "ERROR: This script needs 1 cmdline specifying the 'imptest3_rmtest_cmd'."
    exit $E_NOARGS
fi

case $platformType in
    1) platform=" -fmod64 ";;
    2) platform=" -hwchip -modsOpt";;
    3) platform=" -hwchip2 ";;
    4) platform=" -fmod ";;
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

echo "Setup done, Running the test now"
$cmdLine
