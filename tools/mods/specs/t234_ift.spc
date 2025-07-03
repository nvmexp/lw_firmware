/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/t234_ift.spc#11 $");

g_SpecArgs = "-disable_composition_mode";

//------------------------------------------------------------------------------
function ScaleTestTimeMs(max, frac, goldenGeneration)
{
    return scaleLoopMs(max, frac, goldenGeneration).LoopMs;
}
Log.Never("ScaleTestTimeMs");

//------------------------------------------------------------------------------
function userSpec(testList, perfPoints)
{
    if (Golden.Store == Golden.Action)
    {
        // Do only one pstate for gpugen.js.
        perfPoints = [perfPoints[0]];
    }

    for (let perfPtIdx = 0; perfPtIdx < perfPoints.length; perfPtIdx++)
    {
        testList.AddTest("SetPState", { InfPts: perfPoints[perfPtIdx] });

        const isLastPState = perfPtIdx === perfPoints.length - 1;
        const testFraction = (perfPtIdx === 0) ? 1.0 : 0.1;

        AddTestsInOnePerfPoint(testList, perfPtIdx, isLastPState, testFraction,
                               Golden.Action === Golden.Store);
    }
}

//------------------------------------------------------------------------------
function ThermalResSpec(testList, perfPoints)
{
    testList.AddTest("SetPState", { InfPts: perfPoints[0] });
    testList.AddTest(
        "TegraThermalResistance",
        {
            "SampleDelayMs": 50,
            "IdleDelayMs": 1000,
            "VirtualTestId": "T489 with T630",
            "BgTestNum":"630",
            "IdleTimeMs": 2000,
            "LoadTimeMs": 7000,
            "PrintSummary": true,
            "CheckpointTimesMs": [7000],
            "Monitors": [
                {
                    ThermChannelName: "GPU_MAX", // Use GPU thermal sensors
                    MaxIdleTempDegC: 45,
                    RLimits: [
                        { MinR: 4.000e-5, MaxR: 1e-3 }
                    ]
                },
                {
                    ThermChannelName: "CPU_MAX", // Use CPU thermal sensors
                    MaxIdleTempDegC: 45,
                    RLimits: [
                        { MinR: 4.000e-5, MaxR: 1e-3 }
                    ]
                },
                {
                    ThermChannelName: "CHIP_MAX",  // Use all thermal sensors on the chip
                    MaxIdleTempDegC: 50,
                    RLimits: [
                        { MinR: 4.123e-5, MaxR: 1e-3 }
                    ]
                }
            ],
            "PowerLimit": { MaxIdleMw: 20000, MinAvgMw: 60000, MaxAvgMw: 100000 },
            "StabilityCriteria":false,
            "Verbose":  0
        }
    );
}

//------------------------------------------------------------------------------
function AddTestsInOnePerfPoint(testList, perfPtIdx, isLastPState, testFraction, goldenGeneration)
{
    if (TegraRunFmaxTests(perfPtIdx))
    {
        testList.AddTests([
             ["WfMatsMedium",     { MaxFbMb:      256,
                                    CpuTestBytes: 4*1024*1024 }]
            ,["NewWfMatsNarrow",  { MaxFbMb:    32,
                                    InnerLoops: 8 }]
            ,["NewWfMatsBusTest", { MaxFbMb: 256 }]
            ,["NewWfMatsCEOnly",  { MaxFbMb: 256 }]
            ]);
    }

    if (TegraRunGpuTests(perfPtIdx))
    {
        testList.AddTests([
             "CpyEngTest"
            ,"I2MTest"
            ,"Random2dFb"
            ,"WriteOrdering"
            ]);
    }

    if (TegraRunFmaxTests(perfPtIdx))
    {
        testList.AddTests([
            "PerfSwitch"
            ]);
    }

    if (TegraRunCoreTests(perfPtIdx))
    {
        testList.AddTests([
             "TegraVic"
            ,"VicLdcTnr"
        ]);
        if (Golden.Action === Golden.Check)
        {
            testList.AddTests([
                ["TegraLwJpg", { SkipEngineMask: 1,
                                 VirtualTestId:  "TegraLwJpgInstance1" }]
            ]);
        }
        testList.AddTests([
             ["TegraLwJpg", { SkipEngineMask: 2,
                              VirtualTestId:  "TegraLwJpgInstance0" }]
            ,"TegraLwdec"
            ,"TegraLwOfa"
            ,"TegraLwEnc"
            ]);
    }

    if (TegraRunDisplayTests(perfPtIdx))
    {
        testList.AddTests([
            "LwDisplayRandom",
            "SorLoopbackCDT",
            "ConfigurableDisplayHTOL"
        ]);
    }

    if (TegraRunGpuTests(perfPtIdx))
    {
        testList.AddTests([
             ["GlrA8R8G8B8",   scaleLoops(40*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,["GlrFsaa2x",     scaleLoops(40*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,["GlrFsaa4x",     scaleLoops(40*20, 20, Math.min(0.75, testFraction), perfPtIdx, goldenGeneration)]
            ,["GlrMrtRgbU",    scaleLoops(40*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,["GlrMrtRgbF",    scaleLoops(40*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,["GlrY8",         scaleLoops(40*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,["GlrR5G6B5",     scaleLoops(40*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,["GlrFsaa8x",     scaleLoops(40*20, 20, Math.min(0.75, testFraction), perfPtIdx, goldenGeneration)]
            ,["GlrLayered",    scaleLoops(40*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,"GLComputeTest"
            ,"PathRender"
            ,"FillRectangle"
            ,"MsAAPR"
            ,"GLPRTir"
            ,["GLStress",      scaleLoopMs(15000, testFraction, goldenGeneration)]
            ,["GLStressZ",     scaleLoopMs(5000, testFraction, goldenGeneration)]
            ,["GLStressPulse", scaleLoopMs(5000, testFraction, goldenGeneration)]
            ,["GLRandomCtxSw", scaleLoops(30*20, 20, testFraction, perfPtIdx, goldenGeneration)]
            ,"VkStress"
            ,"VkStressCompute"
            ,"VkStressSCC"
            ,["VkStressMesh",  { LoopMs: 500 }]
            ,["VkStressPulse", { LoopMs: 5000 }]
            ,["VkPowerStress", { LoopMs: 5000 }]
            ,["VkHeatStress",  { LoopMs: 5000 }]
            ,["VkStressRay",   { LoopMs: 5000 }]
            ,"VkStressFb"
            ,"VkFusionRay"
            ,"VkFusionCombi"
            ]);
    }

    if (TegraRunFmaxTests(perfPtIdx))
    {
        testList.AddTests([
            ["NewLwdaMatsTest",        { Iterations: 2 }]
           ,["NewLwdaMatsRandom",      { MaxFbMb: 256 }]
           ,["NewLwdaMatsStress",      { MaxFbMb: 256 }]
           ,["NewLwdaMatsRandomPulse", { MaxFbMb: 256 }]
           ,["NewLwdaMatsReadOnly",    { MaxFbMb: 256 }]
           ,"LwdaXbar"
           ,"LwdaXbar2"
           ,"LwdaTTUStress"
           ,"LwdaTTUBgStress"
           ,"TegraFullLoad"
           ]);
    }

    if (TegraRunGpuTests(perfPtIdx))
    {
        testList.AddTests([
             "NewLwdaL2Mats"
            ,"LwdaCbu"
            ,"LwdaSubctx"
            ,"LwdaL1Tag"
            ,["CaskLinpackSgemm",           { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackIgemm",           { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackDgemm",           { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackHgemm",           { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackHMMAgemm",        { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackIMMAgemm",        { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackDMMAgemm",        { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackE8M10",           { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackE8M7",            { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackBMMA",            { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackINT4",            { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackSparseHMMA",      { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackSparseIMMA",      { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseSgemm",      { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseIgemm",      { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseDgemm",      { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseHgemm",      { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseHMMAgemm",   { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseIMMAgemm",   { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseDMMAgemm",   { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseE8M10",      { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseE8M7",       { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseBMMA",       { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseINT4",       { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseSparseHMMA", { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["CaskLinpackPulseSparseIMMA", { RuntimeMs: ScaleTestTimeMs(15000, testFraction, goldenGeneration) }]
            ,["LwdaRandom",                 scaleLoops(30*75, 75, testFraction, perfPtIdx, goldenGeneration)]
            ]);
    }

    testList.AddTest("TegraEccCheck");
}

var g_SpecFileIsSupported = SpecFileIsSupportedSocOnly;

// vim: ft=javascript
