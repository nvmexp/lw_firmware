/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/cheetah.spc#77 $");

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called
g_TestMode = (1<<4); // TM_ENGR

function ScaleTestTimeMs(max, frac, goldenGeneration)
{
    return scaleLoopMs(max, frac, goldenGeneration).LoopMs;
}

Log.Never("ScaleTestTimeMs");

//------------------------------------------------------------------------------
// Engr (engineering tests).
//------------------------------------------------------------------------------
function addModsGpuTests(testList, perfPoints)
{
    testList.TestMode = 1 << 4; // TM_ENGR

    const goldenGeneration = Golden.Action === Golden.Store;
    if (goldenGeneration)
    {
        // Do just one pstate for gpugen.js.
        perfPoints = perfPoints.slice(0, 1);
    }

    for (let perfPtIdx = 0; perfPtIdx < perfPoints.length; perfPtIdx++)
    {
        // At certain perf points, due to low voltage, we cannot enable display.
        // We detect those perf points and skip them here. We need to use null
        // display while running at those perf points.
        if (!IsDefaultDisplayPossible(perfPoints[perfPtIdx]))
        {
            Out.Printf(Out.PriWarn, "Skipping perfpoint "+ perfPtIdx + " since display cannot" +
                       " be enabled. Use -null_display to run at this perf point\n")
            continue;
        }

        testList.AddTest("SetPState", { InfPts : perfPoints[perfPtIdx] });

        const isLastPState = (perfPtIdx === perfPoints.length - 1);
        const testFraction = (perfPtIdx === 0) ? 1.0 : 0.1;

        addEngrTestsOnePState(testList, perfPtIdx, isLastPState, testFraction,
                              goldenGeneration);
    }

    // Override test duration of optional tests (must use -add to run these):
    testList.AddTestsArgs([
         ["DisplayCrcChecker", { NumOfFrames : 10,
                                 UseRasterFromEDID : true,
                                 EnableDMA : false } ]
        ]);
}

//------------------------------------------------------------------------------
function addEngrTestsOnePState
(
    testList,           // (TestList)
    perfPointIdx,       // (integer)  how many pstates we've tested already
    isLastPState,       // (bool)     if true, add one-time late tests
    testFraction,       // (float)    fraction of full test time
    goldenGeneration    // (bool)     golden generation phase
)
{
    if (TegraRunFmaxTests(perfPointIdx))
    {
        testList.AddTests([
            "CheckConfig"
            ,["WfMatsMedium",    { CpuTestBytes: 6*1024*1024 }]
            ,["NewWfMatsNarrow", { InnerLoops: 8 }]
            // ,"CheckHDCP"
            ]);
    }

    if (TegraRunGpuTests(perfPointIdx))
    {
        testList.AddTests([
            "CpyEngTest"
            ,"I2MTest"
            ,"Random2dFb"
            ,"WriteOrdering"
            ]);
    }

    if (TegraRunFmaxTests(perfPointIdx))
    {
        testList.AddTests([
            "PerfSwitch"
            ,"TegraCec"
            ]);
    }

    if (TegraRunFmaxTests(perfPointIdx))
    {
        testList.AddTests([
             ["LwLinkBwStressTegra", { TestConfiguration: { Loops: 50 } }]
            ,["LwlbwstBgPulse", { TestConfiguration: { Loops: 50 } }]
            ,["LwlbwstCpuTraffic", { TestConfiguration: { Loops: 50 } }]
            ,"LwLinkState"
            ]);
    }

    let graphicsList = [];
    if (TegraRunCoreTests(perfPointIdx))
    {
        graphicsList = graphicsList.concat([
             "TegraVic"
            ,"VicLdcTnr"
            ,"Lwjpg"
            ,"TegraLwJpg"
            ,"TegraLwdec"
            ,"LWENCTest"
            ,"TegraLwEnc"
            ,"TegraLwOfa"
            ,"TegraTsec"
            ]);
    }

    if (TegraRunDisplayTests(perfPointIdx))
    {
        testList.AddTestsWithParallelGroup(PG_DISPLAY, [
             "TegraWin"
            ],
            { display: true });
    }

    if (TegraRunGpuTests(perfPointIdx))
    {
        graphicsList = graphicsList.concat([
             ["GlrA8R8G8B8",   scaleLoops(40*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,["GlrFsaa2x",     scaleLoops(40*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,["GlrFsaa4x",     scaleLoops(40*20, 20, Math.min(0.75, testFraction), perfPointIdx, goldenGeneration)]
            ,["GlrMrtRgbU",    scaleLoops(40*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,["GlrMrtRgbF",    scaleLoops(40*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,["GlrY8",         scaleLoops(40*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,["GlrR5G6B5",     scaleLoops(40*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,["GlrFsaa8x",     scaleLoops(40*20, 20, Math.min(0.75, testFraction), perfPointIdx, goldenGeneration)]
            ,["GlrLayered",    scaleLoops(40*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,"GLComputeTest"
            ,"PathRender"
            ,"FillRectangle"
            ,"MsAAPR"
            ,"GLPRTir"
            ]);
    }
    if (graphicsList.length > 0)
    {
        testList.AddTestsWithParallelGroup(PG_GRAPHICS, graphicsList);
        testList.AddTest("SyncParallelTests", { ParallelId: PG_GRAPHICS });
    }
    if (TegraRunDisplayTests(perfPointIdx))
    {
        testList.AddTest("SyncParallelTests", { ParallelId: PG_DISPLAY });
    }

    if (TegraRunGpuTests(perfPointIdx))
    {
        testList.AddTests([
             "Elpg"
            ,"ElpgGraphicsStress"
            ,["GLStress",      scaleLoopMs(15000, testFraction, goldenGeneration)]
            ,["GLStressZ",     scaleLoopMs(5000, testFraction, goldenGeneration)]
            ,["GLStressPulse", scaleLoopMs(5000, testFraction, goldenGeneration)]
            ,["GLRandomCtxSw", scaleLoops(30*20, 20, testFraction, perfPointIdx, goldenGeneration)]
            ,"VkStress"
            ,"VkStressCompute"
            ,"VkStressSCC"
            ,["VkStressMesh",  { LoopMs: 500 }]
            ,["VkStressPulse", { LoopMs: 5000 }]
            ,["VkPowerStress", { LoopMs: 5000 }]
            ,["VkStressRay",   { LoopMs: 5000 }]
            ,["VkFusionRay",   { RuntimeMs: 5000 }]
            ,["VkFusionCombi", { RuntimeMs: 5000 }]
            ]);
    }

    if (TegraRunFmaxTests(perfPointIdx))
    {
        testList.AddTests([
            ["NewLwdaMatsTest",   { Iterations: 20 } ]
           ,["NewLwdaMatsRandom", { Iterations: 20 } ]
           ,"TegraFullLoad"
           ]);
    }

    if (TegraRunFmaxTests(perfPointIdx))
    {
        testList.AddTests([
             "LwdaXbar"
            ,"LwdaXbar2"
            ]);
    }

    if (TegraRunGpuTests(perfPointIdx))
    {
        testList.AddTests([
             "LwdaCbu"
            ,"LwdaSubctx"
            ,"LwdaL1Tag"
            ,["LwdaLinpackDgemm",           { CheckLoops: 5, TestConfiguration: {Loops: 10} }]
            ,["LwdaLinpackHgemm",           { RuntimeMs: ScaleTestTimeMs(10000, testFraction, goldenGeneration) }]
            ,["LwdaLinpackHMMAgemm",        { RuntimeMs: ScaleTestTimeMs(10000, testFraction, goldenGeneration) }]
            ,["LwdaLinpackSgemm",           { RuntimeMs: ScaleTestTimeMs(10000, testFraction, goldenGeneration) }]
            ,["LwdaLinpackIgemm",           { RuntimeMs: ScaleTestTimeMs(10000, testFraction, goldenGeneration) }]
            ,["LwdaLinpackIMMAgemm",        { RuntimeMs: ScaleTestTimeMs(10000, testFraction, goldenGeneration) }]
            ,["LwdaLinpackPulseDP",         { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["LwdaLinpackPulseSP",         { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["LwdaLinpackPulseHP",         { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["LwdaLinpackPulseHMMA",       { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["LwdaLinpackPulseIP",         { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["LwdaLinpackPulseIMMA",       { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
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
            ,["CaskLinpackPulseSparseIMMA", { RuntimeMs: ScaleTestTimeMs(5000,  testFraction, goldenGeneration) }]
            ,["LwdaRandom",                 scaleLoops(30*75, 75, testFraction, perfPointIdx, goldenGeneration)]
            ,"LwdaTTUStress"
            ,"LwdaTTUBgStress"
            ,"NewLwdaL2Mats"
            ,"MMERandomTest"
            ,"EccSM"
            ,"EccSMShm"
            ,"EccTex"
            ]);
    }

    testList.AddTest("TegraEccCheck");
}

var g_SpecFileIsSupported = SpecFileIsSupportedSocOnly;

// vim: ft=javascript
