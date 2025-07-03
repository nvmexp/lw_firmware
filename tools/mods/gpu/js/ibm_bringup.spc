/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/ibm_bringup.spc#8 $");

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called
g_TestMode = (1<<2); // TM_MFG_BOARD

#include "fpk_comm.h"
g_SpecArgs = "-fb_working -iddq_check_ignore -hw_speedo_check_ignore -devid_check_ignore";
g_SpecArgs += " -adc_cal_check_ignore -adc_temp_err_check_ignore -skip_azalia_init -no_rcwd -power_feature 0x54455555 -null_display";
g_SpecArgs += " -lwlink_force_enable -run_on_error";
g_SpecArgs += " -only_family pascal -poll_interrupts -disable_pstate20 -pstate_disable  -lwlink_verbose 1"
g_SpecArgs += " -timeout_ms 10000";

//------------------------------------------------------------------------------
function userSpec(testList)
{
    var lwlbwsArgs = {"SkipBandwidthCheck":false, "ShowBandwidthData":true, "Verbose":true, "SurfSizeFactor":40, "CopyLoopCount":100 };
    testList.AddTests([
         // Ensure that the copy engines work before proceding (forbid sysmem
         // transfers, just ensure copy engine functionality).  Sysmem transfers
         // will be tested in LwLinkBwStress
         ["CpyEngTest", { TestConfiguration : {SurfaceWidth : 128, SurfaceHeight : 128 },
                          FancyPickerConfig : 
                            [ [ FPK_COPYENG_SRC_SURF, ["random", [1, CpyEngTest.prototype.FB_LINEAR], 
                                                                 [1, CpyEngTest.prototype.FB_TILED],
                                                                 [1, CpyEngTest.prototype.FB_BLOCKLINEAR]] ]
                             ,[ FPK_COPYENG_DST_SURF, ["random", [1, CpyEngTest.prototype.FB_LINEAR], 
                                                                 [1, CpyEngTest.prototype.FB_TILED],
                                                                 [1, CpyEngTest.prototype.FB_BLOCKLINEAR]] ] ] } ]                                                                                        
        ,["LwLinkBwStress",     lwlbwsArgs]
        ,["LwlbwsBeHammer",     lwlbwsArgs]
        ,["LwLinkBwStressTwod", lwlbwsArgs]

        // Now that we are sure that copy engines work and LwLink sysmem transfers work
        // ensure that Lwca works (lwca also does sysmem transfers that will occur
        // over lwlink)
        ,["LwdaRandom",       setLoops(15*75)]
        ,["LwdaLinpackDgemm",  { CheckLoops: 100, TestConfiguration: {Loops: 500}}]
        ,["LwdaLinpackHgemm",  { CheckLoops: 100, TestConfiguration: {Loops: 1000}}]
        ,["LwdaLinpackSgemm",  { CheckLoops: 100, TestConfiguration: {Loops: 1000}}]
        ,["LwdaLinpackPulseDP",{ CheckLoops: 100, TestConfiguration: {Loops: 500}}]
        ,["LwdaLinpackPulseHP",{ CheckLoops: 100, TestConfiguration: {Loops: 1000}}]
        ,["LwdaLinpackPulseSP",{ CheckLoops: 100, TestConfiguration: {Loops: 1000}}]
        ,"DPStressTest"
        //,"PerfSwitch"
        //,"ValidSkuCheck"

        // Now that LWCA works run it in parallel with LwLink
        ,["LwlbwsBgPulse",    lwlbwsArgs]

        // TwoD transfers to sysmem
        ,"Random2dNc"
    ]);
}

