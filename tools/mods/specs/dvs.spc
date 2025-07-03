/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */



// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called
g_TestMode = (1<<6); // TM_DVS

//------------------------------------------------------------------------------
// DVS (software regression suite)
//
// We'll run just one pstate to keep time short.
//------------------------------------------------------------------------------
function addModsGpuTests(testList, PerfPtList, testdevice)
{
    testList.TestMode = (1<<6); // TM_DVS

    var isTegra = g_PvsMode === "CheetAh";
    var isGvs = isTegra && ! g_PvsMachineName.match(/^ausvrl/);

    var numPerfPts = 1;
    var perfPointsList = PerfPtList;
    if ((typeof(g_PerfPointListToTestByDev[0]) !== "undefined") &&
        (g_PerfPointListToTestByDev[0].length > 0))
    {
        perfPointsList = g_PerfPointListToTestByDev[0];
        numPerfPts = perfPointsList.length;
    }

    for (var i=0; i<numPerfPts; i++)
    {
        testList.AddTest("SetPState", { InfPts : PerfPtList[i] });

        var scrubValue = 0x0;
        if (Golden.Store == Golden.Action)
        {
            scrubValue = 0xffffffff;
        }

        if (i === 0)
        {
            // Run ECC check at the beginning of each perf point.
            // This lets us distinguish between errors which were flagged
            // before running MODS and errors which arised during testing.
            testList.AddTest("TegraEccCheck");
        }

        if (!isTegra)
        {
            testList.AddTests([
                ["MemScrub", { ScrubValue : scrubValue }]
                ]);
        }

        testList.AddTests([
             "EvoSli"
            ,"HdmiLoopback"
            ,"GLRandomOcg"
            ]);

        testList.AddTests([
            "CheckConfig"
            ,"CheckConfig2"
            ,"CheckInfoROM"
            ,"I2CTest"
            ,"I2cDcbSanityTest"
            ,"I2cEeprom"
            ,"ValidSkuCheck"
            ,"ValidSkuCheck2"
            ,"FuseCheck"
            ,"CheckThermalSanity"
            ,"CheckFanSanity"
            ,"CheckPwrSensors"
            ,"CheckClocks"
            ,"CheckAVFS"
            ,"CheckSelwrityFaults"
            ,"PwrRegCheck"
            ,["CheckInputVoltage", { NumSamples: 25 }]
            ,"ClockMonitor"
            ]);

        // Perform Optimus and GpuReset tests to see if they influence any other tests
        testList.AddTests([
            "Optimus"
            ,"GpuResetTest"
            ]);

        testList.AddTests([
            ["MatsTest", { MaxFbMb: 16 }]
            ,["FastMatsTest", { MaxFbMb : isTegra ? 16 : 128 }]
            ]);

        testList.AddTests([
            ["NewWfMatsCEOnly", { MaxFbMb : isTegra ? 16 : 512 }]
            ,["WfMatsMedium",  { MaxFbMb : 32,
                                 CpuTestBytes : isTegra ? 512*1024 : 2*1024*1024 } ]
            ,["MscgMatsTest", { MaxFbMb : 10,
                                BoxHeight : 16,
                                MinSuccessPercent : 10.0 } ]
            ]);

        testList.AddTests([
            "CheckHDCP"
            ,"GpuPllLockTest"
            ,["PexBandwidth",   setLoops(20)]
            ,"PcieEyeDiagram"
            ,["PexAspm",        setLoops(5)]
            ,"PexMargin"
            ,["CpyEngTest",     setLoops(isTegra ? 5 : 25)]
            ,"I2MTest"
            ,["CheckDma",  setLoops(5*200)]
            ,"SecTest"
            ,["PcieSpeedChange",setLoops(10)]
            ,["PerfSwitch",      isTegra ?
                { FgTestArgs: { LoopMs: 300 }, PerfSweepCovPct: 2, PerfJumpsCovPct: 2 } :
                { FgTestArgs: { LoopMs: 500 }, PerfJumpTimeMs: 2500, PerfSweepTimeMs: 1000} ]
            ,"PerfPunish"
            ,["PwmMode", { MinTargetTimeMs: 500 }]
            ,["Random2dFb",      setLoops(10*200)]
            ,"Random2dNc"
            ,"LineTest"
            ,"WriteOrdering"
            ,"MSENCTest"
            ,"LSFalcon"
            ,"LWDECTest"
            ,"LWDECGCxTest"
            ,"Sec2"
            ,"LwvidLWDEC"
            ,"LwvidLWJPG"
            ,"LwvidLwOfa"
            ,"LwvidLWENC"
            ,["TegraCec",       setLoops(5*2) ]
            ,["MsAAPR"]
            ,["GLPRTir"]
            ,["PathRender"]
            ,["FillRectangle"]
            ,"DispClkIntersect" // Can't be run in parallel because it sets its own perf points
            ,["VgaHW",          setLoops(4)] // Can't run in parallel as the console has to be disabled
            ]);

        let lwlinkTests = [
             "LwlValidateLinks"
            ,["LwLinkState", { TestConfiguration : { Loops : 5 } }]
            ,"LwLinkEyeDiagram"
            ,"LwLinkBwStress"
            ,["LwLinkBwStressPerfSwitch", { PerfJumpsCovPct: 10, FgTestArgs: { SurfSizeFactor : 2, TestConfiguration : {Loops : 10} } } ]
            ,"LwlbwsBeHammer"
            ,["LwlbwsThermalThrottling", { ThermalThrottlingOnCount : 8000, ThermalThrottlingOffCount : 16000 }]
            ,"LwlbwsThermalSlowdown"
            ,["LwlbwsLpModeSwToggle", { SurfSizeFactor : 2, TestConfiguration : {Loops : 1} }]
            ,["LwlbwsLpModeHwToggle", { SurfSizeFactor : 2, TestConfiguration : {Loops : 1} }]
            ,["LwlbwsSspmState", { SurfSizeFactor : 2, TestConfiguration : {Loops : 1} }]
            ,["LwlbwsLowPower", { SurfSizeFactor : 2, TestConfiguration : {Loops : 1} }]
            ,"LwLinkBwStressTrex"
            ,"LwlbwsTrexLpModeSwToggle"
        ];
        testList.AddTests(lwlinkTests);

        for (let topoIdx = 0; topoIdx < g_LwlMultiTopologyFiles.length; topoIdx++)
        {
            testList.AddTest("LoadLwLinkTopology", g_LwlMultiTopologyFiles[topoIdx]);

            testList.AddTests(lwlinkTests);
        }

        if (isGvs)
        {
            testList.AddTests([
                 ["LwLinkBwStressTegra", { TestConfiguration : {Loops : 10} }]
                ,["LwlbwstBgPulse", { TestConfiguration : {Loops : 10} }]
                ,["LwlbwstLpModeSwToggle", { TestConfiguration : {Loops : 10} }]
                ,["LwlbwstLpModeHwToggle", { TestConfiguration : {Loops : 10} }]
                ,["LwlbwstLowPower", { TestConfiguration : {Loops : 2} }]
                ,["LwlbwstCpuTraffic", { TestConfiguration : {Loops : 5} }]
                ]);
        }

        if (testdevice.DeviceId === Gpu.GV100)
        {
            testList.AddTests([
                [ "StartBgTest", { BgTest: "LwLinkBwStress", SkipBandwidthCheck : true,
                                   TestConfiguration:{TimeoutMs: 90000},
                                   TestAllGpus : true,
                                   PerModeRuntimeSec: 240}]
               ,["StartBgTest",  { BgTest: "LwdaLinpackPulseHMMA"}]
               ,"LwLinkEyeDiagram"
               ,"StopBgTests"
            ]);
        }

        if (testdevice.Type === TestDeviceConst.TYPE_LWIDIA_LWSWITCH)
        {
            testList.AddTests([
                "I2cEeprom"
                ]);
        }

        let displayList = [
             ["TegraWin",       { TestOnlyMaxRes : true }]
            ,["LwDisplayRandom", { "TestConfiguration":{"Loops":200,"RestartSkipCount":10}}]
            ,["LwDisplayDPC", { DPCFilePath:"1024x768.dpc"}]
            ,["LwDisplayDPC", { DPCFilePath:"1024x768_2Head1Or_mst_multi_stream.dpc"}]
            ,"LwDisplayHtol"
            ,"ConfigurableDisplayHTOL"
            ,"DispClkStatic"
            ,"SorLoopback"
            ,"EvoLwrs"
            ,["EvoOvrl",        setLoops(4)]
            ];
        let graphicsList = [
            ["GlrA8R8G8B8",     setLoops(isGvs ? 1*20 : 7*20)]
            ,["GlrFsaa2x",      setLoops(isGvs ? 1*20 : 7*20)]
            ,["GlrFsaa4x",      setLoops(isGvs ? 1*20 : 6*20)]
            ,["GlrMrtRgbU",     setLoops(isGvs ? 1*20 : 7*20)]
            ,["GlrMrtRgbF",     setLoops(isGvs ? 1*20 : 7*20)]
            ,["GlrY8",          setLoops(isGvs ? 1*20 : 7*20)]
            ,["GlrR5G6B5",      setLoops(isGvs ? 1*20 : 7*20)]
            ,["GlrFsaa8x",      setLoops(isGvs ? 1*20 : 6*20)]
            ,["GlrFsaa4v4",     setLoops(3*20)] //Reduced test duration
            ,["GlrFsaa8v8",     setLoops(3*20)] //Reduced test duration
            ,["GlrFsaa8v24",    setLoops(3*20)] //Reduced test duration
            ,["GlrA8R8G8B8Sys", setLoops(6*20)]
            ,["GlrLayered",     setLoops(isGvs ? 1*20 : 6*20)]
            ,["GlrReplay",      setLoops(3*20)] //Reduced test duration
            ,["GlrZLwll",       setLoops(3*20)] //Reduced test duration
            ,"GLComputeTest"
            ,["TegraVic",       {TestConfiguration: {Loops : 10}}]
            ,"VicLdcTnr"
            ,["Lwjpg",          { TestConfiguration: { ShortenTestForSim: true } }]
            ,"TegraLwdec"
            ,"TegraLwJpg"
            ,["TegraLwOfa", {TestConfiguration : {Loops : 5}}]
            ,["TegraLwEnc", {TestConfiguration : {Loops : 8}, NumFramesMax : 1}]
            ,["TegraTsec",      setLoops(1)]
            ];
        if (isTegra)
        {
            graphicsList.unshift("LWENCTest");
        }
        else
        {
            displayList.unshift(["LWENCTest", { MaxFrames : 5 }]);
        }
        testList.AddTestsWithParallelGroup(PG_DISPLAY, displayList, {"display":true});
        testList.AddTestsWithParallelGroup(PG_GRAPHICS, graphicsList);

        testList.AddTests([
             ["SyncParallelTests", {"ParallelId" : PG_DISPLAY}]
            ,["SyncParallelTests", {"ParallelId" : PG_GRAPHICS}]
            ]);

        testList.AddTests([
            ["GlrA8R8G8B8GCx",      setLoops(7*20)]
            ,["GlrA8R8G8B8Rppg",    setLoops(7*20)]
            ,["GlrGpcRg",           setLoops(2*20)]
            ,["GlrGrRpg",           setLoops(7*20)]
            ,["GlrMsRpg",           setLoops(7*20)]
            ,["GlrMsRppg",          setLoops(7*20)]
            ,"AppleGL"
            ,["Elpg",               setLoops(16)]
            ,["ElpgGraphicsStress", setLoopMs(500)]
            ,["DeepIdleStress",     setLoopMs(300)]
            ,["GLStress",           setLoopMs(500)]
            ,["GLStressZ",          setLoopMs(500)]
            ,["GLInfernoTest",      setLoopMs(500)]
            ,["GpuGc5",      {"PrintSummary" : true , "TestConfiguration" : {"Loops" : 50, "StartLoop" : 0} }]
            ,["GpuGc6Test",  {"PrintSummary" : true , "TestConfiguration" : {"Loops" : 50,  "StartLoop" : 0} }]
            ,["GpuGcx",      {"PrintSummary" : true , "TestConfiguration" : {"Loops" : 20,  "StartLoop" : 0} }]
            ,["GpuGcxD3Hot", {"PrintSummary" : true , "TestConfiguration" : {"Loops" : 10,  "StartLoop" : 0} }]
            ]);

        // Ensure that we regress both outer and inner iterations, with the minimal runtime
        let newLwdaMatsArgs =
        {
            "Iterations": 3,
            "MaxInnerIterations": 2,
            "MaxFbMb": 64
        };

        // At least one test in dvs.spc should try allocating the entire FB.
        // The memory allocator is fragile so we need to regress it.
        let allocEntireFbArgs =
        {
            "Iterations": 1,
            "Patterns": [0x55555555],
            "MaxFbMb": 0
        };

        testList.AddTests([
             ["NewLwdaMatsTest",          allocEntireFbArgs]
            ,["NewLwdaMatsReadOnly",      newLwdaMatsArgs]
            ,["NewLwdaMatsRandom",        newLwdaMatsArgs]
            ,["NewLwdaL2Mats",            {RuntimeMs: 500}]
            ,["NewLwdaMatsReadOnlyPulse", newLwdaMatsArgs]
            ,["NewLwdaMatsRandomPulse",   newLwdaMatsArgs]
            ,["NewLwdaMatsStress",  { "bgTestArg": { "TestConfiguration": { Loops: 10 } },
                                      "Iterations": 128, "MaxFbMb": 64 }]
            //,"LwdaBoxTest"
            ]);

        let lwdaStress2Args = setLoopMs(500);
        lwdaStress2Args.MaxFbMb = 128;
        if (isTegra)
        {
            lwdaStress2Args.MinFbTraffic = true;
        }

        let lwdaXbarArgs =
        {
            InnerIterations: isTegra ? 2 : 8192,
            TestConfiguration: { Loops: isTegra ? 1 : 30 }
        };

        testList.AddTests([
            ["LwdaGrfTest",         setLoops(8*16)]
            ,["DPStressTest",        setLoops(10)]
            ,["LwdaStress2",         lwdaStress2Args]
            ,["LwdaLinpackDgemm",    { CheckLoops: 4, RuntimeMs: 500 }]
            ,["LwdaLinpackPulseDP",  { CheckLoops: 4, RuntimeMs: 500, Ksize: 256 }]
            ,["LwdaLinpackSgemm",    { CheckLoops: 10, TestConfiguration: {Loops: 100}}]
            ,["LwdaLinpackIgemm",    { CheckLoops: 10, RuntimeMs: 500 }]
            ,["LwdaLinpackIMMAgemm", { CheckLoops: 10, RuntimeMs: 500 }]
            ,["LwdaLinpackHgemm",    { CheckLoops: 10, RuntimeMs: 500 }]
            ,["LwdaLinpackHMMAgemm", { CheckLoops: 10, RuntimeMs: 500 }]
            ,["LwdaLinpackCliffA10", { Ksize: 1024, RuntimeMs: 500 }]
            ,["LwdaLinpackCliffA30", { Ksize: 1024, RuntimeMs: 500 }]
            ,["LwdaLinpackCliffOVP", { RuntimeMs: 500 } ]
            ,["LwdaLinpackCombi",    { fgTestArg: { RuntimeMs: 500 },
                                       bgTestArg: { RuntimeMs: 250 }} ]
            ,["LwdaLinpackStress",   { RuntimeMs: 500, PrintPerf: true }]
            ,["CaskLinpackSgemm",    { RuntimeMs: 500, PrintPerf: true }]
            ,["CaskLinpackIgemm",    { RuntimeMs: 500 }]
            ,["CaskLinpackDgemm",    { RuntimeMs: 500 }]
            ,["CaskLinpackHgemm",    { RuntimeMs: 500 }]
            ,["CaskLinpackHMMAgemm", { RuntimeMs: 500 }]
            ,["CaskLinpackIMMAgemm", { RuntimeMs: 500 }]
            ,["CaskLinpackDMMAgemm", { RuntimeMs: 500 }]
            ,["CaskLinpackPulseDMMAgemm", { CheckLoops: 4, RuntimeMs: 500, Ksize: 256 }]
            ,["CaskLinpackE8M10",    { RuntimeMs: 500 }]
            ,["CaskLinpackE8M7",     { RuntimeMs: 500 }]
            ,["CaskLinpackBMMA",     { RuntimeMs: 500 }]
            ,["CaskLinpackINT4",     { RuntimeMs: 500 }]
            ,["CaskLinpackSparseHMMA", { RuntimeMs: 500 }]
            ,["CaskLinpackSparseIMMA", { RuntimeMs: 500 }]
            ,["LwdaTTUStress",       { RuntimeMs: 500 }]
            ,["LwdaTTUBgStress",     { RuntimeMs: 500, bgTestArg: { RuntimeMs: 500 } }]
            ,["LwdaL1Tag",           { RuntimeMs: 500 }]
            ,["LwdaGXbar",           { RuntimeMs: 500 }]
            ,["LwdaCILPStress",      { RuntimeMs: 500 }]
            ,["LwdaInstrPulse",      { RuntimeMs: 500 } ]
            ,["LwdaRandom",          {"TestConfiguration" : { "Loops" : 6*75, "StartLoop" : 0 },
                                      Coverage: true }]
            ,["LwdaL2Mats",          { "Iterations" : 200 } ]
            ,["LwdaXbar",            lwdaXbarArgs]
            ,"LwdaMmioYield"
            ,["LwdaSubctx",          { EndVal : 10000 } ]
            ,["LwdaCbu",             { StressFactor : 1 }]
            ,["LwdaBoxTest",         { Coverage : 1 } ]
            ,["LwdaByteTest",        { MaxFbMb : 2 } ]
            ,["LwdaMarchTest",       { MaxFbMb : 2 } ]
            ,["LwdaPatternTest",     { MaxFbMb : 4 } ]
            ,["LwdaAppleMOD3Test",   { MaxFbMb : 4 } ]
            ,["LwdaAppleAddrTest",   { MaxFbMb : 4 } ]
            ,["LwdaAppleKHTest",     { MaxFbMb : 4 } ]
            ,["LwdaRadixSortTest",   { PercentOclwpancy : 0.01 } ]
            ,["VkStress",            { LoopMs: 500 }]
            ,["VkStressRay",         { LoopMs: 500 }]
            ,["VkStressCompute",     { LoopMs: 500 }]
            ,["VkStressMesh",        { LoopMs: 500 }]
            ,["VkStressPulse",       isTegra ? { LoopMs: 500 } : { LoopMs: 1500, OctavesPerSecond: 4 }]
            ,["VkStressFb",          { LoopMs: 500 }]
            ,["VkHeatStress",        { LoopMs: 500 }]
            ,["VkPowerStress",       { LoopMs: 500 }]
            ,["VkTriangle"]
            ,["VkStressSCC",         { LoopMs: 500 }]
            ]);

        // WAR crash in VK_KHR_ray_query on aarch64
        if (OperatingSystem === "Android" || !IsLinuxOS() || LinuxArch !== "aarch64")
        {
            testList.AddTests([
                 ["VkFusionRay",   { RuntimeMs: 500 }]
                ,["VkFusionCombi", { RuntimeMs: 500 }]
                ]);
        }

        testList.AddTests([
            "LwlbwsBgPulse"
          , "LwdaLwLinkAtomics"
          , ["LwdaLwLinkGupsBandwidth", { PerModeRuntimeSec : 1 }]
          , "IntAzaliaLoopback"
          , [
                "GrIdle"
              , {
                    Tests :
                    [
                        "LwDisplayHtol"
                      , "ConfigurableDisplayHTOL"
                      , [
                            "LWENCTest", { MaxFrames : 5 }
                        ]
                      // The rest of the tests will be picked up from the
                      // `SetupGrIdle` function. This is the way `TestListCopyObject`
                      // works: it overrides only explicitely specified first
                      // members of an array. There is a correspondent note in
                      // `SetupGrIdle` left. Change it, if you change this property
                      // here.
                    ]
                  , RuntimeMs: 1000
                }
            ]
        ]);

        testList.AddTests([
            ["MMERandomTest",       setLoops(10*100)]
            ,["MME64Random", {
                "MacroSize" : 256,
                "SimOnly" : false,
                "SimTrace" : false,
                "DumpMacros" : false,
                "DumpSimEmissions" : false,
                "DumpMmeEmissions" : false,
                "NumMacrosPerLoop" : 12,
                "GenMacrosEachLoop" : true,
                "GenOpsArithmetic" : true,
                "GenOpsLogic" : true,
                "GenOpsComparison" : true,
                "GenOpsDataRam" : true,
                "GenNoLoad" : false,
                "GenNoSendingMethods" : false,
                "GenUncondPredMode" : false,
                "GenMaxNumRegisters" : 0, // All registers
                "GenMaxDataRamEntry" : 0, // Full data ram
                "PrintTiming" : false,
                "PrintDebug" : false,
                "TestConfiguration" : { "Loops" : 50 }
            }]
            ,["Bar1RemapperTest",   setLoops(10*5)]
            ,["EccFbTest",          setLoops(1875)]
            ,["EccL2Test",          setLoops(10000)]
            ,["LwdaEccL2",          { TestConfiguration: {Loops: 2}, Iterations : 100 } ]

            , ["EccErrorInjectionTest", {
                "IntrWaitTimeMs": 1000,
                "IntrCheckDelayMs": 3,
                "IntrTimeoutMs": 6,
                "ActiveGrContextWaitMs": 1500,
                "RetriesOnFailure": 0,
                "TestConfiguration": { "Loops" : 1 }
            }]

            ,["EccSM",              setLoops(isTegra ? 16 : 32)]
            ,["EccSMShm",           setLoops(isTegra ? 1 : 32)]
            ,"EccTex"
            ]);

        testList.AddTests([
            "CheckOvertemp"
            ,"CheckFbCalib"
            ,["KFuseSanity",    setLoops(1)]
            ]);

        // USB test
        testList.AddTests([
            ["UsbStress"/*395*/, {
                "VirtualTestId": "UsbStress (395-03) - Alt-Mode DP4+3",
                "ResetFtb":true,
                "AltModeMask":"0x1",
                "DispTest":46,
                "DispTestArgs":{"LimitVisibleBwFromEDID":true,
                                "NumOfFrames":10,
                                "InnerLoopModesetDelayUs":5000000,
                                "OuterLoopModesetDelayUs":3000000}
            }]
            ,["UsbStress"/*395*/, {
                "VirtualTestId": "UsbStress (395-04) - Alt-Mode DP4+2",
                "AltModeMask":"0x2",
                "DispTest":46,
                "DispTestArgs":{"LimitVisibleBwFromEDID":true,
                                "NumOfFrames":10,
                                "InnerLoopModesetDelayUs":5000000,
                                "OuterLoopModesetDelayUs":3000000}
            }]
            ,["UsbStress"/*395*/, {
                "VirtualTestId": "UsbStress (395-07) - Orientation Flip",
                "AltModeMask":"0x1",
                "Orient":"1",
                "DispTest":46,
                "DispTestArgs":{"LimitVisibleBwFromEDID":true,
                                "NumOfFrames":10,
                                "InnerLoopModesetDelayUs":5000000,
                                "OuterLoopModesetDelayUs":3000000},
                "UsbTestArgs":{"ShowBandwidthData":true}
            }]
            ,["UsbStress"/*395*/, {
                "VirtualTestId": "UsbStress (395-10) - Data-Rate Limiting - SS",
                "AltModeMask":"0x1",
                "UsbTestArgs":{"DataRate":"1",
                               "ShowBandwidthData":true}
            }]
        ]);
    }

    // On machines where we run the GpuTrace test, run 3 loops
    testList.AddTestsArgs([
        ["GpuTrace", setLoops(3)]
    ]);

    testList.AddTest("ThermalResistance",
    {
        IdleTimeMs: 100,
        LoadTimeMs: 500,
        FanSettleTimeMs: 0,
        PrintSamples: false,
        PrintSummary: false,
        LoadPerfPoint: { PStateNum: 0, LocationStr: "tdp" }
    });

    testList.AddTest("TegraEccCheck");
}

var g_SpecFileIsSupported = SpecFileIsSupportedAll;
