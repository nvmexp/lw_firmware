#!/bin/csh -f
#=======================================================
# IMPTest3 cmdline for Linux to test POR modes
# useFakedDisplays\linux\keplerAndLater\IMPtest3_TestPORmodesFromInputFile.sh
#=======================================================

#imptest3_config_file : 
#    File must exist in MODS_RUNSPACE in folder
#    $MODS_RUNSPACE/dispinfradata/imptest3/configs/"
set usage="Usage -chip <> -ram <sddr3/gddr5/sddr4/hbm> -fbp <1/2/3/4> [-type <2t>] [-vbios <>] [-target <def=emu: emu/silicon>]"
set prc_id = `date +%F_%k_%M_%S | sed "s/ //g"`

if($#argv == 0) then
   echo $usage
   exit
endif

set type = ""
set target = "emu"
set vbios = ""

while ($#argv > 0)
   switch ($argv[1])
      case -vbios:
           set vbios = $2
           shift argv
       breaksw
      case -ram:
           set dram = $2
           shift argv
       breaksw
      case -chip:
           set chip = $2
           shift argv
       breaksw
      case -fbp:
           set fbp = $2
           shift argv
       breaksw
      case -type:
           set type = $2
           shift argv
       breaksw
      case -target:
           set target = $2
           shift argv
       breaksw
      default:
           echo "Bad command line argument -->> $usage"
       exit
   endsw
   shift argv
end

echo "Chip is $chip RAM is $dram and FBP is $fbp"

set emu_specific_cmd_line =""
if (($target == "emu") && (($dram == "gddr5") | ($dram == "sddr4")) | ($dram == "hbm")) then 
  set emu_specific_cmd_line = "-cmos_training 0x80000000 -cml_training 0x80000000 -rmkey RMFbLinkTraining 0x00000020"
  echo " WARNING  WARNING WARNING WARNING WARNING WARNING WARNING  WARNING  WARNING"
  echo " WARNING  This is EMULATOR based run so bypassing Link training     WARNING"
  echo " WARNING  This is EMULATOR based run so bypassing Link training     WARNING"
  echo " WARNING  This is EMULATOR based run so bypassing Link training     WARNING"
  echo " WARNING  This is EMULATOR based run so bypassing Link training     WARNING"
  echo " WARNING  This is EMULATOR based run so bypassing Link training     WARNING"
  echo " WARNING  WARNING WARNING WARNING WARNING WARNING WARNING  WARNING  WARNING"
  #set emu_specific_cmd_line = "-cmos_training 0x80000000 -cml_training 0x80000000 "
endif

if($vbios == "") then
  if(($chip == "gm200") || ($chip == "gm204") || ($chip == "gm206")) then
    if($type == "") then
      set vbios2use = "-gpubios 0 ${chip}_emu_${dram}_dcb-ultimate_p-states.rom"
    else
      set vbios2use = "-gpubios 0 ${chip}_emu_${dram}_${type}_dcb-ultimate_p-states.rom"
    endif
  else
    if(($chip == "gp100") || ($chip == "gp102") || ($chip == "gp104") || ($chip == "gp106") || ($chip == "gp107") || ($chip == "gp108")) then
      if($type == "") then
        set vbios2use = "-gpubios 0 ${chip}_emu_${dram}_imp_p-states_dcb-ultimate.rom"
      else
        set vbios2use = "-gpubios 0 ${chip}_emu_${dram}_${type}_imp_p-states_dcb-ultimate.rom"
      endif
	else
      if($type == "") then
        set vbios2use = "-gpubios 0 ${chip}_emu_${dram}_dpcfg1_splitsor3-pstates-4dp.rom"
      else
        set vbios2use = "-gpubios 0 ${chip}_emu_${dram}_${type}_dpcfg1_splitsor3-pstates-4dp.rom"
      endif
	endif
  endif
else
     set vbios2use = "-gpubios 0 $vbios.rom"
endif

set DRAM = `echo $dram | tr '[:lower:]' '[:upper:]'`
set CHIP = `echo $chip | tr '[:lower:]' '[:upper:]'`

set imptest3_config_file="${chip}_arch_imp_testplan_${DRAM}_${fbp}fbp.js"
set log_file=`echo $imptest3_config_file | sed s/\.js//g`
set log_file="imp_log_$log_file"_"$prc_id"".log"

#IMPTest3 test cmdline
set fbp_args = ""
if($target == "silicon") then 
  if($fbp == 1) then 
    set fbp_args = "-fermi_fs fbp_enable:1 -fermi_fs fbio_enable:1"
  else
    set fbp_args = "-fermi_fs fbp_enable:3 -fermi_fs fbio_enable:3"
  endif
endif

#
# Note:
# 1) "-no_pstate_lock_at_init" is used to prevent the mods infrastructure
# from automatically calling PStates 1.x-style "force pstate" functions at
# mods init time.  These APIs have been defined to disable dispclk decoupling
# (if dispclk decoupling happens to be enabled), and this causes dispclk to
# switch to the frequency specified in the pstate, which, in the case of
# decoupled dispclk, has not been validated by IMP.
# 2) enabling autoGenerate EDID. To force any specific EDID file, kindly remove -autoGenerateEdid and 
# replace it with -useDFPFakeEdid edidFileName1.txt,edidFileName2.txt,edidFileName3.txt
#

#set imptest3_rmtest_cmd=" -forcetestname ModeSet -no_pstate_lock_at_init -ignoreClockDiffError -freqTolerance100x 2 -forceLargeCursor -protocol "DP" -autoGenerateEdid -runfrominputfile -inputconfigfile $imptest3_config_file -forcePStateBeforeModeset -disableRandomPStateTest $vbios2use $emu_specific_cmd_line -rmkey RMBandwidthFeature 0x3AFFFFFF -rmkey RMBandwidthFeature2 0x3 $fbp_args"
 
set imptest3_rmtest_cmd=" -forcetestname ModeSet -no_pstate_lock_at_init -ignoreClockDiffError -freqTolerance100x 2 -forceLargeCursor -protocol "DP" -useDFPFakeEdid "ARCH_PASCAL_IMP.txt" -runfrominputfile -inputconfigfile $imptest3_config_file -forcePStateBeforeModeset -disableRandomPStateTest $vbios2use $emu_specific_cmd_line -rmkey RMBandwidthFeature 0x3AFFFFFF -rmkey RMBandwidthFeature2 0x3 $fbp_args"

# ---------------------- Extract non-disp tests
pushd ../../../../../../ > /dev/null
pwd
if ( ! -d DTI ) then
   echo "ERROR : Missing DTI directory, Check client spec and make sure to include //sw/mods/rmtest/..."
   exit
else 
   echo "Extracting the dynamic traces for the non-Display engines"
endif

# create the output directory
foreach trace_path (`cat  dispinfradata/imptest3/configs/imp_traffic_${chip}.cfg`)
   # Look for the Dynamic traces (These are Grafix trace which are generated ones. 
   # We extract them from the .tar files.
   echo $trace_path | grep "DTI\/imptest3/imptraffic\/dynamic" > /dev/null
   if ($? == 0) then
      #if the path has "dynamic" tests in it, then add chip to the path as every chip would have its own traces
      set trace_path_chip = `echo $trace_path | sed "s/dynamic/dynamic\/$chip/g"`
      echo $trace_path | grep "test.hdr" > /dev/null # Look for -crc path, -i test.hdr or -o out
      if ($? != 0) then 
        echo $trace_path | grep "out" > /dev/null
        if ($? != 0) then
      if( ! -d $trace_path_chip ) then
        echo "ERROR : Trace doesn't exist for $trace_path_chip in Mods"
        exit
      endif

      if( ! -d $trace_path ) then
        echo "Trace path doesn't exist for $trace_path in Mods, creating one"
        mkdir -p $trace_path
      endif
      
          # Everything looks fine, so let's copy over the traces from the chip directory 
          # to the chip independent directory for IMPTest3 to run
      cp -rf $trace_path_chip $trace_path/..

      pushd $trace_path > /dev/null

      #Now Extract the trace  "trace" directory
          echo "Extracting test $trace_path sourced from $trace_path_chip"
      if(-d trace ) then 
        rm -rf trace
      endif
      
          # create trace directory as that's where the test files would be extracted.
      mkdir trace
      cd trace

      if ( ! -e ../test.tar ) then 
        if ( -e ../test.tgz ) then 
         cp ../test.tgz .
         pwd
         gunzip test.tgz > /dev/null
         if ( $? == 1 ) then 
           echo "Can't unzip $trace_path Exiting..."
           exit
         endif
        else
         echo "ERROR: Don't see test.tgz for $trace_path/test.tgz  exiting"
         exit
        endif
      else
        cp ../test.tar .
      endif

      tar xvf test.tar > /dev/null

      if ( $? == 1 ) then 
        echo "Can't untar $trace_path Exiting..."
        exit
      endif
      popd > /dev/null
    endif
      endif
   else
     #if the traces are Static traces (Video and CE then use trace02)
     echo $trace_path | grep "DTI\/imptest3/imptraffic\/static" > /dev/null
     if ($? == 0) then
        echo "Extracting test $trace_path"
    mkdir -p -v  $trace_path
     endif
   endif
end
# ---------------------- Extract non-disp tests
popd

switch ($target)
   case silicon:
        set platform = 3
    breaksw
   case emu:
        set platform = 2
    breaksw
   case fmodel:
        set platform = 1
    breaksw
   case fmodel32:
        set platform = 4
    breaksw
   default:
        echo "Bad command line argument -->> $usage"
    exit
endsw

cd ../../../
./archImpEmuRuns.sh  "$imptest3_rmtest_cmd" "$platform" "$chip" "$log_file"

