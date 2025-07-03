/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/base.spc#26 $");

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called
g_TestMode = (1 << 2); // TM_MFG_BOARD

//------------------------------------------------------------------------------
// long (run all MODS tests using long durations).
//------------------------------------------------------------------------------
function addModsGpuTests(testList, PerfPtList)
{
    testList.TestMode = (1 << 2); // TM_MFG_BOARD

    let goldenGeneration = false;
    if (Golden.Store === Golden.Action)
    {
        // Do just one pstate for gpugen.js.
        PerfPtList = PerfPtList.slice(0,1);
        goldenGeneration = true;
    }

    for (let i = 0; i < PerfPtList.length; i++)
    {
        testList.AddTest("SetPState", { InfPts : PerfPtList[i] });

        // hmm.... ask chip solutions what they want here. They are going to do some
        // Qual and give us some data
        let isLastPState    = (PerfPtList.length-1 === i);
        let testFraction    = (0 === i) ? 1.0 : 0.1;

        addTestsOnePState(testList, i, isLastPState, testFraction,
                          goldenGeneration);
    }
}

//------------------------------------------------------------------------------
function addTestsOnePState
(
    testList              // (TestList)
    ,pStateIdx            // (integer)  how many pstates we've tested already
    ,isLastPState         // (bool)     if true, add one-time late tests
    ,testFraction         // (float)    fraction of full test time
    ,goldenGeneration     // (bool)     golden generation phase
)
{
    let isFirstPState = (0 === pStateIdx);

    if (isFirstPState)
    {
        testList.AddTests([
            "CheckConfig"
            ,"CheckConfig2"
            ,"CheckClocks"
            ,"CheckAVFS"
            ,"CheckInfoROM"
            ,"CheckInputVoltage"
            ,"CheckSelwrityFaults"
            ,"ClockMonitor"
            ,"I2CTest"
            ,"I2cDcbSanityTest"
            ,"ValidSkuCheck"
            ,"ValidSkuCheck2"
            ,"FuseCheck"
            ,"PwrRegCheck"
            ,"CheckThermalSanity"
            ,["CheckFanSanity", { OnlySpinCheck: true } ]
            ,"CheckPwrSensors"
            ,"PowerBalancingTest"
            ,"PowerBalancing2"
            ,"CheckHDCP"
            ,"GpuPllLockTest"
            ,"CheckGc6EC"
            ]);
    }

    testList.AddTests([
        "FastMatsTest"
        ,"WfMatsMedium"
        ,["NewWfMatsNarrow", g_ShortenTestForSim ? {} :
                                 {
                                   MaxFbMb: (isFirstPState ? 32 : 20),
                                   InnerLoops: (isFirstPState ? 8 : 4)
                                 }]
        ,["MscgMatsTest", g_ShortenTestForSim ? {} :
                                 {
                                   MaxFbMb : 10,
                                   BoxHeight : 16,
                                   MinSuccessPercent : 10.0
                                 } ]
        ]);

    testList.AddTests([
         "CpyEngTest"
        ,"I2MTest"
        ,"CheckDma"
        ,"SecTest"
        ,"PcieSpeedChange"
        ,"PcieEyeDiagram"
        ]);

    if (isFirstPState)
    {
        testList.AddTests([
            "PerfSwitch"
           ,["PerfPunish", {"PunishTimeMs": 15000}]
           ,"DispClkSwitch"
           ,"LwvddStress"
           ,"DispClkIntersect" // Can't be run in parallel because it sets its own perf points
           ,"I2cEeprom"
           ]);
       
        let lwlinkTests = [
            "LwlValidateLinks"
           ,"LwLinkState"
           ,"LwLinkEyeDiagram"
           ,"LwLinkBwStress"
           ,"LwLinkBwStressPerfSwitch"
           ,"LwlbwsBeHammer"
           ,"LwlbwsLpModeSwToggle"
           ,"LwlbwsLpModeHwToggle"
           ,"LwlbwsLowPower"
           ,"LwlbwsSspmState"
           ,["LwlbwsThermalThrottling", { ThermalThrottlingOnCount : 8000, ThermalThrottlingOffCount : 16000 }]
           ,"LwlbwsThermalSlowdown"
           ,"LwLinkBwStressTrex"
           ,"LwlbwsTrexLpModeSwToggle"
        ];
        testList.AddTests(lwlinkTests);

        for (let topoIdx = 0; topoIdx < g_LwlMultiTopologyFiles.length; topoIdx++)
        {
            testList.AddTest("LoadLwLinkTopology", g_LwlMultiTopologyFiles[topoIdx]);

            testList.AddTests(lwlinkTests);
        }
    }

    testList.AddTests([
         "LineTest"
        ,"WriteOrdering"
        ,"MSENCTest"
        ,"LWDECTest"
        ,"LWDECGCxTest"
        ,"LWENCTest"
        ,"LwvidLWDEC"
        ,"LwvidLWJPG"
        ,"LwvidLwOfa"
        ,"LwvidLWENC"
        ,"LSFalcon"
        ,"Sec2"
        ,"AppleGL"
        ,"VgaHW"             // Can't run in parallel as the console has to be disabled
        ]);
    testList.AddTestsWithParallelGroup(PG_DISPLAY, [
         "LwDisplayRandom"
        ,"LwDisplayHtol"
        ,"ConfigurableDisplayHTOL"
        ,"DispClkStatic"
        ,"SorLoopback"
        ,"EvoLwrs"
        ,["EvoOvrl",       {"TestConfiguration" : {"Loops" : 20, "StartLoop" : 0}}]
        ],
        {"display":true});
    testList.AddTestsWithParallelGroup(PG_GRAPHICS, [
         "Random2dFb"
        ,"Random2dNc"
        ,["GlrA8R8G8B8",   {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrFsaa2x",     {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrFsaa4x",     {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrMrtRgbU",    {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrMrtRgbF",    {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrY8",         {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrR5G6B5",     {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrFsaa8x",     {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrFsaa4v4",    {"TestConfiguration" : {"Loops" : 20*20, "StartLoop" : 0}}]
        ,["GlrFsaa8v8",    {"TestConfiguration" : {"Loops" : 20*20, "StartLoop" : 0}}]
        ,["GlrFsaa8v24",   {"TestConfiguration" : {"Loops" : 20*20, "StartLoop" : 0}}]
        ,["GlrLayered",    {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrA8R8G8B8Sys",{"TestConfiguration" : {"Loops" : 20*20, "StartLoop" : 0}}]
        ,["GlrReplay",     {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrZLwll",      {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GLComputeTest"]
        ,["PathRender"]
        ,["MsAAPR"]
        ,["GLPRTir"]
        ,["FillRectangle"]
        ]);
    testList.AddTests([
        ["SyncParallelTests", {"ParallelId" : PG_DISPLAY}]
       ,["SyncParallelTests", {"ParallelId" : PG_GRAPHICS}]
       ]);

    testList.AddTests([
        ["GlrA8R8G8B8GCx", {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrA8R8G8B8Rppg", {"TestConfiguration" : {"Loops" : 40*20, "StartLoop" : 0}}]
        ,["GlrGpcRg", {"TestConfiguration" : {"Loops" : 10*20, "StartLoop" : 0}}]
        ,["GlrGrRpg", {"TestConfiguration" : {"Loops" : 10*20, "StartLoop" : 0}}]
        ,["GlrMsRpg", {"TestConfiguration" : {"Loops" : 10*20, "StartLoop" : 0}}]
        ,["GlrMsRppg", {"TestConfiguration" : {"Loops" : 10*20, "StartLoop" : 0}}]
        ,"Elpg"
        ,"ElpgGraphicsStress"
        ,"DeepIdleStress"
        ,["GLStress",      scaleLoopMs(15000, testFraction, goldenGeneration)]
        ,["GLStressZ",     scaleLoopMs(5000, testFraction, goldenGeneration)]
        ,["GLStressPulse", scaleLoopMs(5000, testFraction, goldenGeneration)]
        ,["GLRandomCtxSw", {"TestConfiguration" : {"Loops" : 30*20, "StartLoop" : 0}}]
        ,["GpuGc5",         {"PrintSummary" : true}]
        ,"VkStress"
        ,"VkStressCompute"
        ,["VkStressMesh",       {LoopMs: 500}]
        ,["VkStressPulse", { LoopMs: (isFirstPState ? 30000 : 5000) }]
        ,["VkStressFb", { LoopMs: 5000 }]
        ,["VkHeatStress", { LoopMs: 5000 }]
        ,["VkPowerStress", { LoopMs: 5000 }]
        ,"VkStressSCC"
        ,["VkStressRay", { LoopMs: 5000 }]
        ,["VkFusionRay",   { RuntimeMs: 5000 }]
        ,["VkFusionCombi", { RuntimeMs: 5000 }]
        ]);

    if (isFirstPState)
    {
        testList.AddTests([
            "GLPowerStress"
            ]);
    }

    testList.AddTests([
        "NewLwdaMatsTest"
        ,"NewLwdaMatsReadOnly"
        ,"NewLwdaMatsRandom"
        ,"NewLwdaMatsStress"
        ,"NewLwdaL2Mats"
        ,"NewLwdaMatsReadOnlyPulse"
        ,"NewLwdaMatsRandomPulse"
        ]);

    testList.AddTests([
        "LwdaGrfTest"
        ,["DPStressTest", {"TestConfiguration" : {"Loops" : 1000, "StartLoop" : 0}}]
        ,"LwdaStress2"
        ,"LwdaCbu"
        ,["LwdaLinpackDgemm",   { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackHgemm",   { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackHMMAgemm",{ RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackSgemm",   { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackIgemm",   { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackIMMAgemm",{ RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackPulseDP", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackPulseSP", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackPulseHP", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackPulseIP", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackPulseHMMA", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackPulseIMMA", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackCombi",   { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["LwdaLinpackStress",  { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackSgemm",    { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackIgemm",    { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackDgemm",    { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackHgemm",    { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackHMMAgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackIMMAgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackDMMAgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackE8M10",    { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackE8M7",     { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackBMMA",     { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackINT4",     { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackSparseHMMA", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackSparseIMMA", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseSgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseIgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseDgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseHgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseHMMAgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseIMMAgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseDMMAgemm", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseE8M10", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseE8M7", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseBMMA", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseINT4", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseSparseHMMA", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,["CaskLinpackPulseSparseIMMA", { RuntimeMs: ((isFirstPState && !g_ShortenTestForSim) ? 30000 : 5000)}]
        ,"LwdaLinpackCliffOVP"
        ,"LwdaTTUStress"
        ,"LwdaTTUBgStress"
        ,"LwdaL1Tag"
        ,["LwdaRandom", {"TestConfiguration" : {"Loops" : 30*75, "StartLoop" : 0}}]
        ,"LwdaRadixSortTest"
        ,"LwdaL2Mats"
        ,"LwdaXbar"
        ,"LwdaMmioYield"
        ,"LwdaSubctx"
        ]);

    if (isFirstPState)
    {
        testList.AddTests([
            "LwlbwsBgPulse"
            ,"LwdaLwLinkAtomics"
            ,"LwdaLwLinkGupsBandwidth"
            ,"IntAzaliaLoopback"
            ,"GrIdle"
            ]);
    }

    testList.AddTests([
        "MMERandomTest"
        ,["MME64Random", {
            "MacroSize": 512,
            "NumMacrosPerLoop": 16,
            "TestConfiguration": {
                "Loops": 25
            }
        }]
        ,"Bar1RemapperTest"
        ,"EccFbTest"
        ,"EccL2Test"
        ,"LwdaEccL2"
        ,["EccErrorInjectionTest", setLoops(100)]
        ,"EccSM"
        ,"EccSMShm"
        ,"EccTex"
        ]);

    if (isFirstPState)
    {
        addUsbTests(testList);
    }

    if (isLastPState)
    {
        testList.AddTests([
            "CheckOvertemp"
            ,"CheckFanSanity"
            ,"CheckFbCalib"
            ,"KFuseSanity"
            ,"Optimus"
            ,["GpuGc6Test", { "PrintSummary": true
                              ,TestConfiguration: {Loops: 100 }
                            }]
            ,["GpuGcx", { "PrintSummary": true
                              ,TestConfiguration: {Loops: 200 }
                            }]

            ]);
    }
}

// USB test needs special hardware attached to the GPU
// HW needed - functional test board, USB device(s) and (optionally) display device based on display test
// More details on
// https://confluence.lwpu.com/display/GM/USB+-+PLC#USB-PLC-PostSiliconDesign
// https://confluence.lwpu.com/display/GM/USB+-+PLC#USB-PLC-MODSUSBTestdetails
function addUsbTests(testList)
{
    g_UsbPpcConfigWaitTimeMs = 6000;
    g_UsbSysfsNodeWaitTimeMs = 2000;

    testList.AddTest("UsbStress"/*395*/, {
        "VirtualTestId": "UsbStress (395-03) - Data Transfer + Display",
        "ResetFtb":true,
        "DispTest":46,
        "DispTestArgs":{"LimitVisibleBwFromEDID":true,
                        "NumOfFrames":10,
                        "InnerLoopModesetDelayUs":5000000,
                        "OuterLoopModesetDelayUs":3000000}
    });
    testList.AddTest("UsbStress"/*395*/, {
        "VirtualTestId": "UsbStress (395-08) - Alt-Mode Switching",
        "AltModeMask":"0x7",
        "DispTest":46,
        "DispTestArgs":{"LimitVisibleBwFromEDID":true,
                        "NumOfFrames":10,
                        "InnerLoopModesetDelayUs":5000000,
                        "OuterLoopModesetDelayUs":3000000}
    });
    testList.AddTest("UsbStress"/*395*/, {
        "VirtualTestId": "UsbStress (395-06) - Orientation Flip",
        "AltModeMask":"0x6",
        "Orient":"1",
        "DispTest":46,
        "DispTestArgs":{"LimitVisibleBwFromEDID":true,
                        "NumOfFrames":10,
                        "InnerLoopModesetDelayUs":5000000,
                        "OuterLoopModesetDelayUs":3000000}
    });
    testList.AddTest("UsbStress"/*395*/, {
        "VirtualTestId": "UsbStress (395-09) - Data-Rate Limiting - HS",
        "AltModeMask":"0x6",
        "UsbTestArgs":{ "DataRate":"0" }
    });
    testList.AddTest("UsbStress"/*395*/, {
        "VirtualTestId": "UsbStress (395-10) - Data-Rate Limiting - SS",
        "AltModeMask":"0x5",
        "UsbTestArgs":{ "DataRate":"1" }
    });
    testList.AddTest("XusbEyeDiagram");
}

