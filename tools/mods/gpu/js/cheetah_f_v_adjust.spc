/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013,2015,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/tegra_f_v_adjust.spc#8 $");

function TegraFVAdjust()
{
    // Example to set GPU clock, which is controlled by RM and thus
    // We have to go through GpuSubDevice
    // var subdev = new GpuSubdevice(0, 0);
    // CHECK_RC(subdev.SetClock(Gpu.ClkGpc2, Frequency.InInfo));
    // var freq = [];
    // CHECK_RC(subdev.GetClock(Gpu.ClkGpc2, freq));
    // Frequency.OutInfo = freq[Gpu.ClkActual];
    // Example to set CBUS, frequency value comes from possible_rates
    //var frequency = Soc.GetClock("override.c2bus", "");
    //Out.Printf(Out.PriAlways, "oFLOOR c2 F = %llu\n", frequency);
    //Soc.SetClock("override.c2bus", "", 528000000);
    //frequency = Soc.GetClock("override.c2bus", "");
    //Out.Printf(Out.PriAlways, "oFLOOR c2 F = %llu\n", frequency);
}

//------------------------------------------------------------------------------
// Engr (engineering tests).
//------------------------------------------------------------------------------
function userSpec(testList, PerfPtList)
{ 
    testList.TestMode = (1<<4); // TM_ENGR

    // pstates will be disabled for this .spc, mostly a noop.
    testList.AddTest("SetPState", { InfPts : PerfPtList[0] });  

    addEngrTests (testList, true);  // first run, default clocks

    if (Golden.Store !== Golden.Action)
    {
        testList.AddTest("RunUserFunc", {"UserFunc" : TegraFVAdjust});
        addEngrTests (testList, false);  // second run, hacked clocks
    }
}

//------------------------------------------------------------------------------
function addEngrTests
(
    testList        // (TestList)
    ,isFirstRun     // (bool), only true for first set of tests.     
)
{
    if (isFirstRun)
    {
        testList.AddTests([
            "CheckConfig"
            ]);
    } 
    var testFraction = 1.0;  // make this smaller for shorter test times

    testList.AddTests([
        "WfMatsMedium"
        ,["NewWfMatsNarrow", { MaxFbMb: (isFirstPState ? 32 : 20), 
                               InnerLoops: (isFirstPState ? 8 : 4)}]
        ]);

    if (isFirstRun)
    {
        testList.AddTests([ "CheckHDCP" ]);
    }

    testList.AddTests([
        "CpyEngTest"
        ,"I2MTest"
        ]);

    if (isFirstRun)
    {
        testList.AddTests([ "PerfSwitch" ]);
    }

    testList.AddTests([
        "TegraWin"
        ,"Random2dFb"
        ,"TegraCec"
        ,["GlrA8R8G8B8",   scaleLoops(40*20, 20, testFraction, 0, false)]
        ,["GlrFsaa2x",     scaleLoops(40*20, 20, testFraction, 0, false)]
        ,["GlrFsaa4x",     scaleLoops(40*20, 20, testFraction, 0, false)]
        ,["GlrMrtRgbU",    scaleLoops(40*20, 20, testFraction, 0, false)]
        ,["GlrMrtRgbF",    scaleLoops(40*20, 20, testFraction, 0, false)]
        ,["GlrY8",         scaleLoops(40*20, 20, testFraction, 0, false)]
        ,["GlrR5G6B5",     scaleLoops(40*20, 20, testFraction, 0, false)]
        ,["GlrFsaa8x",     scaleLoops(40*20, 20, testFraction, 0, false)]
        ]);

    testList.AddTests([
        "Elpg"
        ,"ElpgGraphicsStress"
        ,["GLStress",      scaleLoopMs(15000, testFraction, 0, false)]
        ,["GLStressZ",     scaleLoopMs(5000, testFraction, 0, false)]
        ,["GLStressPulse", scaleLoopMs(5000, testFraction, 0, false)]
        ,["GLRandomCtxSw", scaleLoops(30*20, 20, testFraction, 0, false)]
        ,"GLCombustTest"
        ,"GLInfernoTest"
        ]);

    testList.AddTests([
        "NewLwdaMatsTest"
        ,["DPStressTest", scaleLoops(1000, 1, testFraction, 0, false)]
        ,"LwdaStress2"
        ,["LwdaLinpackDgemm",  { "CheckLoops": 100, TestConfiguration: {Loops: 1000}}]
        ,["LwdaRandom", scaleLoops(30*75, 75, testFraction, 0, false)]
        ,"MMERandomTest"
        ,"Bar1RemapperTest"
        ]);
}

var g_SpecFileIsSupported = SpecFileIsSupportedSocOnly;



