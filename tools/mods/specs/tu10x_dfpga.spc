/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

g_SpecArgs = "-pmu_bootstrap_mode 1 -exelwte_rm_devinit_on_pmu 0 -use_mods_console -skip_azalia_init -rmkey RMBandwidthFeature 0x40000";

// Besides RM Tests, dvs uses this file to regress simmods on dfpga. It does 2 ilwocations of mods using the
// following command lines:
//   mods -chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_no_ifr.rom gpugen.js -readspec gv100_dfpga.spc -spec dvsSpecTests
//   mods -chip hwchip2.so -gpubios 0 tu102f_fpga_sddr3_nopll_no_ifr.rom gputest.js -readspec gv100_dfpga.spc -spec dvsSpecTests
// You may find the most recent command line (given above) in //sw/dev/gpu_drv/chips_a/diag/mods/sim/dfpga_cmds.tcl

//------------------------------------------------------------------------------
// Short list of tests to regress in dvs.
function dvsSpecTests(testList)
{
    testList.AddTests([
        ["LwDisplayRandom", {
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
    //    ,"LwDisplayHtol"
        ,["LwDisplayDPC", {
            "DPCFilePath" : "800x600_dfpga.dpc"
        }]
    ]);
}
dvsSpecTests.specArgs = "";
