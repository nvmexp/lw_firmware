/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//***************************************************************************
// Test specification for running MODS on CheetAh simulation and emulation
//
// To run this test spec on linsim, execute the following command:
// mods -gpubios 0 t124_sim_sddr3.rom gputest.js -readspec tegrasim.spc
// (and provide lots of linsim-specific arguments)
//***************************************************************************

g_SpecArgs  = " -maxwh 128 128";
g_SpecArgs += " -verbose";
g_SpecArgs += " -timeout_ms 1800000";
g_SpecArgs += " -disable_mods_console";
g_SpecArgs += " -disable_composition_mode"; // Due to long test time on VDK GVS
g_SpecArgs += " -skip_pertest_pexcheck"; // Disables PEX checks on standalone GK20A
#ifdef INCLUDE_OGL
g_SpecArgs += " -regress_miscompare_count 1"; // Reduce number of triages in GLRandom
#endif

// Don't touch clocks
if (OperatingSystem === "Android")
    g_SpecArgs += " -dvfs";
else
    g_SpecArgs += " -pstate_disable";

if (OperatingSystem === "Linux")
{
    g_SpecArgs += " -no_rc"; // This avoids issues on ASIM and VSP
    g_SpecArgs += " -no_rcwd"; // This avoids issues on VSP
}

// Uncomment this on Tegrasim
//g_SpecArgs += " -pmu_bootstrap_mode 1"; // PMU falcon lwrrently not supported on Tegrasim

// Uncomment this to disable SMMU
//g_SpecArgs += " -disable_smmu";

// Uncomment this when running non-display tests
//g_SpecArgs += " -null_display";

if (OperatingSystem === "Sim")
{
    Platform.SysMemSize = 192*1024*1024;

    // Uncomment this to do experiments with carveout - this enables fake FB heap in RM
    //Platform.CarveoutSizeMb = 96;
}
else
{
    g_SpecArgs += " -queued_print_enable false"; // Print instantly, useful when MODS crashes
}

// Uncomment these to disable relevant non-existent engines on emulation
//Registry.ResourceManager.RmDisableDisplay = 1;
//Registry.ResourceManager.RmDisableMsenc = 1;

// Disable SkipTextureHostCopies for dramatic speedup on linsim
Registry.OpenGL.SkipTextureHostCopies = 0;

function CaskTestArgs(m, n, k)
{
    return {
        Msize:              m,
        Nsize:              n,
        Ksize:              k,
        MNKMode:            2,
        ReportFailingSmids: false,
        RuntimeMs:          0,
        TestConfiguration:  { Loops:   1,
                              Verbose: true }
    };
}

//------------------------------------------------------------------------------
function userSpec(testList, PerfPtList)
{
    testList.TestMode = 128;

    var glrTestArgs = {
	FrameRetries: 0,
	TestConfiguration: {Loops: 6*20, StartLoop: 0} };

    var glrLayeredTestArgs = {
        FrameRetries: 0,
        NumLayersInFBO: 2,
        ClearLines: false,
        TestConfiguration: {Loops: 4*20, StartLoop: 0, ShortenTestForSim: true} };

    let lwdaMatsArgs = {
        MaxFbMb:            64/1024,
        NumBlocks:          1,
        NumThreads:         1,
        Iterations:         2
    };

    testList.AddTests(
    [
        [ "GlrA8R8G8B8",            glrTestArgs ]
       ,[ "GlrFsaa2x",              glrTestArgs ]
       ,[ "GlrFsaa4x",              glrTestArgs ]
       ,[ "GlrMrtRgbU",             glrTestArgs ]
       ,[ "GlrMrtRgbF",             glrTestArgs ]
       ,[ "GlrY8",                  glrTestArgs ]
       ,[ "GlrR5G6B5",              glrTestArgs ]
       ,[ "GlrFsaa8x",              glrTestArgs ]
       ,[ "GlrLayered",             glrLayeredTestArgs ]
       ,[ "GLRandomCtxSw",          glrTestArgs ]
       ,[ "GLStress", {
                LoopMs:             0,
                TestConfiguration:  {Loops: 5} }]
       ,[ "GLStressZ", {
                LoopMs:             0,
                TestConfiguration:  {Loops: 5} }]
       ,[ "VkStress", {
                LoopMs:             0,
                FramesPerSubmit:    4,
                NumLights:          1,
                SampleCount:        2,
                NumTextures:        2,
                TestConfiguration:  { Loops: 4 }}]
       ,[ "VkStressRay", {
                LoopMs:             0,
                FramesPerSubmit:    4,
                NumLights:          1,
                NumTextures:        2,
                Raytracer:          { FramesPerSubmit: 2 },
                TestConfiguration:  { Loops: 4 }}]
       ,[ "VkStressMesh", {
                LoopMs:             0,
                FramesPerSubmit:    4,
                NumLights:          1,
                SampleCount:        2,
                NumTextures:        2,
                TestConfiguration:  { Loops: 4 }}]
       ,[ "GLComputeTest", {
                LoopMs:             3000,
                TestConfiguration:  {Loops: 2, ShortenTestForSim: true} }]
       ,[ "LwdaRandom", {
                TestConfiguration:  {Loops: 4,
                                     RestartSkipCount: 1} }]
       ,[ "CpyEngTest", {
                TestConfiguration:  {Loops: 1} }]
       ,[ "Random2dFb", {
                TestConfiguration:  {Loops: 200} }]
       ,[ "WriteOrdering", {
                TestConfiguration:  {Loops: 200} }]
       ,[ "I2MTest", {
                TestConfiguration:  {Loops: 20} }]
       ,[ "LineTest", {
                LineSegments:       10,
                TestConfiguration:  {Loops: 1} }]
       ,[ "MMERandomTest", {
                TestConfiguration:  {Loops: 10*5,
                                     RestartSkipCount: 10} }]
       ,[ "Bar1RemapperTest", {
                TestConfiguration:  {Loops: 5} }]
       ,[ "FastMatsTest", {
                MaxFbMb:            512/1024 }]
       ,[ "NewWfMatsShort", {
                MaxFbMb:            3,
                CpuTestBytes:       512*1024,
                PatternSet:         1,
                VerifyChannelSwizzle: true }]
       ,[ "NewWfMatsCEOnly", {
                MaxFbMb:            2,
                VerifyChannelSwizzle: true }]
       ,[ "LwdaStress2", {
                LoopMs:             1,
                Iterations:         2,
                NumBlocks:          1,
                MinFbTraffic:       true,
                TestConfiguration:  {ShortenTestForSim : true} }]
       ,[ "LwdaXbar", {
                InnerIterations:    2,
                TestConfiguration:  {Loops: 1} }]
       ,[ "LwdaXbar2", {
                Iterations:         1,
                InnerIterations:    1,
                Mode:               3}]
       ,[ "LwdaSubctx", {
                EndVal: 0x100 }]
       ,[ "LwdaTTUStress", {
                RuntimeMs: 0,
                InnerIterations: 32,
                TestConfiguration: { Loops: 1, Verbose: true } }]
       ,[ "NewLwdaMatsTest",        lwdaMatsArgs ]
       ,[ "NewLwdaMatsRandom",      lwdaMatsArgs ]
       ,[ "NewLwdaMatsStress",      lwdaMatsArgs ]
       ,[ "NewLwdaMatsReadOnly",    lwdaMatsArgs ]
       ,[ "TegraCec", {
                TestConfiguration:  {RestartSkipCount:  5,
                                     Loops:            10} }]
       ,"LWDECTest"
       ,"LWENCTest"
       ,[ "TegraLwdec", {
                TestConfiguration:  {ShortenTestForSim: true} }]
       ,[ "TegraLwEnc", {
                NumFramesMax     :  2,
                TestConfiguration:  {ShortenTestForSim: true} }]
       ,[ "TegraLwOfa", {
                TestConfiguration:  {ShortenTestForSim: true} }]
       ,[ "TegraVic", {
                TestConfiguration:  {ShortenTestForSim: true} }]
       ,[ "TegraLwJpg", {
                // VDK doesn't support conlwrrent LwJpg testing
                SkipEngineMask:     2,
                TestConfiguration:  {ShortenTestForSim: true} }]
       ,[ "TegraTsec", {
                TestConfiguration:  {ShortenTestForSim: true} }]
       ,[ "PathRender", {
                TestConfiguration:  {RestartSkipCount: 10,
                Loops:               10*2,
                StartLoop:           0,
                Verbose:             true } }]
       ,[ "LwLinkBwStressTwod", {
                SurfSizeFactor: 1 }]
       ,[ "CaskLinpackBMMA",        CaskTestArgs(256, 128, 128) ]
       ,[ "CaskLinpackDgemm",       CaskTestArgs(128, 128,   8) ]
       ,[ "CaskLinpackDMMAgemm",    CaskTestArgs(128, 128,   8) ]
       ,[ "CaskLinpackE8M10",       CaskTestArgs(256, 128,   8) ]
       ,[ "CaskLinpackE8M7",        CaskTestArgs(256, 128,   8) ]
       ,[ "CaskLinpackHgemm",       CaskTestArgs(256, 128,   8) ]
       ,[ "CaskLinpackHMMAgemm",    CaskTestArgs(256, 128,   8) ]
       ,[ "CaskLinpackIgemm",       CaskTestArgs(128, 128,  16) ]
       ,[ "CaskLinpackIMMAgemm",    CaskTestArgs(256, 128,  16) ]
       ,[ "CaskLinpackINT4",        CaskTestArgs(256, 128,  32) ]
       ,[ "CaskLinpackSgemm",       CaskTestArgs(128, 128,   8) ]
       ,[ "CaskLinpackSparseHMMA",  CaskTestArgs(128, 128,  64) ]
       ,[ "CaskLinpackSparseIMMA",  CaskTestArgs(128, 128,  64) ]
    ]);
}

var g_SpecFileIsSupported = SpecFileIsSupportedSocOnly;
