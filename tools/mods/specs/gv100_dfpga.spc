/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
AddFileId(Out.PriNormal, "$Id: //sw/dev/gpu_drv/chips_a/diag/mods/gpu/js/dfpga.spc#1 $");

g_SpecArgs = "-pmu_bootstrap_mode 1 -exelwte_rm_devinit_on_pmu 0 -use_mods_console";

// Besides RM Tests, dvs uses this file to regress simmods on dfpga. It does 2 ilwocations of mods using the
// following command lines:
//   mods -chip hwchip2.so -gpubios 0 gv100f_tmds_dp.rom gpugen.js -readspec gv100_dfpga.spc -spec dvsSpecTests
//   mods -chip hwchip2.so -gpubios 0 gv100f_tmds_dp.rom gputest.js -readspec gv100_dfpga.spc -spec dvsSpecTests
// You may find the most recent command line (given above) in //sw/dev/gpu_drv/chips_a/diag/mods/sim/dfpga_cmds.tcl

//------------------------------------------------------------------------------
// Short list of tests to regress in dvs.
function dvsSpecTests(testList)
{
    testList.AddTests([
         ["CpyEngTest"]
        ,["LwDisplayRandom", {
            "UseSmallRaster" : true,
            "TestConfiguration" : {
                "StartLoop" : 0,
                "Loops" : 200, // Number of frames
                "RestartSkipCount" : 10 // Number of frames per modeset
            }
        }]
        ,["SorLoopback", {
            "WaitForEDATA" : false,
            "IgnoreCrcMiscompares" : true
        }]
        ,"LwDisplayHtol"
    ]);
}
dvsSpecTests.specArgs = "";


