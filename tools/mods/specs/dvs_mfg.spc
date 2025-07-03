/*
* LWIDIA_COPYRIGHT_BEGIN
* Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
* LWIDIA_COPYRIGHT_END
*/

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/dvs_mfg.spc#11 $");

//------------------------------------------------------------------------------
function userSpec(testList, PerfPtList)
{
    var goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        // Do just one pstate for gpugen.js.
        PerfPtList = PerfPtList.slice(0,1);
        goldenGeneration = true;
    }

    let lastPState = 0;
    for (let i = 0; i < PerfPtList.length; i++)
    {
        if (PerfPtList[i].PStateNum > lastPState)
        {
            lastPState = PerfPtList[i].PStateNum;
        }
    }

    for (var i = 0; i < PerfPtList.length; i++)
    {
        let isLastPState = (lastPState === PerfPtList[i].PStateNum);
        testList.AddTest("SetPState", { InfPts : PerfPtList[i] });
        addBoardMfgTestsOnePState(testList, i, PerfPtList, goldenGeneration, isLastPState);
    }
}

//------------------------------------------------------------------------------
function addBoardMfgTestsOnePState(
    testList          // (TestList)
    ,pStateIdx        // (integer) how many pstates we've tested already
    ,perfPts
    ,goldenGeneration
    ,isLastPState
)
{
    var perfPt = perfPts[pStateIdx];
    var pLocationStr = perfPt.LocationStr;
    var isFirst = (0 == perfPt.PStateNum);
    var isMax = pLocationStr === "max";
    var isIntersect = pLocationStr === "intersect";
    var glrTestArgs;
    if (isFirst && isMax)
    {
        // A frame is 20 loops.
        // Goldens end after 40 frames.
        // At first PerfPoint (P0.max) run frames 0 to 19.
        glrTestArgs = scaleLoops(40*20, 20, 0.5, 0, goldenGeneration);
    }
    else
    {
        // All subsequent PerfPoints run two frames starting with 20.
        // Wrap back to frame 0 if we have too many PerfPoints.
        glrTestArgs = scaleLoops(40*20, 20, 2/40, pStateIdx, goldenGeneration);
    }

    // Skip all max points except P0.max to save test time
    if (!isFirst && isMax)
    {
        return;
    }

    if (isFirst && isMax)
    {
        testList.AddTests([
             [ "CheckConfig"/*1*/, {}]
            ,[ "CheckConfig2"/*350*/, {}]
            ,[ "GLPowerStress"/*153*/, {}]
            ,[ "GLStress"/*2*/, {"LoopMs":5000}]
            ,[ "GLStressDots"/*23*/,  {"LoopMs":5000}]
            ,[ "GLStressPulse"/*92*/, {"LoopMs":5000}]
            ,[ "GLStressFbPulse"/*95*/, {"LoopMs":5000}]
            ,[ "GLStressZ"/*166*/, {"LoopMs":5000}]
            ,"VkStress"/*267*/
            ,"VkStressCompute"/*367*/
            ,"VkStressRay"/*368*/
            ,"VkStressMesh"/*264*/
            ,["VkStressPulse"/*192*/, { "LoopMs":5000 }]
            ,["VkStressFb"/*263*/, { "LoopMs":1000 }]
            ,["VkHeatStress"/*262*/, { "LoopMs":1000 }]
            ,["VkPowerStress"/*261*/, { "LoopMs":1000 }]
            ,"VkStressSCC"/*366*/
            ,[ "GLComputeTest"/*201*/, {}]

            ,[ "LwdaLinpackDgemm"/*199*/, {"RuntimeMs":5000}]
            ,[ "LwdaLinpackSgemm"/*200*/, {"RuntimeMs":5000}]
            ,[ "LwdaLinpackHgemm"/*210*/, {"RuntimeMs":5000}]
            ,[ "LwdaLinpackIgemm"/*212*/, {}]
            ,[ "LwdaLinpackPulseDP"/*292*/, {"RuntimeMs":5000}]
            ,[ "LwdaLinpackPulseSP"/*296*/, {"RuntimeMs":5000}]
            ,[ "LwdaLinpackPulseHP"/*211*/, {"RuntimeMs":5000}]
            ,[ "LwdaLinpackPulseIP"/*213*/, {}]

            ,[ "LwdaXbar"/*220*/, {}]

            ,[ "MMERandomTest"/*150*/, {}]
            ,[ "PcieSpeedChange"/*30*/, {}]
            ,[ "PerfSwitch", {PerfSweepCovPct: 25}] // requires pstate vbios for testing
            ,[ "PerfPunish"/*6*/, {}]
            ,[ "PwmMode"/*509*/, {}]
            ,[ "CheckThermalSanity", {}]
            ,[ "I2cDcbSanityTest"/*293*/, {"UseI2cReads":true}]
            ,[ "DispClkSwitch"/*245*/, {}]
            ,[ "LwvddStress"/*300*/, {}]

            ,[ "CheckClocks"/*10*/, {}]
            ,[ "CheckAVFS"/*13*/, {}]
            ,"CheckInputVoltage"/*271*/
            ,"ClockMonitor"/*280*/
            ,"PowerBalancing2"/*282*/
        ]);
    }

    if (isFirst && isIntersect)
    {
        testList.AddTests([
            [ "FastMatsTest"/*19*/, {}]
            ,[ "CpyEngTest"/*100*/, {}]
            ,[ "I2MTest"/*205*/, {}]
            ,[ "NewWfMatsNarrow"/*180*/, {"MaxFbMb":32,"InnerLoops":8}]
            ,[ "NewWfMatsCEOnly"/*161*/, {}]
            ,[ "NewWfMats"/*94*/, {}]
        ]);
    }

    if (isMax || isIntersect)
    {
        testList.AddTests([
             [ "GlrA8R8G8B8"/*130*/, glrTestArgs]
            ,[ "GlrR5G6B5"/*131*/, glrTestArgs]
            ,[ "GlrFsaa2x"/*132*/, glrTestArgs]
            ,[ "GlrFsaa4x"/*133*/, glrTestArgs]
            ,[ "GlrMrtRgbU"/*135*/, glrTestArgs]
            ,[ "GlrMrtRgbF"/*136*/, glrTestArgs]
            ,[ "GlrY8"/*137*/, glrTestArgs]
            ,[ "GlrFsaa8x"/*138*/, glrTestArgs]
            ,[ "LwdaRandom"/*119*/, {"TestConfiguration":{"Loops":2250,"StartLoop":0}}]
            ,[ "GlrLayered"/*231*/, glrTestArgs]
            ,[ "PathRender"/*176*/, {}]
            ,[ "FillRectangle"/*286*/, {}]
            ,[ "MsAAPR"/*287*/, {}]
            ,[ "GLPRTir"/*289*/, {}]
            ,[ "GLRandomCtxSw"/*54*/, {"TestConfiguration":{"Loops":600,"StartLoop":0}}]
            ,["GlrA8R8G8B8Rppg"/*303*/, glrTestArgs]
        ]);
    }

    if (isMax)
    {
        testList.AddTests([
             [ "LWDECTest"/*277*/, {}]
            ,[ "LWENCTest"/*278*/, { MaxFrames : 5 }]
            ,[ "LwDisplayRandom"/*304*/, {}]
            ,[ "LwDisplayHtol"/*317*/, {}]
            ,[ "ConfigurableDisplayHTOL"/*324*/, {}]
            ,[ "LwvidLWDEC"/*360*/, {}]
            ,[ "LwvidLWJPG"/*361*/, {}]
            ,[ "LwvidLwOfa"/*362*/, {}]
            ,[ "LwvidLWENC"/*363*/, {}]
        ]);
    }

    if (isIntersect)
    {
        testList.AddTests([
             [ "LwDisplayRandom"/*304*/, {}]
            ,[ "LwDisplayHtol"/*317*/, {}]
            ,[ "ConfigurableDisplayHTOL"/*324*/, {}]
            ,[ "SorLoopback"/*11*/, {}]
        ]);
    }

    if (pStateIdx == 0)
    {
        // Tests that setup their own perf modes:
        testList.AddTests([
             [ "DispClkIntersect"/*302*/, {}]
        ]);
    }

    if (isLastPState)
    {
        if (isIntersect)
        {
            testList.AddTests([
              ["GpuGc5",       {"PrintSummary" : true, "TestConfiguration" : {"Loops" : 20, "StartLoop" : 0} }] //$
              ,["GpuGc6Test",  {"PrintSummary" : true, "TestConfiguration" : {"Loops" : 20,  "StartLoop" : 0} }] //$
              ,["GpuGcx",      {"PrintSummary" : true, "TestConfiguration" : {"Loops" :50,  "StartLoop" : 0} }] //$
            ]);
        }
        else
        {
            testList.AddTests([
              ["GpuGc5",       {"PrintSummary" : true, "TestConfiguration" : {"Loops" : 50, "StartLoop" : 0} }]  //$
              ,["GpuGc6Test",  {"PrintSummary" : true, "TestConfiguration" : {"Loops" : 50,  "StartLoop" : 0} }] //$
              ,["GpuGcx",      {"PrintSummary" : true, "TestConfiguration" : {"Loops" :150,  "StartLoop" : 0} }] //$
            ]);
        }
    }

    //Bug 1853225, we want to run test 4 only on 1 perfpoint on each pstate.
    //For merged rail boards there are 8 perf points and split rail boards have 12 perf points.
    //Now merged rail boards will run on 4 perf points and split rail will run on 5 perf points.
    if (pStateIdx == 0 || pStateIdx == 3 || pStateIdx == 4 || pStateIdx == 7 || pStateIdx == 11)
    {
        testList.AddTest("EvoLwrs"/*4*/, {"EnableReducedMode":true});
    }
}
