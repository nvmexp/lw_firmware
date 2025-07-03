#!/bin/sh
#=======================================================
# IMPTest3 cmdline for Linux to test POR modes
# onlyConnectedDisplays\linux\keplerAndLater\IMPtest3_TestPORmodesFromInputFile.sh
#=======================================================

#imptest3_config_file : 
#    File must exist in MODS_RUNSPACE in folder
#    $MODS_RUNSPACE/dispinfradata/imptest3/configs/"
imptest3_config_file="GK208_NB_Geforce_DDR3_GDDR5_64Bit_or_Hybrid.js"

#IMPTest3 test cmdline
# Note: "-no_pstate_lock_at_init" is used to prevent the mods infrastructure
# from automatically calling PStates 1.x-style "force pstate" functions at
# mods init time.  These APIs have been defined to disable dispclk decoupling
# (if dispclk decoupling happens to be enabled), and this causes dispclk to
# switch to the frequency specified in the pstate, which, in the case of
# decoupled dispclk, has not been validated by IMP.
imptest3_rmtest_cmd=" -forcetestname Modeset -no_pstate_lock_at_init -ignoreClockDiffError -freqTolerance100x 2 -forceLargeCursor -enableMSCGTest -onlyConnectedDisplays -runfrominputfile -inputconfigfile $imptest3_config_file "

cd ../../../
./linuxLauncherSupportScript.sh  "$imptest3_rmtest_cmd"

