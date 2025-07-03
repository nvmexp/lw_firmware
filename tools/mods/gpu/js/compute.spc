/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/compute.spc#123 $");

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called
g_TestMode = (1<<7); // TM_COMPUTE

//------------------------------------------------------------------------------
// Test specification for creating extended golden values
//
// To run this test spec, execute the following command:
//         "mods gpugen.js -readspec compute.spc -spec ExtGoldenSpec"
//------------------------------------------------------------------------------
function ExtGoldenSpec(testList, PerfPtList /* unused */)
{
    var subdev           = GpuDevMgr.GetGpuSubdeviceByGpuIdx(0);
    var pStates          = new Array;
    subdev.Perf.GetPStates(pStates);
    var firstPState      = Math.min.apply(Math, pStates);
    var perfPt           = { PStateNum : firstPState, LocationStr : "tdp" };
    var pStateZero       = 0;
    var pStateIdx        = 0;
    var isLastPState     = true;
    var testFraction     = 1.0;
    var goldenGeneration = true;

    var DPStressArgs = setLoops(30*1000);
    DPStressArgs.MaxAllowedErrorBits = 24;

    // Override test duration of some tests:
    testList.AddTestsArgs([
        ["LwdaRandom", setLoops(60*75*500)]
        ,["GlrA8R8G8B8", setLoops(4800)]
        ,["DPStressTest", DPStressArgs]
        ]);

    testList.AddTest("SetPState", { InfPts : perfPt });

    addComputeTestsOnePState(testList, pStateZero, pStateIdx, isLastPState,
                             testFraction, goldenGeneration, false, false);
}

//------------------------------------------------------------------------------
// Compute is a test suite for functional testing of the "Tesla" product line of
// compute server boards.  It is like long, but with a more complete list of
// tests (display, audio and CheetAh-specific tests are excluded, though) and a
// longer test time on the lwca tests.
//------------------------------------------------------------------------------
function addModsGpuTests(testList, PerfPtList)
{
    testList.TestMode = (1<<7); // TM_COMPUTE

    var goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        // Do just one pstate for gpugen.js.
        PerfPtList = PerfPtList.slice(0,1);
        goldenGeneration = true;
    }

    for (var i = 0; i < PerfPtList.length; i++)
    {
        var perfPt    = PerfPtList[i];
        var pStateNum = 0;
        if (perfPt !== null)
            pStateNum = perfPt.PStateNum;

        testList.AddTest("SetPState", { InfPts : perfPt });

        var isLastPState = (PerfPtList.length-1 == i);
        var testFraction = 1.0;

        addComputeTestsOnePState(testList, pStateNum, i, isLastPState,
                                 testFraction, goldenGeneration, false, false);
    }
}

//------------------------------------------------------------------------------
function addComputeTestsOnePState
(
    testList              // (TestList)
    ,pState               // (integer)  pstate for which tests are being added
    ,pStateIdx            // (integer)  how many pstates we've tested already
    ,isLastPState         // (bool)     if true, add one-time late tests
    ,testFraction         // (float)    fraction of full test time
    ,goldenGeneration     // (bool)     golden generation phase
    ,onlyAddOQATests      // (bool)     if true, add only OQA tests
    ,longDurationRuns     // (bool)     if true, run tests for longer duration
    ,setOfVariants        // (string)   which set of Linpack variants to run,
                          //              e.g., "FCT"
    ,reducePower          // (bool)     if true, run high-power tests for
                          //              only a short time
    ,omitNeverFailTests   // (bool)     whether to add tests that never fail
                          //              on the production line
    ,reduceTemperature    // (bool)     whether to reduce temperature by using
                          //              LaunchDelayUs in variants (unused)
)
{
    var isFirstPState   = (0 == pStateIdx);
    var isPStateZero    = (0 == pState);
    // assume by default fast double precision floating-point hardware
    // is not present
    // g_hasFastDP and g_hasFastHP are expected to be defined only when this file
    // is used in conjunction with tesla.spc
    var hasFastDP = "undefined" != typeof(g_hasFastDP) && g_hasFastDP;
    var hasFastHP = "undefined" != typeof(g_hasFastHP) && g_hasFastHP;
    var onlyAddComputeTests = "undefined" == typeof(g_hasFastDP);
    var longGLStressRuns = longDurationRuns
                           && isFirstPState
                           && !onlyAddOQATests;

    if (longDurationRuns)
    {
        // Override duration of Linpack tests if indicated
        testList.AddTestsArgs([
            ["LwdaLinpackDgemm",     setRunTimeMs(Math.ceil((isFirstPState && hasFastDP
                                                ? 1*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackPulseDP",  setRunTimeMs(Math.ceil((isFirstPState && hasFastDP
                                                ? 2*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackSgemm",    setRunTimeMs(Math.ceil((reducePower
                                                ? 3*1000
                                                : isFirstPState ? 1*60*1000
                                                                : 15*1000) * testFraction))]
            ,["LwdaLinpackPulseSP",  setRunTimeMs(Math.ceil((reducePower
                                                ? 3*1000
                                                : isFirstPState ? 2*60*1000
                                                                : 15*1000) * testFraction))]
            ,["LwdaLinpackHgemm",    setRunTimeMs(Math.ceil((hasFastHP
                                                ? 1*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackPulseHP",  setRunTimeMs(Math.ceil((isFirstPState && hasFastHP
                                                ? 2*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackHMMAgemm", setRunTimeMs(Math.ceil((isFirstPState
                                                ? 1*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackPulseHMMA",setRunTimeMs(Math.ceil((isFirstPState
                                                ? 2*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackIgemm",    setRunTimeMs(Math.ceil((reducePower
                                                ? 5*1000
                                                : isFirstPState ? 1*60*1000
                                                                : 15*1000) * testFraction))]
            ,["LwdaLinpackPulseIP",  setRunTimeMs(Math.ceil((reducePower
                                                ? 5*1000
                                                : isFirstPState ? 2*60*1000
                                                                : 15*1000) * testFraction))]
            ,["LwdaLinpackIMMAgemm", setRunTimeMs(Math.ceil((isFirstPState
                                                ? 1*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackPulseIMMA",setRunTimeMs(Math.ceil((isFirstPState
                                                ? 2*60*1000 : 15*1000) * testFraction))]
            ,["LwdaLinpackCombi",    setRunTimeMs(Math.ceil((isFirstPState
                                                ? 1*60*1000 : 15*1000) * testFraction))]
            ]);
    }
    if (reduceTemperature)
    {
        // Use launchDelayUs to reduce temperature on boards with
        // non-optimal thermal solution.
        testList.AddTestsArgs([
             ["LwdaLinpackDgemm",      {LaunchDelayUs: 150}]
            ,["LwdaLinpackSgemm",      {LaunchDelayUs: 150}]
            ,["LwdaLinpackHMMAgemm",   {LaunchDelayUs: 20000}]
            ,["LwdaLinpackIgemm",      {LaunchDelayUs: 150}]
            ]);
    }

    if ("undefined" === typeof setOfVariants)
    {
        setOfVariants = "ALL";
    }

    if (isFirstPState)
    {
        testList.AddTests([
            "CheckConfig"
            ,"CheckClocks"
            ,["CheckAVFS", {"ShowWorstAdcDelta": true}]
            ,"CheckInfoROM"
            ,"I2CTest"
            // a HULK license may be required to run the following test!
            ,"I2cDcbSanityTest"
            ,"ValidSkuCheck"
            ,"ValidSkuCheck2"
            ,"PwrRegCheck"
            ,"CheckInputVoltage"
            ,["CheckThermalSanity", onlyAddComputeTests ? {} : {SkipExternal: 1} ]
            ,"CheckFanSanity"
            ,"CheckPwrSensors"
            ,"BaseBoostClockTest"
            ,"PowerBalancingTest"
            ,"PowerBalancing2"
            ,["MatsTest", { "Coverage" : 0.065 * testFraction } ]
            ]);
    }

    testList.AddTest("FastMatsTest",  {Coverage: Math.ceil(100 * testFraction)});

    if (onlyAddComputeTests)
    {
        testList.AddTest("WfMatsMedium",
                         {CpuTestBytes: Math.ceil(12 * testFraction) *1024*1024});
    }

    testList.AddTests([
        "NewWfMatsNarrow"
        ,["MscgMatsTest", { MaxFbMb : 10,
                            BoxHeight : 16,
                            MinSuccessPercent : 10.0 } ]
        ]);

    if (isFirstPState)
    {
        testList.AddTests([
            "GpuPllLockTest"
            ]);
    }

    testList.AddTests([
        "CpyEngTest"
        ,"I2MTest"
        ,"CheckDma"
        ,"SecTest"
        ,"PcieSpeedChange"
        ]);

    if (isFirstPState)
    {
        testList.AddTests([
             "PerfSwitch"
            ,"PerfPunish"
            ,["LwLinkBwStress", {TestAllGpus: true}]
            ,["LwlbwsBeHammer", {TestAllGpus: true}]
            ]);
    }

    testList.AddTests([
         "Random2dFb"
        ,"Random2dNc"
        ,"LineTest"
        ,"MSENCTest"
        ,"LWDECTest"
        ,"LWDECGCxTest"
        ,"LWENCTest"
        ,"Sec2"
        ,"AppleGL"
        ,"LwvidLWDEC"
        ,"LwvidLwOfa"
        ,"LwvidLWJPG"
        ,"LwvidLWENC"
        ]);
    testList.AddTestsWithParallelGroup(PG_GRAPHICS, [
         ["GlrA8R8G8B8",   scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrFsaa2x",     scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrFsaa4x",     scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrMrtRgbU",    scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrMrtRgbF",    scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrY8",         scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrR5G6B5",     scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrFsaa8x",     scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrFsaa4v4",    scaleLoops(20*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrFsaa8v8",    scaleLoops(20*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrFsaa8v24",   scaleLoops(20*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrA8R8G8B8Sys",scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GlrLayered",    scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ,["GLComputeTest"]
        ,["PathRender"]
        ,["MsAAPR"]
        ,["GLPRTir"]
        ,["FillRectangle"]
        ]);

    testList.AddTest("GlrA8R8G8B8GCx", scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration));

    if (pState === 5 || pState === 8)
    {
        testList.AddTests([
            ["GlrA8R8G8B8Rppg", scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
           ,["GlrGpcRg", scaleLoops(10*20, 20, testFraction, pStateIdx, goldenGeneration)]
           ,["GlrGrRpg", scaleLoops(40*20, 20, testFraction, pStateIdx, goldenGeneration)]
           ]);
    }

    if (onlyAddComputeTests)
    {
        testList.AddTest("Elpg");
    }

    testList.AddTests([
        "ElpgGraphicsStress"
        ,"DeepIdleStress"
        ,["GLStress",      scaleLoopMs(longGLStressRuns
                                       ? 60*1000
                                       : 15000, testFraction, goldenGeneration)]
        ,["GLStressPulse", scaleLoopMs(longGLStressRuns
                                       ? 60*1000
                                       : 5000, testFraction, goldenGeneration)]
        ,["GLRandomCtxSw", scaleLoops(30*20, 20, testFraction, pStateIdx, goldenGeneration)]
        ]);

    if (!onlyAddComputeTests)
    {
        testList.AddTest("GLPowerStress", longGLStressRuns
                                          ? {RuntimeMs: Math.ceil(60*1000*testFraction)}
                                          : {});
    }
    else if (isFirstPState)
    {
        testList.AddTest("GLPowerStress");
    }

    if (!onlyAddOQATests)
    {
        if (!onlyAddComputeTests)
        {
            testList.AddTestArgs("LwdaMatsPatCombi", setIterations(Math.ceil(10*testFraction)));
        }
        testList.AddTests([
            // setIterations doesn't work properly if specified
            // with a ternary operator here!
            "LwdaMatsPatCombi"
            ]);
    }

    testList.AddTest("NewLwdaMatsTest",
                     setIterations(Math.ceil((onlyAddOQATests
                                   ? 2 : (onlyAddComputeTests ? 10 : 100)) * testFraction)));

    if (onlyAddOQATests)
    {
        testList.AddTest("NewLwdaMatsRandom",
                         setIterations(Math.ceil(2 * testFraction)));
    }
    else if (onlyAddComputeTests)
    {
        testList.AddTest("NewLwdaMatsRandom",
                         setIterations(Math.ceil(10 * testFraction)));
    }
    else
    {
        testList.AddTests([
            [ "NewLwdaMatsRandom", {Iterations: Math.ceil(178 * g_testFraction),
                                    MaxInnerIterations: 300, ChunkSizeMb: 256,
                                    VirtualTestId: "242-c",
                                    TestDescription: "ChunkSizeMb=256"}]
            ,[ "NewLwdaMatsRandom", {Iterations: Math.ceil(178 * g_testFraction),
                                    MaxInnerIterations: 300}]
            ,[ "NewLwdaMatsRandom", {MaxFbMb: 6,
                                     Iterations: Math.ceil(583000 * testFraction)}]
            ]);
    }

    testList.AddTests([
        ["LwdaMatsTest",    setIterations(Math.ceil((onlyAddOQATests
                                          ? 2 : (onlyAddComputeTests ? 20 : 200)) * testFraction))]
        ,["LwdaGrfTest",     setLoops(Math.ceil(((onlyAddOQATests ? 1 : 68) * 16) * testFraction))]
        ,["DPStressTest",    scaleLoops(onlyAddOQATests
                                        ? 6 : onlyAddComputeTests ? 1000 : 2000,
                                        1, testFraction, pStateIdx, goldenGeneration)]
        ]);

    if (onlyAddOQATests || onlyAddComputeTests)
    {
        testList.AddTests([
            "LwdaLinpackDgemm"
            ,"LwdaLinpackPulseDP"
            ,"LwdaLinpackSgemm"
            ,"LwdaLinpackPulseSP"
            ,"LwdaLinpackHgemm"
            ,"LwdaLinpackPulseHP"
            ,"LwdaLinpackHMMAgemm"
            ,"LwdaLinpackPulseHMMA"
            ,"LwdaLinpackIgemm"
            ,"LwdaLinpackPulseIP"
            ,"LwdaLinpackIMMAgemm"
            ,"LwdaLinpackPulseIMMA"
            ,"LwdaLinpackCombi"
            ]);
    }
    else
    {
        if (hasFastDP)
        {
            AddLinpackTests(testList, "LwdaLinpackDgemm", setOfVariants,
                            testFraction);
        }
        else
        {
            testList.AddTests([
                "LwdaLinpackDgemm"
                ]);
        }

        testList.AddTests([
            "LwdaLinpackPulseDP"
            ]);

        AddLinpackTests(testList, "LwdaLinpackSgemm", setOfVariants,
                        testFraction);

        testList.AddTests([
            "LwdaLinpackPulseSP"
            ,"LwdaLinpackHgemm"
            ,"LwdaLinpackPulseHP"
            ,"LwdaLinpackIMMAgemm"
            ,"LwdaLinpackPulseIMMA"
            ,"LwdaLinpackCombi"
            ]);

        AddLinpackTests(testList, "LwdaLinpackHMMAgemm", setOfVariants,
                        testFraction);

        // Add 16 bit HMMA variants
        AddLinpackTests(testList, "LwdaLinpackHMMAgemm", setOfVariants,
                        testFraction, -1, "h884_fp16");

        testList.AddTest("LwdaLinpackPulseHMMA");

        AddLinpackTests(testList, "LwdaLinpackIgemm", setOfVariants,
                        testFraction);

        testList.AddTests([
            "LwdaLinpackPulseIP"
            ]);
    }

    testList.AddTests([
        ["LwdaStress2", setLoopMs(Math.ceil(15000 * testFraction))]
        ,"LwdaTTUStress"
        ,"LwdaTTUBgStress"
        ,"LwdaL1Tag"
        ,["LwdaRandom", scaleLoops(180*75, 75, testFraction, pStateIdx, goldenGeneration)]
        ,"LwdaL2Mats"
        ,["LwdaXbar", onlyAddOQATests || onlyAddComputeTests
                      ? {TimeoutSec: 120}
                      : {TestConfiguration: {Loops: 1},
                         InnerIterations: Math.ceil(17997824 * testFraction),
                         TimeoutSec: 900}]
        ]);

    if (isFirstPState)
    {
        testList.AddTests([
            ["LwlbwsBgPulse", {TestAllGpus: true}]
            ]);
    }

    testList.AddTests([
        "MMERandomTest"
        ,"MME64Random"
        ,"Bar1RemapperTest"
        ]);

    if (onlyAddComputeTests)
    {
        testList.AddTests([
            // not yet ready for prime time (bug 800106)
            ["EccFbTest", onlyAddComputeTests ? {} : {IntrWaitTimeMs: 2000 } ]
            ]);
    }

    testList.AddTests([
        ["EccL2Test", onlyAddComputeTests ? {} : {IntrWaitTimeMs: 2000 } ]
        ,"LwdaEccL2"
        ,"EccErrorInjectionTest"
        ,"EccSM"
        ,"EccSMShm"
        ,"EccTex"
        ,["GLStressDots", longGLStressRuns ? {RuntimeMs: Math.ceil(2*60*1000*testFraction)} : {} ]
        ,"NewWfMatsShort"
        ,"NewWfMats"
        ,"GLStressFbPulse"
        ,"LwdaBoxTest"
        ,"LwdaByteTest"
        ,"GLStressNoFb"
        ,["GLStressZ", onlyAddComputeTests
                       ? scaleLoopMs(5000, testFraction, goldenGeneration)
                       : longGLStressRuns ? {RuntimeMs: Math.ceil(60*1000*testFraction)} : {} ]
        ,["HeatStressTest", longGLStressRuns ? {RuntimeMs: Math.ceil(2*60*1000*testFraction)} : {} ]
        ,["VkStress", { EnableValidation:false }]
        ,["VkStressMesh", { EnableValidation:false }]
        ,"VkStressPulse"
        ,["VkStressCompute", { EnableValidation:false }]
        ,"VkStressFb"
        ,"VkHeatStress"
        ,"VkPowerStress"
        ,["VkStressSCC", { EnableValidation:false }]
        ]);

    if (isFirstPState)
    {
        testList.AddTests([
             ["LwdaMarchTest", { TestConfiguration: {Loops: Math.ceil(1000 * testFraction)} }]
            ,["LwdaPatternTest", { TestConfiguration: {Loops: Math.ceil(1000 * testFraction)} }]
            ,["LwdaAppleMOD3Test", { TestConfiguration: {Loops: Math.ceil(1000 * testFraction)} } ]
            ,["LwdaAppleAddrTest", { TestConfiguration: {Loops: Math.ceil(1000 * testFraction)} }]
            ,["LwdaAppleKHTest", { TestConfiguration: {Loops: Math.ceil(1000 * testFraction)} }]
            ,["LwdaColumnTest", { ColumnIlwStep: 0x80000,
                                 Iterations: Math.ceil((onlyAddOQATests || onlyAddComputeTests
                                             ? 1 : 2) * testFraction)}]
            ,"NewLwdaMatsReadOnly"
            ,"NewLwdaMatsReadOnlyPulse"
            ,"NewLwdaMatsRandomPulse"
            ,["NewLwdaMatsStress", { Iterations: Math.ceil(30 * testFraction), Verbose: true }]
            ]);
    }

    testList.AddTests([
        [ "GLInfernoTest", {LoopMs: Math.ceil(15*1000*testFraction), EndLoopPeriodMs: 60000} ]
        ,[ "LwdaLdgTest",  {Iterations: Math.ceil(10 * testFraction)} ]
        ,[ "LwdaRadixSortTest", {DirtyExitOnFail: true, TestConfiguration:{TimeoutMs: 80000}} ]
        ]);

    if (isLastPState)
    {
        testList.AddTests([
            "CheckOvertemp"
            ,"CheckFbCalib"
            ,"KFuseSanity"
            ]);

        if (onlyAddComputeTests)
        {
            testList.AddTest("CheckInfoROM", {EccCorrTest:true, EclwncorrTest:true});
        }
    }
}

#include "linpack_tests.js"
