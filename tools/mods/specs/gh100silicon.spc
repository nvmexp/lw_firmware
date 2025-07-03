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

/*
 * This script is intended for MODS engineers to do initial silicon checkout 
 * and report the results to the chipsDB. 
 * It is NOT intended to be used or given to other teams for running MODS tests.
 *
 *
*/

#include "fpk_comm.h"

// place any global WAR args here.
let g_WarArgs = " ";

g_SpecArgs = g_WarArgs;

let g_VerboseL0 = { TestDescription: "Level0", TestConfiguration: { Verbose: true }};
let g_VerboseL1 = { TestDescription: "Level1", TestConfiguration: { Verbose: true }};
let g_VerboseL2 = { TestDescription: "Level2", TestConfiguration: { Verbose: true }};


function specNoTest(testList)
{
    testList.AddTests(
     ["Idle", {                                 // -test 520 (no_test)
         "VirtualTestId": "Init/Teardown",
         RuntimeMs:100,
         TestDescription: "Level0",
         TestConfiguration: { Verbose: true }
     }]);
}

// For short/miscellaneous tests
function specBasic(testList)
{
    testList.AddTests([
        ["CheckConfig", g_VerboseL0]            // -test 1
       ,["CpyEngTest", g_VerboseL0]             // -test 100
       ,["GpuPllLockTest", g_VerboseL0]         // -test 170
//       ,["CheckDma", g_VerboseL0]               // -test 41 //Bug 3479343 fail miscompares
//       ,["CheckFbCalib", g_VerboseL0]           // -test 48 // Bug 3479348 fails Test exceeds teh expected threshold
       ,["ClockPulseTest", g_VerboseL0]         // -test 117
       ,["FuseCheck", g_VerboseL0]              // -test 196
       ,["I2CTest", g_VerboseL0]                // -test 50
       ,["I2MTest", g_VerboseL0]                // -test 205
       ,["LineTest", g_VerboseL0]               // -test 108
       ,["LSFalcon", g_VerboseL0]               // -test 55
       ,["Random2dFb", g_VerboseL0]             // -test 58 // need gpugen.js
       ,["Random2dNc", g_VerboseL0]             // -test 9
       ,["WriteOrdering", g_VerboseL0]          //-test 258 // need gpugen.js
//       ,["LWSpecs", g_VerboseL0]                // -test 14 // needs filename unless you set Override=true?
       ,["CheckSelwrityFaults",  g_VerboseL0]   //-test 284
       ,["CheckOvertemp",  g_VerboseL0]         //-test 65
       ,["CheckThermalSanity",  g_VerboseL0]    //-test 31
       ,["I2cDcbSanityTest",  g_VerboseL0]      //-test 293
//       ,["QuickSlowdown",  g_VerboseL0]         //-test 319 //quick_slowdown module is not enabled on phys GPC 0!
    ]);
}

// For ECC error checking requires the following:
// - an ECC enabled VBIOS (gh100_emu_hbm_ecc.rom)
// - source ../hdfuse.qel , then source ../../fuses/hdfuse_ECC.qel
function specEcc(testList)
{
    testList.AddTests([
        ["EccErrorInjectionTest", { //test 268
            TestDescription: "Level0",
            TestConfiguration: { Loops: 100, Verbose: true }
        }]
        ,["EccFbTest", g_VerboseL0] // -test 155
        ,["LwdaEccL2", g_VerboseL0] // -test 256
        ,["EccSMShm", g_VerboseL0]  // -test 369
        ,["EccTex", g_VerboseL0]    // -test 269
    ]);
}

// On INT & PROD skus (ie fused boards) you need a MemQual HULK license with the debug checkbox
// checked.
function specInstInSys(testList)
{
    testList.AddTests([
        ["Idle", { // -test 520 (no_test), passed
          VirtualTestId: "InstInSys - Init",
          RuntimeMs:100,
          TestDescription: "Level1",
          TestConfiguration: { Verbose: true }
        }] 
        ,["GLStress", { // passed
            VirtualTestId: "InstInSys - GLStress (2)",
            TestDescription: "Level1",
            TestConfiguration: { Verbose: true }
        }] 
        ,["VkStress", {  // passed
            VirtualTestId: "InstInSys - VkStress (267)",
            TestDescription: "Level1",
            NumTextures: 2,
            TestConfiguration: {Verbose: true}
        }]
        ,["LwdaRandom", { // passed
            VirtualTestId: "InstInSys - LwdaRandom (119)",
            TestDescription: "Level1", 
            TestConfiguration: { Verbose: true }
        }]
    ]);
}
specInstInSys.specArgs = "-inst_in_sys -enable_fbflcn 0 -lwda_in_sys -disable_acr";


//Note: These tests require a pstate enabled VBIOS to run.
function specPcie(testList)
{
    testList.AddTests([
        ["CpyEngTest", g_VerboseL0]         // -test 100
        ,["PcieSpeedChange", g_VerboseL0]   // -test 30
        ,["PexBandwidth", g_VerboseL1]      // -test 146
        ,["PexAspm", g_VerboseL1]           // -test 26
    ]);
}
specPcie.specArgs = " "; // -perlink_aspm 0 0 0 -no_gen5 -no_gen4 -test 146 -testarg 146 ShowBandwidthData true -testarg 146 LinkSpeedsToTest 0x7 -testarg 146 IgnoreBwCheck 1 -pex_verbose 0x2 -testarg 146 AllowCoverageHole true -loops 10
function specGlrFull(testList)
{
    let glrArgsL0 =
    {
        FrameRetries : 0,
        LogShaderCombinations : false,
        TestConfiguration :{Verbose : false}, TestDescription: "Level0"};
    let glrArgsL1 =
    {
        FrameRetries : 0,
        LogShaderCombinations : false,
        TestConfiguration :{Verbose : false},TestDescription: "Level1"};

    testList.AddTests([
//       ["GLRandomCtxSw", glrArgsL1]     // -test 54 hang, need to debug.
       ,["GLRandomOcg", glrArgsL1]      // -test 126
       ,["GlrA8R8G8B8", glrArgsL0]      // -test 130
       ,["GlrR5G6B5", glrArgsL0]        // -test 131
       ,["GlrFsaa2x", glrArgsL0]        // -test 132
       ,["GlrFsaa4x", glrArgsL0]        // -test 133
       ,["GlrMrtRgbU", glrArgsL0]       // -test 135
       ,["GlrMrtRgbF", glrArgsL0]       // -test 136
       ,["GlrY8", glrArgsL0]            // -test 137
       ,["GlrFsaa8x", glrArgsL0]        // -test 138
       ,["GlrFsaa4v4", glrArgsL0]       // -test 139
       ,["GlrFsaa8v8", glrArgsL0]       // -test 140
       ,["GlrFsaa8v24", glrArgsL0]      // -test 141
       ,["GlrA8R8G8B8Sys", glrArgsL1]   // -test 148
       ,["GlrReplay", glrArgsL1]        // -test 149
       ,["GlrZLwll", glrArgsL1]         // -test 162
       ,["GlrLayered", glrArgsL0]       // -test 231
    ]);
}
specGlrFull.specArgs = "-regress_miscompare_count 2 -regress_using_gltrace";

function specBFT(testList, perfPoints, testDevice)
{
    testList.AddTests(
    [
        ["GlrA8R8G8B8"],
        ["CaskLinpackPulseSgemm",{"RuntimeMs":5000}],
        ["CaskLinpackIgemm",{"RuntimeMs":1000}],
        ["CaskLinpackDgemm",{"RuntimeMs":1000}],
        ["CaskLinpackIMMAgemm",{"RuntimeMs":5000}],
        ["CaskLinpackPulseIMMAgemm",{"RuntimeMs":5000}],
        ["LwdaRandom",{TestConfiguration : {Loops: 30*75*2}}],
        ["VkStress",{LoopMs: 5000}],
        ["NewLwdaMatsRandom",{"Iterations": 2}],
        ["PexBandwidth",{Gen4Threshold:20}],
        ["CaskLinpackHgemm",{"RuntimeMs":5000}],
        ["CaskLinpackHMMAgemm",{"RuntimeMs":5000}],
        ["NewLwdaMatsRandomPulse",{"Iterations": 2}],
        ["WfMatsMedium",{"MaxFbMb": 512}],
        ["CaskLinpackDMMAgemm",{"RuntimeMs":5000}],
        ["NewWfMatsCEOnly",{MaxFbMb: 8192}],
        ["CaskLinpackSgemm",{"RuntimeMs":5000}]
    ]);
};
// For memory tests that do not use LWCA
function specMemoryNoLwda(testList)
{
    testList.AddTests([
        ["MatsTest", g_VerboseL0]           // -test 3
        ,["NewWfMats", g_VerboseL0]         // -test 94
        ,["Bar1RemapperTest", g_VerboseL0]  // -test 151
        ,["ByteTest", g_VerboseL0]          // -test 18
        ,["FastMatsTest", g_VerboseL0]      // -test 19
        ,["MarchTest", g_VerboseL0]         // -test 52
        ,["MemScrub", g_VerboseL0]          // -test 159
        ,["NewWfMatsBusTest", g_VerboseL0]  // -test 123
        ,["NewWfMatsCEOnly", g_VerboseL0]   // -test 161
        ,["NewWfMatsNarrow", g_VerboseL0]   // -test 180
        ,["NewWfMatsShort", g_VerboseL0]    // -test 93
        ,["PatternTest", g_VerboseL0]       // -test 70
        ,["WfMatsBgStress", g_VerboseL0]    // -test 178 (94 + 2)
        ,["WfMatsMedium", g_VerboseL0]      // -test 118
        ,["BoxTest", g_VerboseL0]           // -test 3
    ]);
}

// For all LWCA tests except the CaskLinpack variants and LwLink tests
function specLwda(testList)
{
    testList.AddTests([
         ["LwdaAppleAddrTest", g_VerboseL0]     // -test 115
        ,["LwdaAppleKHTest", g_VerboseL0]       // -test 116
        ,["LwdaAppleMOD3Test", g_VerboseL0]     // -test 114
        ,["LwdaBoxTest", g_VerboseL0]           // -test 110
        ,["LwdaByteTest", g_VerboseL0]          // -test 111
        ,["LwdaCbu", g_VerboseL0]               // -test 226
        ,["RegFileStress", g_VerboseL0]         // -test 336
        ,["LwdaCILPStress",g_VerboseL0]         // -test 337, run CaskLinpack in background
        ,["LwdaColumnTest", g_VerboseL0]        // -test 127
        ,["LwdaInstrPulse", g_VerboseL1]        // -test 318
        ,["LwdaLdgTest", g_VerboseL0]           // -test 184
        ,["LwdaMarchTest", g_VerboseL0]         // -test 112
        ,["LwdaMatsPatCombi", g_VerboseL0]      // -test 144
        ,["LwdaMatsTest", g_VerboseL0]          // -test 87
        ,["LwdaMmioYield", g_VerboseL0]         // -test 221
        ,["LwdaPatternTest", g_VerboseL0]       // -test 113
        ,["LwdaRadixSortTest", g_VerboseL0]     // -test 185
        ,["LwdaRandom", g_VerboseL0]            // -test 119
        ,["LwdaStress2", g_VerboseL0]           // -test 198
        ,["LwdaSubctx", g_VerboseL0]            // -test 223
        ,["LwdaTTUStress", g_VerboseL0]         // -test 340
        ,["LwdaXbar", g_VerboseL0]              // -test 220
        ,["LwdaGXbar", {                        // -test 370
            VirtualTestId: "LwdaGXbar - ReadOnly",
            ReadOnly: true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaGXbar", {                        // test 370
            VirtualTestId: "LwdaGXbar - ReadWrite",
            ReadOnly: false,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["DPStressTest", g_VerboseL0]          // -test 190
        ,["LwdaLdgTest", g_VerboseL0]           // -test 184
        ,["LwdaMarchTest", g_VerboseL0]         // -test 112
        ,["LwdaMatsPatCombi", g_VerboseL0]      // -test 144
        ,["LwdaL1Tag", g_VerboseL1]             // -test 255
        ,["LwdaLinpackCombi", g_VerboseL1]      // -test 330
    ]);
}

function specNewLwdaMats(testList)
{
    let ncmArgsL0 = { TestConfiguration: {Verbose : true}, TestDescription: "Level0"};
    let ncmArgsL1 = { TestConfiguration: {Verbose : true}, TestDescription: "Level1"};
    testList.AddTests([
        ["NewLwdaMatsTest", ncmArgsL0]          // -test 143
       ,["NewLwdaMatsReadOnly", ncmArgsL0]      // -test 241
       ,["NewLwdaMatsRandom", ncmArgsL0]        // -test 242
       ,["NewLwdaMatsStress", ncmArgsL1]        // -test 243
       ,["NewLwdaMatsReadOnlyPulse", ncmArgsL1] // -test 354
       ,["NewLwdaMatsRandomPulse", ncmArgsL1]   // -test 355
       ,["NewLwdaL2Mats", ncmArgsL1]            // -test 351
    ]);
}

// For all the CaskLinpack tests
function specCask(testList)
{
    let caskArgsL0 = {TestDescription: "Level0"};
    let caskArgsL1 = {TestDescription: "Level1"};

    testList.AddTests([
        ["CaskLinpackSgemm", caskArgsL0]            // -test 600
       ,["CaskLinpackPulseSgemm", caskArgsL1]       // -test 601
       ,["CaskLinpackIgemm", caskArgsL0]            // -test 602
       ,["CaskLinpackDgemm", caskArgsL0]            // -test 604
       ,["CaskLinpackHgemm", caskArgsL0]            // -test 606
       ,["CaskLinpackHMMAgemm", caskArgsL0]         // -test 610
       ,["CaskLinpackPulseHMMAgemm", caskArgsL0]    // -test 611
       ,["CaskLinpackIMMAgemm", caskArgsL0]         // -test 612
       ,["CaskLinpackDMMAgemm", caskArgsL0]         // -test 614
       ,["CaskLinpackE8M10", caskArgsL0]            // -test 620
       ,["CaskLinpackE8M7", caskArgsL0]             // -test 622
       ,["CaskLinpackSparseHMMA", caskArgsL0]       // -test 630
       ,["CaskLinpackSparseIMMA", caskArgsL0]       // -test 632
       ,["CaskLinpackBMMA", caskArgsL0]             // -test 640
    ]);
}

// Use Netlist: NET26/full_111_1LWDEC_1JPG_OFA_HBM3_fixECC_soqAP2102s2_2X_FV_6BD_AP_0.qt
function specVideo(testList)
{
    testList.AddTests([
        ["LwvidLWDEC", g_VerboseL0] // -test 360 Bug 3304823
       ,["LwvidLwOfa", g_VerboseL0] // -test 362
       ,["LwvidLWJPG", g_VerboseL0] // -test 361
       ,["GrIdle",     g_VerboseL1] // -test 328 runs the above 3 tests in the background
    ]);
}

function specLwLink(testList)
{
    testList.AddTests([
        ["LwLinkBwStress", g_VerboseL0]             // -test 246
       ,["LwdaLwLinkAtomics", g_VerboseL1]          // -test 56
       ,["LwdaLwLinkGupsBandwidth", g_VerboseL1]    // -test 251
       ,["LwlbwsBeHammer", g_VerboseL1]             // -test 28
       ,["LwlbwsBgPulse", g_VerboseL1]              // -test 37
       ,["LwlbwsLowPower", g_VerboseL1]             // -test 64
       ,["LwlbwsLpModeHwToggle", g_VerboseL1]       // -test 77
       ,["LwlbwsLpModeSwToggle", g_VerboseL1]       // -test 16
       ,["LwlbwsSspmState", g_VerboseL2]            // -test 5
       ,["LwLinkBwStressPerfSwitch", g_VerboseL2]   // -test 345
       ,["LwLinkEyeDiagram", g_VerboseL0]           // -test 248
       ,["LwLinkState", g_VerboseL0]                // -test 252
       ,["LwlValidateLinks", g_VerboseL0]           // -test 254
       ,["LwlbwsErrorInjection", g_VerboseL0]       // -test 51
    ]);
}
specLwLink.specArgs = "-c2c_disable";

// For OpenGL tests that are not GLRandom or GLStress variants
function specGLMisc(testList)
{
    testList.AddTests([
       ["PathRender", g_VerboseL0]          // -test 176
       ,["GLVertexFS", g_VerboseL0]         // -test 179
       ,["GLComputeTest", g_VerboseL0]      // -test 201
       ,["GLHistogram", g_VerboseL0]        // -test 274
       ,["FillRectangle", g_VerboseL0]      // -test 286
       ,["MsAAPR", g_VerboseL1]             // -test 287
       ,["GLPRTir", g_VerboseL1]            // -test 289
    ]);
}

// For variants of GLStress
function specGLStress(testList)
{
    let glsArgsL0 = { TestConfiguration: {Verbose: false }, TestDescription: "Level0" };
    let glsArgsL1 = { TestConfiguration: { Verbose: false }, TestDescription: "Level1" };
    testList.AddTests([
        ["GLStress", glsArgsL0]             // -test 2
       ,["GLStressDots", glsArgsL0]         // -test 23
       ,["GLStressPulse", glsArgsL1]        // -test 92
       ,["GLStressFbPulse", glsArgsL1]      // -test 95
       ,["GLStressNoFb", glsArgsL1]         // -test 152
       ,["GLPowerStress", glsArgsL1]        // -test 153
       ,["GLStressZ", glsArgsL0]            // -test 166
       ,["GLStressClockPulse", glsArgsL1]   // -test 177
       ,["GLInfernoTest", glsArgsL1]        // -test 222
    ]);
}

// For variants of VkStress and other Vulkan tests
function specVulkan(testList)
{
    let vkStressArgsL0 = {TestConfiguration: {Verbose: false}, TestDescription: "Level0"};
    let vkStressArgsL1 = {TestConfiguration: {Verbose: false}, TestDescription: "Level1"};

    testList.AddTests([
         ["VkPowerStress", vkStressArgsL1]      // -test 261
        ,["VkHeatStress", vkStressArgsL1]       // -test 262
        ,["VkStressFb", vkStressArgsL1]         // -test 263
        ,["VkStressMesh", vkStressArgsL1]       // -test 264
        ,["VkTriangle", vkStressArgsL1]         // -test 266
        ,["VkStress", vkStressArgsL0]           // -test 267
        ,["VkStressCompute", vkStressArgsL1]    // -test 367
        ,["VkStressSCC", vkStressArgsL1]        // -test 366
        ,["VkStressPulse", vkStressArgsL1]      // -test 192
        ,["VkFusion", vkStressArgsL1]           // -test 21
    ]);
}

// Requires special .0C VBIOS with C2C enabled.
function specC2C(testList)
{
    let c2cArgsL0 = {TestConfiguration: {Verbose: true}, TestDescription: "Level0"};
    let c2cArgsL1 = {TestConfiguration: {Verbose: true}, TestDescription: "Level1"};
    testList.AddTests([
         ["C2CBandwidth", c2cArgsL0]        // -test 546
        ,["C2CBwBeHammer", c2cArgsL0]       // -test 547
        ,["C2CBwBgPulse", c2cArgsL0]        // -test 548
        //,["LwdaC2CAtomics", c2cArgsL0]      // -test 549 see http://lwbugs/3339369
        //,["LwdaC2CBandwidth", c2cArgsL0]    // -test 550 see http://lwbugs/3339369
    ]);
}
specC2C.specArgs = " -c2c_verbose 1"

function specBar1P2P(testList)
{
    let bar1ArgsL0 = {TestConfiguration: {Verbose: true}, TestDescription: "Level0"};
    testList.AddTests([
         ["Bar1P2PBandwidth", bar1ArgsL0] // -test 39
    ]);
}
specBar1P2P.specArgs = " -c2c_disable -lwlink_force_disable"


function specMisc(testList)
{
    testList.AddTests([
        ["GpuTrace", g_VerboseL0]               // -test 20
        ,["PerfPunish", g_VerboseL0]            // -test 6
        ,["BaseBoostClockTest", g_VerboseL0]    // -test 275
        ,["PwmMode", g_VerboseL0]               // -test 509
        ,["PerfSwitch", g_VerboseL0]            // -test 145
    ]);
}

// These tests are going to take a long time on emulation.
function specLwdaSchmoo(testlist)
{
    testList.AddTests([
        ["LwdaColumnShmooTest", { // -test 227
            MaxFbMb : 2,
            Iterations : 3,
            ColumnStep : 3,
            Patterns : [0x0,0xFFFFFFFF],
            TestConfiguration: {Verbose: true},
            TestDescription: "Level1"
        }]
        ,["LwdaMatsShmooTest", { // -test 187
            MaxFbMb : 2,
            Iterations : 3,
            TestConfiguration : {ShortenTestForSim : true, Verbose : true},
            TestDescription: "Level1"
        }]
    ]);
}

//-lwda_in_sys 
// This spec requires:
// -Confidential Compute to be enabled via "-enable_hopper_cc" mods cmd line
// On emulation:
//   source ../hdfuse.qel then
//   source ../../fuses/hdfuse_onlyGSP_PRIV_SEC.qel then
//   force lw_top.s0_0.SYS.fuse.u_fusegen_logic.u_intfc.opt_c2c_disable_intfc_fuse 1
function specCCBasic(testList)
{
    testList.AddTests([
                      ["SelwreCopy", g_VerboseL0] // -test 109
                      ]);
}
specCCBasic.specArgs = " -enable_hopper_cc";

function userSpec(testList)
{
     specNoTest(testList);
     specBasic(testList);
     specInstInSys(testList);
     specGlrFull(testList);
     specMemoryNoLwda(testList);
     specLwda(testList);
     specNewLwdaMats(testList);
     specCask(testList);
     specVideo(testList);
     specLwLink(testList);
     specGLMisc(testList);
     specGLStress(testList);
     specVulkan(testList);
     specPcie(testList);
     specEcc(testList);
     specBar1P2P(testList);
     specMisc(testList);
}

