/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016,2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gv10xpm.cpp
 * @brief  Implemetation of the GV10x performance monitor
 *
 */

#include "gv10xpm.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/gpu.h"
#include "volta/gv100/dev_ltc.h"
#include "volta/gv100/dev_perf.h"
#include "volta/gv100/pm_signals.h"
#include "volta/gv100/dev_graphics_nobundle.h"


//*************************************************************************************************
// Contacts: Robert Hero
// To get to correct signal routing and perfmon programming you need to do the
// following:
//
// 1. Search through the //hw/tools/perfalyze/chips/gv100/pml_files/ltc.pml
//    template file to find any experiment that captures the ltc hit & miss
//    counts. ie I found these and created text files for each Gpu:
// Experiment                       Found in GPU's ltc.pml file
// ltc_pm_tag_hit_fbp0_l2slice0     GV100
// ltc_pm_tag_miss_fbp0_l2slice0    GV100
// ltc_pm_tag_hit_fbp0_l2slice1     GV100
// ltc_pm_tag_miss_fbp0_l2slice1    GV100
// ltc_pm_tag_hit_fbp0_l2slice2     GV100
// ltc_pm_tag_miss_fbp0_l2slice2    GV100
// ltc_pm_tag_hit_fbp0_l2slice3     GV100
// ltc_pm_tag_miss_fbp0_l2slice3    GV100
// ltc_pm_tag_hit_fbp1_l2slice0     GV100
// ltc_pm_tag_miss_fbp1_l2slice0    GV100
// ltc_pm_tag_hit_fbp1_l2slice1     GV100
// ltc_pm_tag_miss_fbp1_l2slice1    GV100
// ltc_pm_tag_hit_fbp1_l2slice2     GV100
// ltc_pm_tag_miss_fbp1_l2slice2    GV100
// ltc_pm_tag_hit_fbp1_l2slice3     GV100
// ltc_pm_tag_miss_fbp1_l2slice3    GV100
// ltc_pm_tag_hit_fbp2_l2slice0     GV100
// ltc_pm_tag_miss_fbp2_l2slice0    GV100
// ltc_pm_tag_hit_fbp2_l2slice1     GV100
// ltc_pm_tag_miss_fbp2_l2slice1    GV100
// ltc_pm_tag_hit_fbp2_l2slice2     GV100
// ltc_pm_tag_miss_fbp2_l2slice2    GV100
// ltc_pm_tag_hit_fbp2_l2slice3     GV100
// ltc_pm_tag_miss_fbp2_l2slice3    GV100
// ltc_pm_tag_hit_fbp3_l2slice0     GV100
// ltc_pm_tag_miss_fbp3_l2slice0    GV100
// ltc_pm_tag_hit_fbp3_l2slice1     GV100
// ltc_pm_tag_miss_fbp3_l2slice1    GV100
// ltc_pm_tag_hit_fbp3_l2slice2     GV100
// ltc_pm_tag_miss_fbp3_l2slice2    GV100
// ltc_pm_tag_hit_fbp3_l2slice3     GV100
// ltc_pm_tag_miss_fbp3_l2slice3    GV100
// ltc_pm_tag_hit_fbp4_l2slice0     GV100
// ltc_pm_tag_miss_fbp4_l2slice0    GV100
// ltc_pm_tag_hit_fbp4_l2slice1     GV100
// ltc_pm_tag_miss_fbp4_l2slice1    GV100
// ltc_pm_tag_hit_fbp4_l2slice2     GV100
// ltc_pm_tag_miss_fbp4_l2slice2    GV100
// ltc_pm_tag_hit_fbp4_l2slice3     GV100
// ltc_pm_tag_miss_fbp4_l2slice3    GV100
// ltc_pm_tag_hit_fbp5_l2slice0     GV100
// ltc_pm_tag_miss_fbp5_l2slice0    GV100
// ltc_pm_tag_hit_fbp5_l2slice1     GV100
// ltc_pm_tag_miss_fbp5_l2slice1    GV100
// ltc_pm_tag_hit_fbp5_l2slice2     GV100
// ltc_pm_tag_miss_fbp5_l2slice2    GV100
// ltc_pm_tag_hit_fbp5_l2slice3     GV100
// ltc_pm_tag_miss_fbp5_l2slice3    GV100
// ltc_pm_tag_hit_fbp6_l2slice0     GV100
// ltc_pm_tag_miss_fbp6_l2slice0    GV100
// ltc_pm_tag_hit_fbp6_l2slice1     GV100
// ltc_pm_tag_miss_fbp6_l2slice1    GV100
// ltc_pm_tag_hit_fbp6_l2slice2     GV100
// ltc_pm_tag_miss_fbp6_l2slice2    GV100
// ltc_pm_tag_hit_fbp6_l2slice3     GV100
// ltc_pm_tag_miss_fbp6_l2slice3    GV100
// ltc_pm_tag_hit_fbp7_l2slice0     GV100
// ltc_pm_tag_miss_fbp7_l2slice0    GV100
// ltc_pm_tag_hit_fbp7_l2slice1     GV100
// ltc_pm_tag_miss_fbp7_l2slice1    GV100
// ltc_pm_tag_hit_fbp7_l2slice2     GV100
// ltc_pm_tag_miss_fbp7_l2slice2    GV100
// ltc_pm_tag_hit_fbp7_l2slice3     GV100
// ltc_pm_tag_miss_fbp7_l2slice3    GV100
// ltc_pm_tag_hit_fbp8_l2slice0     GV100
// ltc_pm_tag_miss_fbp8_l2slice0    GV100
// ltc_pm_tag_hit_fbp8_l2slice1     GV100
// ltc_pm_tag_miss_fbp8_l2slice1    GV100
// ltc_pm_tag_hit_fbp8_l2slice2     GV100
// ltc_pm_tag_miss_fbp8_l2slice2    GV100
// ltc_pm_tag_hit_fbp8_l2slice3     GV100
// ltc_pm_tag_miss_fbp8_l2slice3    GV100
// ltc_pm_tag_hit_fbp9_l2slice0     GV100
// ltc_pm_tag_miss_fbp9_l2slice0    GV100
// ltc_pm_tag_hit_fbp9_l2slice1     GV100
// ltc_pm_tag_miss_fbp9_l2slice1    GV100
// ltc_pm_tag_hit_fbp9_l2slice2     GV100
// ltc_pm_tag_miss_fbp9_l2slice2    GV100
// ltc_pm_tag_hit_fbp9_l2slice3     GV100
// ltc_pm_tag_miss_fbp9_l2slice3    GV100
// ltc_pm_tag_hit_fbp10_l2slice0    GV100
// ltc_pm_tag_miss_fbp10_l2slice0   GV100
// ltc_pm_tag_hit_fbp10_l2slice1    GV100
// ltc_pm_tag_miss_fbp10_l2slice1   GV100
// ltc_pm_tag_hit_fbp10_l2slice2    GV100
// ltc_pm_tag_miss_fbp10_l2slice2   GV100
// ltc_pm_tag_hit_fbp10_l2slice3    GV100
// ltc_pm_tag_miss_fbp10_l2slice3   GV100
// ltc_pm_tag_hit_fbp11_l2slice0    GV100
// ltc_pm_tag_miss_fbp11_l2slice0   GV100
// ltc_pm_tag_hit_fbp11_l2slice1    GV100
// ltc_pm_tag_miss_fbp11_l2slice1   GV100
// ltc_pm_tag_hit_fbp11_l2slice2    GV100
// ltc_pm_tag_miss_fbp11_l2slice2   GV100
// ltc_pm_tag_hit_fbp11_l2slice3    GV100
// ltc_pm_tag_miss_fbp11_l2slice3   GV100
// ltc_pm_tag_hit_fbp12_l2slice0    GV100
// ltc_pm_tag_miss_fbp12_l2slice0   GV100
// ltc_pm_tag_hit_fbp12_l2slice1    GV100
// ltc_pm_tag_miss_fbp12_l2slice1   GV100
// ltc_pm_tag_hit_fbp12_l2slice2    GV100
// ltc_pm_tag_miss_fbp12_l2slice2   GV100
// ltc_pm_tag_hit_fbp12_l2slice3    GV100
// ltc_pm_tag_miss_fbp12_l2slice3   GV100
// ltc_pm_tag_hit_fbp13_l2slice0    GV100
// ltc_pm_tag_miss_fbp13_l2slice0   GV100
// ltc_pm_tag_hit_fbp13_l2slice1    GV100
// ltc_pm_tag_miss_fbp13_l2slice1   GV100
// ltc_pm_tag_hit_fbp13_l2slice2    GV100
// ltc_pm_tag_miss_fbp13_l2slice2   GV100
// ltc_pm_tag_hit_fbp13_l2slice3    GV100
// ltc_pm_tag_miss_fbp13_l2slice3   GV100
// ltc_pm_tag_hit_fbp14_l2slice0    GV100
// ltc_pm_tag_miss_fbp14_l2slice0   GV100
// ltc_pm_tag_hit_fbp14_l2slice1    GV100
// ltc_pm_tag_miss_fbp14_l2slice1   GV100
// ltc_pm_tag_hit_fbp14_l2slice2    GV100
// ltc_pm_tag_miss_fbp14_l2slice2   GV100
// ltc_pm_tag_hit_fbp14_l2slice3    GV100
// ltc_pm_tag_miss_fbp14_l2slice3   GV100
// ltc_pm_tag_hit_fbp15_l2slice0    GV100
// ltc_pm_tag_miss_fbp15_l2slice0   GV100
// ltc_pm_tag_hit_fbp15_l2slice1    GV100
// ltc_pm_tag_miss_fbp15_l2slice1   GV100
// ltc_pm_tag_hit_fbp15_l2slice2    GV100
// ltc_pm_tag_miss_fbp15_l2slice2   GV100
// ltc_pm_tag_hit_fbp15_l2slice3    GV100
// ltc_pm_tag_miss_fbp15_l2slice3   GV100
//
// 2. Add these experiments to a text file for example:
//    mods_exp_fbp0_15.txt, mods_exp_fbp0_0.txt, mods_exp_fbp1_1.txt, etc
// 3. Run the pmlsplitter program to generate the register setting for the PM.
// ie. I created a temp dir at //hw/tools/pmlsplitter/gv100/Larry and created a cmd.sh file with
//     the following commands:
/* cmd.sh contents
# run this shell script from /home/scratch.lbangerter_maxwell/src/hw/tools/pmlsplitter/bin/linux
# ie ../../gv100/Larry/cmd.sh
#
# -i = Include location of the experiments.pml
# -p = location of the Programming guide
# -floorsweeping = location of the floorsweeping xml file
# -o = output directory
# -chip = the GPU to generate the experiments
# -output_format = specify mods style of output results.
# -e = the input file containing a list of the experiments to generate.
# -disable_add_clocks = pmlsplitter by default adds dummy experiments for each clock domain.
#  Use this option to disable it.
#pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -floorsweeping ../../gv100/Larry/floorsweeping_gv100.xml -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp0_15.txt //$
# Create a set of experiments for a single slice on FBPA 0
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp0_0_s0_s0.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_0_0_s0.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp0_0_s1_s1.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_0_0_s1.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp0_0_s2_s2.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_0_0_s2.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp0_0_s3_s3.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_0_0_s3.xml
# Create a set of experiments for all 4 slices on each FBPA
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp0_0.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_0_0.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp1_1.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_1_1.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp2_2.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_2_2.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp3_3.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_3_3.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp4_4.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_4_4.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp5_5.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_5_5.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp6_6.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_6_6.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp7_7.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_7_7.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp8_8.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_8_8.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp9_9.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_9_9.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp10_10.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_10_10.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp11_11.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_11_11.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp12_12.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_12_12.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp13_13.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_13_13.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp14_14.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_14_14.xml
pmlsplitter -disable_add_clocks -i ../../../perfalyze/chips/gv100/pml_files/experiments.pml -p ../../chips/gv100/pm_programming_guide.txt -o ../../gv100/Larry -chip gv100 -output_format mods -e ../../gv100/Larry/mods_exp_fbp15_15.txt //$
mv ../../gv100/Larry/modspml_0000.xml ../../gv100/Larry/modspml_15_15.xml
*/

// Then from: //hw/tools/pmlsplitter/bin/linux  I ran "../../gv100/Larry/cmd.sh"
// 4. Using the output from pmlsplitter (modspml_*_*.xml) you can get the signal busIndex values
//    needed to program the PM and the proper registers that should be written to program each PM.
//
// To debug you can add the following line in gputest.js:LwdaL2Test()
/*
      Gpu.LogAccessRegions([
                    [ 0x0040415c, 0x00404160 ] //LW_PGRAPH_PRI_FE_PERFMON
                    ,[ 0x00140000, 0x0017e3cc ] // LW_PLTCG_LTC0 - LW_PLTCG_LTCS_LTSS_G_PRI_IQ_CG1
                    ,[ 0x001A0040, 0x001A1500 ] //LW_PERF_PMMFBP_*(0)
                    ,[ 0x001B4000, 0x001B4124 ]
                    ]);
*/
// Note: For more supporting documentation please refer to http://lwbugs/1294252
// L2 Cache PerfMon(PM) signals:
// There is no header file where these are available to software, however you
// can find the signal programming in the hardware tree.
// hw/tools/pmlsplitter/gp102/pm_programming_guide.txt. Also refer to
// hw/lwgpu/manuals/pri_perf_pmm.ref
// also see: https://wiki.lwpu.com/gpuhwdept/index.php/Tool/HardwarePerformanceMonitor
#define GVLIT1_SIG_L2C_REQUEST_ACTIVE_S0_S2 0x0B // BusIndex="11" for even slices 0, 2, 4, 6
#define GVLIT1_SIG_L2C_HIT_S0_S2            0x12 // BusIndex="18" for even slices 0, 2, 4, 6
#define GVLIT1_SIG_L2C_MISS_S0_S2           0x13 // BusIndex="19" for even slices 0, 2, 4, 6
#define GVLIT1_SIG_L2C_REQUEST_ACTIVE_S1_S3 0x2A // BusIndex="42" for odd slices 1, 3, 5, 7
#define GVLIT1_SIG_L2C_HIT_S1_S3            0x31 // BusIndex="49" for odd slices 1, 3, 5, 7
#define GVLIT1_SIG_L2C_MISS_S1_S3           0x32 // BusIndex="50" for odd slices 1, 3, 5, 7

// -----------------------------------------------------------------------------------------------
// GV100 has 8 chiplets, each chiplet has 4 LTCs, each LTC has 2 slices.
// GV100 Bus Signal index values
// Signal       chiplet     ltc     slice   busIndex    Gpu
// ------------ -------     ---     -----   --------    -----
// req_active   0/1         0/1     0/2     11 (0x0b)   GV100
// hit          0/1         0/1     0/2     18 (0x12)   GV100
// miss         0/1         0/1     0/2     19 (0x13)   GV100
// req_active   0/1         0/1     1/3     42 (0x2A)   GV100
// hit          0/1         0/1     1/3     49 (0x31)   GV100
// miss         0/1         0/1     1/3     50 (0x32)   GV100

// req_active   2/3         0/1     0/2     11 (0x0b)   GV100
// hit          2/3         0/1     0/2     18 (0x12)   GV100
// miss         2/3         0/1     0/2     19 (0x13)   GV100
// req_active   2/3         0/1     1/3     42 (0x2A)   GV100
// hit          2/3         0/1     1/3     49 (0x31)   GV100
// miss         2/3         0/1     1/3     50 (0x32)   GV100

// req_active   4/5         0/1     0/2     11 (0x0b)   GV100
// hit          4/5         0/1     0/2     18 (0x12)   GV100
// miss         4/5         0/1     0/2     19 (0x13)   GV100
// req_active   4/5         0/1     1/3     42 (0x2A)   GV100
// hit          4/5         0/1     1/3     49 (0x31)   GV100
// miss         4/5         0/1     1/3     50 (0x32)   GV100

// req_active   6/7         0/1     0/2     11 (0x0b)   GV100
// hit          6/7         0/1     0/2     18 (0x12)   GV100
// miss         6/7         0/1     0/2     19 (0x13)   GV100
// req_active   6/7         0/1     1/3     42 (0x2A)   GV100
// hit          6/7         0/1     1/3     49 (0x31)   GV100
// miss         6/7         0/1     1/3     50 (0x32)   GV100

// For gvxxx the register addresses have been moved around to accommodate more slices & FBPs.
// There are 4 slices per LTC, 2 LTCs per FBP, & 8 FBPs per GPU.
// So the FBP offset is now 0x4000.
// -----------------------------------------------------------------------------------------------
//|       C++ Code                  | PmlSplitter Experiment tags                |LW_PERF_PMMFBP_*|
//|FB           |L2Slice|Perfmon    |ltc?|_chiplet?|_fbp?|_l2slice?|event/trig0  |starting address|
//|partition    |FBP    |domain     |    |         |     |         |select       |                |
//|             |       |(pmIdx)    |    |         |     |         |             |                |
//|fbpNum*0x4000|       |pmIdx*0x200|    |         |     |         |             |                |
//|-----------------------------------------------------------------------------------------------|
//|   0         |  0    |  Ltc0(2)  |  0 |    0    |  0  |   0     |0x0b2c/0x0b2d| 0x002004??     |
//|   0         |  1    |  Ltc0(2)  |  0 |    0    |  0  |   1     |0x2a31/0x2a32| 0x002004??     |
//|   0         |  2    |  Ltc1(3)  |  1 |    0    |  0  |   2     |0x0b2c/0x0b2d| 0x002006??     |
//|   0         |  3    |  Ltc1(3)  |  1 |    0    |  0  |   3     |0x2a31/0x2a32| 0x002006??     |
//|   0         |  4    |  Ltc2(4)  |  2 |    0    |  1  |   0     |0x0b2c/0x0b2d| 0x002008??     |
//|   0         |  5    |  Ltc2(4)  |  2 |    0    |  1  |   1     |0x2a31/0x2a32| 0x002008??     |
//|   0         |  6    |  Ltc3(5)  |  3 |    0    |  1  |   2     |0x0b2c/0x0b2d| 0x00200a??     |
//|   0         |  7    |  Ltc4(5)  |  3 |    0    |  1  |   3     |0x2a31/0x2a32| 0x00200a??     |

//|   1         |  0    |  Ltc0(2)  |  0 |    1    |  2  |   0     |0x0b2c/0x0b2d| 0x002044??     |
//|   1         |  1    |  Ltc0(2)  |  0 |    1    |  2  |   1     |0x2a31/0x2a32| 0x002044??     |
//|   1         |  2    |  Ltc1(3)  |  1 |    1    |  2  |   2     |0x0b2c/0x0b2d| 0x002046??     |
//|   1         |  3    |  Ltc1(3)  |  1 |    1    |  2  |   3     |0x2a31/0x2a32| 0x002046??     |
//|   1         |  4    |  Ltc2(4)  |  2 |    1    |  3  |   0     |0x0b2c/0x0b2d| 0x002048??     |
//|   1         |  5    |  Ltc2(4)  |  2 |    1    |  3  |   1     |0x2a31/0x2a32| 0x002048??     |
//|   1         |  6    |  Ltc3(5)  |  3 |    1    |  3  |   2     |0x0b2c/0x0b2d| 0x00204a??     |
//|   1         |  7    |  Ltc4(5)  |  3 |    1    |  3  |   3     |0x2a31/0x2a32| 0x00204a??     |

//|   2         |  0    |  Ltc0(2)  |  0 |    2    |  4  |   0     |0x0b2c/0x0b2d| 0x002084??     |
//|   2         |  1    |  Ltc0(2)  |  0 |    2    |  4  |   1     |0x2a31/0x2a32| 0x002084??     |
//|   2         |  2    |  Ltc1(3)  |  1 |    2    |  4  |   2     |0x0b2c/0x0b2d| 0x002086??     |
//|   2         |  3    |  Ltc1(3)  |  1 |    2    |  4  |   3     |0x2a31/0x2a32| 0x002086??     |
//|   2         |  4    |  Ltc2(4)  |  2 |    2    |  5  |   0     |0x0b2c/0x0b2d| 0x002088??     |
//|   2         |  5    |  Ltc2(4)  |  2 |    2    |  5  |   1     |0x2a31/0x2a32| 0x002088??     |
//|   2         |  6    |  Ltc3(5)  |  3 |    2    |  5  |   2     |0x0b2c/0x0b2d| 0x00208a??     |
//|   2         |  7    |  Ltc4(5)  |  3 |    2    |  5  |   3     |0x2a31/0x2a32| 0x00208a??     |

//...

//|   7         |  0    |  Ltc0(2)  |  0 |    7    |  14 |   0     |0x0b2c/0x0b2d| 0x0021c4??     |
//|   7         |  1    |  Ltc0(2)  |  0 |    7    |  14 |   1     |0x2a31/0x2a32| 0x0021c4??     |
//|   7         |  2    |  Ltc1(3)  |  1 |    7    |  14 |   2     |0x0b2c/0x0b2d| 0x0021c6??     |
//|   7         |  3    |  Ltc1(3)  |  1 |    7    |  14 |   3     |0x2a31/0x2a32| 0x0021c6??     |
//|   7         |  4    |  Ltc2(4)  |  2 |    7    |  15 |   0     |0x0b2c/0x0b2d| 0x0021c8??     |
//|   7         |  5    |  Ltc2(4)  |  2 |    7    |  15 |   1     |0x2a31/0x2a32| 0x0021c8??     |
//|   7         |  6    |  Ltc3(5)  |  3 |    7    |  15 |   2     |0x0b2c/0x0b2d| 0x0021ca??     |
//|   7         |  7    |  Ltc4(5)  |  3 |    7    |  15 |   3     |0x2a31/0x2a32| 0x0021ca??     |

/*************************************************************************************************
 * GV100GpuPerfMon Implementation
 * This is a very thin wrapper around GF100GpuPerfMon to report the new offsets that are specific
 * to Volta.
 ************************************************************************************************/
GV100GpuPerfmon::GV100GpuPerfmon(GpuSubdevice *pSubdev) : GpuPerfmon(pSubdev)
{
}

//------------------------------------------------------------------------------------------------
//return the offset from the base FBP PMM register to the register for subsequent FBPs
/*virtual*/
UINT32 GV100GpuPerfmon::GetFbpPmmOffset()
{
    return 0x4000;
}

/*************************************************************************************************
 * GVLit1L2CacheExperiment Implementation
 *
 ************************************************************************************************/

//------------------------------------------------------------------------------------------------
GVLit1L2CacheExperiment::GVLit1L2CacheExperiment
(
   UINT32 fbpNum,
   UINT32 l2Slice,
   GpuPerfmon* pPerfmon
)
: GML2CacheExperiment(fbpNum, l2Slice, pPerfmon)
{
    if (l2Slice & 0x01) // odd slices
    {
        m_L2Bsi.ltcActive = GVLIT1_SIG_L2C_REQUEST_ACTIVE_S1_S3;
        m_L2Bsi.ltcHit = GVLIT1_SIG_L2C_HIT_S1_S3;
        m_L2Bsi.ltcMiss = GVLIT1_SIG_L2C_MISS_S1_S3;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    else // even slices
    {
        m_L2Bsi.ltcActive = GVLIT1_SIG_L2C_REQUEST_ACTIVE_S0_S2;
        m_L2Bsi.ltcHit = GVLIT1_SIG_L2C_HIT_S0_S2;
        m_L2Bsi.ltcMiss = GVLIT1_SIG_L2C_MISS_S0_S2;
        m_L2Bsi.pmTrigger = LW_PERF_PMMFBP_LTC0_SIGVAL_PMA_TRIGGER;
    }
    /**********************************************************************************************
    Debug, please don't remove. We use this to debug the pmlsplitter files during development
    pPerfmon->SetPrintPri(Tee::PriNormal);
    REGISTER_RANGE NewRegions[] =
    {
        { 0x0017e350, 0x0017e350 }, // LW_PLTCG_LTCS_LTSS_MISC_LTC_LTS_PM broadcast register
        { 0x0040415c, 0x0040415c }, // LW_PGRAPH_PRI_FE_PERFMON
        { 0x00246018, 0x00246018 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 0)
        { 0x00246218, 0x00246218 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 1)
        { 0x00246418, 0x00246418 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 2)
        { 0x00246618, 0x00246618 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 3)
        { 0x00246818, 0x00246818 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 4)
        { 0x00246a18, 0x00246a18 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 5)
        { 0x00246c18, 0x00246c18 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 6)
        { 0x00246e18, 0x00246e18 }, //LW_PERF_PMMFBPROUTER_CG2 (chiplet 7)

        { 0x0024A008, 0x0024A008 }, //LW_PERF_PMASYS_TRIGGER_GLOBAL
        { 0x0024A024, 0x0024A024 }, //LW_PERF_PMASYS_FBP_TRIGGER_START_MASK
        { 0x0024A044, 0x0024A044 }, //LW_PERF_PMASYS_FBP_TRIGGER_STOP_MASK
        { 0x0024A10C, 0x0024A10C }, //LW_PERF_PMASYS_FBP_TRIGGER_CONFIG_MIXED_MODE_ENGINE

        { 0x00200500, 0x00200500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 0)
        { 0x00200700, 0x00200700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 0)
        { 0x00200900, 0x00200900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 0)
        { 0x00200b00, 0x00200b00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 0)
        { 0x00200440, 0x002004E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 0)
        { 0x00200640, 0x002006E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 0)
        { 0x00200840, 0x002008E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 0)
        { 0x00200a40, 0x00200aE0 }, //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 0)

        { 0x00204500, 0x00204500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 1)
        { 0x00204700, 0x00204700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 1)
        { 0x00204900, 0x00204900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 1)
        { 0x00204b00, 0x00204b00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 1)
        { 0x00204440, 0x002044E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 1)
        { 0x00204640, 0x002046E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 1)
        { 0x00204840, 0x002048E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 1)
        { 0x00204a40, 0x00204aE0 }, //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 1)

        { 0x00208500, 0x00208500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 2)
        { 0x00208700, 0x00208700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 2)
        { 0x00208900, 0x00208900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 2)
        { 0x00208b00, 0x00208b00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 2)
        { 0x00208440, 0x002084E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 2)
        { 0x00208640, 0x002086E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 2)
        { 0x00208840, 0x002088E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 2)
        { 0x00208a40, 0x00208aE0 }, //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 2)

        { 0x0020c500, 0x0020c500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 3)
        { 0x0020c700, 0x0020c700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 3)
        { 0x0020c900, 0x0020c900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 3)
        { 0x0020cb00, 0x0020cb00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 3)
        { 0x0020c440, 0x0020c4E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 3)
        { 0x0020c640, 0x0020c6E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 3)
        { 0x0020c840, 0x0020c8E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 3)
        { 0x0020ca40, 0x0020caE0 }, //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 3)

        { 0x00210500, 0x00210500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 4)
        { 0x00210700, 0x00210700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 4)
        { 0x00210900, 0x00210900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 4)
        { 0x00210b00, 0x00210b00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 4)
        { 0x00210440, 0x002104E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 4)
        { 0x00210640, 0x002106E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 4)
        { 0x00210840, 0x002108E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 4)
        { 0x00210a40, 0x00210aE0 }, //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 4)

        { 0x00214500, 0x00214500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 5)
        { 0x00214700, 0x00214700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 5)
        { 0x00214900, 0x00214900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 5)
        { 0x00214b00, 0x00214b00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 5)
        { 0x00214440, 0x002144E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 5)
        { 0x00214640, 0x002146E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 5)
        { 0x00214840, 0x002148E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 5)
        { 0x00214a40, 0x00214aE0 }, //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 5)

        { 0x00218500, 0x00218500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 6)
        { 0x00218700, 0x00218700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 6)
        { 0x00218900, 0x00218900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 6)
        { 0x00218b00, 0x00218b00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 6)
        { 0x00218440, 0x002184E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 6)
        { 0x00218640, 0x002186E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 6)
        { 0x00218840, 0x002188E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 6)
        { 0x00218a40, 0x00218aE0 }, //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 6)

        { 0x0021c500, 0x0021c500 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 2, fbp = 7)
        { 0x0021c700, 0x0021c700 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 3, fbp = 7)
        { 0x0021c900, 0x0021c900 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 4, fbp = 7)
        { 0x0021cb00, 0x0021cb00 }, //LW__PERF_PMMFBP_CLAMP_CYA_CONTROL (pmIdx = 5, fbp = 7)
        { 0x0021c440, 0x0021c4E0 }, //LW_PERF_PMMFBP_* (pmIdx = 2, fbp = 7)
        { 0x0021c640, 0x0021c6E0 }, //LW_PERF_PMMFBP_* (pmIdx = 3, fbp = 7)
        { 0x0021c840, 0x0021c8E0 }, //LW_PERF_PMMFBP_* (pmIdx = 4, fbp = 7)
        { 0x0021ca40, 0x0021caE0 } //LW_PERF_PMMFBP_* (pmIdx = 5, fbp = 7)
    };
    g_GpuLogAccessRegions.clear();
    for (size_t i = 0; i < NUMELEMS(NewRegions); i++)
        g_GpuLogAccessRegions.push_back(NewRegions[i]);
    *********************************************************************************************/
}

//------------------------------------------------------------------------------------------------
// Callwlate the proper PerfMon domain index for the LTC based on the current L2 slice.
// The perfmon domains are ordered alphabetically and have the following values:
// GPxxx and older: fbp=0,          ltc0=1, ltc1=2,                 rop0=3, rop1=4.
// GVxxx:           fbp0=0, fbp1=1, ltc0=2, ltc1=3, ltc2=4, ltc3=5, rop0=6, rop1=7.
// The L2 slices are spread across all Ltc domains.
// L2 slices 0/1 use ltc0, slices 2/3 use ltc1, slices 4/5 use ltc2, slices 6/7 use ltc3.
// PmIdx is the perfmon domain index, so set the PmIdx based on the slice and use the Fbp number to
// select the specific permon in that domain.
/* virtual */
UINT32 GVLit1L2CacheExperiment::CalcPmIdx()
{
    switch (GetL2Slice())
    {
        case 0:
        case 1:
            return 2;
        case 2:
        case 3:
            return 3;
        case 4:
        case 5:
            return 4;
        case 6:
        case 7:
            return 5;
        default:
            MASSERT(!"Dont know what PerfMon index to use.");
            return 0;
    }
}

//----------------------------------------------------------------------------
//! \brief Create a L2 Cache Experiment
//!
//! \param FbpNum   : FBP number for the experiment
//! \param L2Slice  : L2 Cache slice for the experiment
//! \param pHandle  : Pointer to the newly created experiment
//!
//! \return OK if creating the experiment was successful, not OK otherwise
//!
RC GV100GpuPerfmon::CreateL2CacheExperiment
(
    UINT32 FbpNum,
    UINT32 L2Slice,
    const GpuPerfmon::Experiment **pHandle
)
{
    RC rc;
    unique_ptr<Experiment> newExperiment;
    GpuSubdevice *pSubdev = GetGpuSubdevice();
    const UINT32 fbpCount = pSubdev->GetFB()->GetFbpCount();

    *pHandle = NULL;

    if (fbpCount <= FbpNum)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (L2Slice >= pSubdev->GetFB()->GetMaxL2SlicesPerFbp())
    {
        return RC::SOFTWARE_ERROR;
    }

    if (Platform::GetSimulationMode() == Platform::RTL ||
        Platform::GetSimulationMode() == Platform::Hardware)
    {
        newExperiment.reset((Experiment*)(new GVLit1L2CacheExperiment(
                    FbpNum, L2Slice, this)));
        CHECK_RC(AddExperiment(newExperiment.get()));
        *pHandle = newExperiment.release();
    }
    return rc;
}

