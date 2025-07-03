/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
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
g_SpecArgs = "-null_display -skip_azalia_init";

//------------------------------------------------------------------------------
// DVS SRIOV (software regression suite)
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

    testList.AddTest("SetPState", { InfPts : PerfPtList[0] });

    testList.AddTests([
        "GLRandomOcg"
        ]);

    testList.AddTests([
        ["MatsTest", { MaxFbMb: 16 }]
        ,["FastMatsTest", { MaxFbMb : isTegra ? 16 : 128 }]
        ]);

    //testList.AddTests([
    //    ["NewWfMatsCEOnly", { MaxFbMb : isTegra ? 16 : 512 }]
    //    ,["WfMatsMedium",  { MaxFbMb : 10,
    //                         CpuTestBytes : isTegra ? 512*1024 : 2*1024*1024 } ]
    //    ,["MscgMatsTest", { MaxFbMb : 10,
    //                        BoxHeight : 16,
    //    		    MinSuccessPercent : 10.0 } ]
    //    ]);

    testList.AddTests([
        ["CpyEngTest",     setLoops(isTegra ? 5 : 25)]
        ,"I2MTest"
        ,["Random2dFb",      setLoops(10*200)]
        ,"Random2dNc"
        ,"LineTest"
        ,"WriteOrdering"
        ,"MSENCTest"
    //    ,"LWDECTest"
    //    ,"LWDECGCxTest"
        ,["MsAAPR"]
        ,["GLPRTir"]
        ,["PathRender"]
        ,["FillRectangle"]
        ]);

    //let graphicsList = [
    //    ["GlrA8R8G8B8",     setLoops(isGvs ? 1*20 : 7*20)]
    //    ,["GlrFsaa2x",      setLoops(isGvs ? 1*20 : 7*20)]
    //    ,["GlrFsaa4x",      setLoops(isGvs ? 1*20 : 6*20)]
    //    ,["GlrMrtRgbU",     setLoops(isGvs ? 1*20 : 7*20)]
    //    ,["GlrMrtRgbF",     setLoops(isGvs ? 1*20 : 7*20)]
    //    ,["GlrY8",          setLoops(isGvs ? 1*20 : 7*20)]
    //    ,["GlrR5G6B5",      setLoops(isGvs ? 1*20 : 7*20)]
    //    ,["GlrFsaa8x",      setLoops(isGvs ? 1*20 : 6*20)]
    //    ,["GlrFsaa4v4",     setLoops(3*20)] //Reduced test duration
    //    ,["GlrFsaa8v8",     setLoops(3*20)] //Reduced test duration
    //    ,["GlrFsaa8v24",    setLoops(3*20)] //Reduced test duration
    //    ,["GlrA8R8G8B8Sys", setLoops(6*20)]
    //    ,["GlrLayered",     setLoops(isGvs ? 1*20 : 6*20)]
    //    ,["GLComputeTest",  { TestConfiguration: { Loops : 2 }, LoopMs: 500 }]
    //    ];
    //testList.AddTestsWithParallelGroup(PG_GRAPHICS, graphicsList);

    //testList.AddTests([
    //    ["GLStress",           setLoopMs(3000)]
    //    ,["GLInfernoTest",      setLoopMs(isTegra ? 500 : 2000)]
    //    ]);

    // Ensure that we regress both outer and inner iterations, with the minimal runtime
    // (Although we use the entire framebuffer by default to regress the memory allocator)
    //let newLwdaMatsArgs =
    //{
    //    "Iterations": 3,
    //    "MaxInnerIterations": 2,
    //    "Patterns": [0x55555555, 0xAAAAAAAA],
    //    "MaxFbMb": 64
    //};

    //testList.AddTests([
    //   ["NewLwdaMatsTest",     newLwdaMatsArgs]
    //   ,["NewLwdaMatsRandom",  { "Iterations": 15, "MaxFbMb": 128}]
    //   ,["NewLwdaMatsStress",  { "bgTestArg": { "TestConfiguration": { Loops: 10 } },
    //                             "Iterations": 128, "MaxFbMb": 128}]
    //   //,"LwdaBoxTest"
    //   ]);

    //let lwdaStress2Args = setLoopMs(500);
    //lwdaStress2Args.MaxFbMb = 128;
    //if (isTegra)
    //{
    //    lwdaStress2Args.MinFbTraffic = true;
    //}

    //let lwdaXbarArgs =
    //{
    //    InnerIterations: isTegra ? 2 : 8192,
    //    TestConfiguration: { Loops: isTegra ? 1 : 30 }
    //};

    testList.AddTests([
    //    ["LwdaGrfTest",         setLoops(8*16)]
    //    ,["DPStressTest",        setLoops(10)]
    //    ,["LwdaStress2",         lwdaStress2Args]
        ["LwdaLinpackDgemm",    { "CheckLoops": 4, RuntimeMs: 500 }]
        ,["LwdaLinpackPulseDP",  { "CheckLoops": 4, RuntimeMs: 500, Ksize: 256 }]
        ,["LwdaLinpackSgemm",    { "CheckLoops": 10, TestConfiguration: { Loops: 100 }}]
        ,["LwdaLinpackIgemm",    { "CheckLoops": 10, RuntimeMs: 500 }]
        ,["LwdaLinpackIMMAgemm", { "CheckLoops": 10, RuntimeMs: 500 }]
        ,["LwdaLinpackHgemm",    { "CheckLoops": 10, RuntimeMs: 500 }]
        ,["LwdaLinpackHMMAgemm", { "CheckLoops": 10, RuntimeMs: 500 }]
        ,["LwdaLinpackCliffA10", { Ksize: 1024, RuntimeMs: 500 }]
        ,["LwdaLinpackCliffA30", { Ksize: 1024, RuntimeMs: 500 }]
        ,["LwdaLinpackCliffOVP", { RuntimeMs: 500 } ]
        ,["LwdaLinpackCombi",    { fgTestArg: { RuntimeMs: 500 },
                                   bgTestArg: { RuntimeMs: 250 }} ]
    //    ,["LwdaTTUStress",       { RuntimeMs: 500 }]
    //    ,["LwdaTTUBgStress",     { RuntimeMs: 500 }]
    //    ,["LwdaInstrPulse",      { RuntimeMs: 500 } ]
    //    ,["LwdaRandom",          setLoops(6*75)]
    //    ,["LwdaL2Mats",          { "Iterations" : 200 } ]
    //    ,["LwdaXbar",            lwdaXbarArgs]
    //    ,["LwdaMmioYield",       { TestMode : 0x5 } ] // Bug with nanosleeo: http://lwbugs/2212722
    //    ,["LwdaSubctx",          { EndVal : 10000 } ]
    //    ,["LwdaCbu",             { StressFactor : 1 }]
    //    ,["LwdaBoxTest",         { Coverage : 1 } ]
    //    ,["LwdaByteTest",        { MaxFbMb : 2 } ]
    //    ,["LwdaMarchTest",       { MaxFbMb : 2 } ]
    //    ,["LwdaPatternTest",     { MaxFbMb : 4 } ]
    //    ,["LwdaAppleMOD3Test",   { MaxFbMb : 4 } ]
    //    ,["LwdaAppleAddrTest",   { MaxFbMb : 4 } ]
    //    ,["LwdaAppleKHTest",     { MaxFbMb : 4 } ]
    //    ,["LwdaRadixSortTest",   { PercentOclwpancy : 0.01 } ]
        ]);
    //testList.AddTests([
    //     ["VkStress",            { LoopMs: 500 }]
    //    ,["VkStressCompute",     { LoopMs: 500 }]
    //    ,["VkStressMesh",        { LoopMs: 500 }]
    //    ,["VkStressPulse",       isTegra ? { LoopMs: 500 } : { LoopMs: 1500, OctavesPerSecond: 4 }]
    //    ,["VkStressFb",          { LoopMs: 500 }]
    //    ,["VkHeatStress",        { LoopMs: 500 }]
    //    ,["VkPowerStress",       { LoopMs: 500 }]
    //    ,["VkTriangle"]
    //    ,["VkStressSCC",         { LoopMs: 500 }]
    //    ]);
    }

    // On machines where we run the GpuTrace test, run 3 loops
    testList.AddTestsArgs([
        ["GpuTrace", setLoops(3)]
    ]);
}

var userSpec = addModsGpuTests;

var g_SpecFileIsSupported = SpecFileIsSupportedAll;
