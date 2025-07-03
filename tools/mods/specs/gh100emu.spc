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
#include "fpk_comm.h"

g_SpecArgs = "";

let g_VerboseL0 = { TestDescription: "Level0", TestConfiguration: { Verbose: true }};
let g_VerboseL1 = { TestDescription: "Level1", TestConfiguration: { Verbose: true }};
let g_VerboseL2 = { TestDescription: "Level2", TestConfiguration: { Verbose: true }};

let g_LwdaWARArgs = " -no_rc -no_rcwd";

// For ECC error checking
// Requires the following:
// - an ECC enabled VBIOS (gh100_emu_hbm_ecc.rom)
// - source ../hdfuse.qel , then source ../../fuses/hdfuse_ECC.qel
function specEcc(testList)
{
    testList.AddTests([
        ["EccErrorInjectionTest", { 
            TestDescription: "Level0",
            EccTestMask: 0x3FFFFF, // only test PCIE_REORDER & PECI_P2PREQ
            IntrTimeoutMs: 6.0,
            IntrCheckDelayMs: 2.0,
            ActiveGrContextWaitMs: 5000,
            TestConfiguration: { Verbose: true }
        }] // -test 268
    ]);
}

function specInstInSys(testList)
{
    testList.AddTests([
        ["Idle", { // -test 520 (no_test)
          VirtualTestId: "InstInSys - Init",
          RuntimeMs:100,
          TestDescription: "Level1",
          TestConfiguration: { Verbose: true }
        }] 
        ,["GLStress", { // -test 2 hangs in emulation, see http://lwbugs/3314609 
            VirtualTestId: "InstInSys - GLStress (2)",
            TestDescription: "Level1",
            LoopMs: 0,
            TestConfiguration: { Loops: 4, Verbose: true }
        }] 
        ,["VkStress", {  // -test 267 Hangs on emulation, wait for resolution of B3314609 before filing a new bug.
            VirtualTestId: "InstInSys - VkStress (267)",
            TestDescription: "Level1",
            LoopMs: 0,
            FramesPerSubmit: 1,
            NumTextures: 2,
            TestConfiguration: {
                Loops: 4,
                Verbose: true
            }
        }]
        ,["LwdaRandom", { // -test 119 see http://lwbugs/3314500
            VirtualTestId: "InstInSys - LwdaRandom (119)",
            TestDescription: "Level1", 
            TestConfiguration: { Verbose: true }
        }]
    ]);
}
specInstInSys.specArgs = "-inst_in_sys -enable_fbflcn 0 -disable_intr_illegal_compstat_access 1 ";
//-lwda_in_sys 
// For short/miscellaneous tests
function specBasic(testList)
{
    testList.AddTests([
        ["CheckConfig", g_VerboseL0] // -test 1
       ,["CheckSelwrityFaults", g_VerboseL1] // -test 284
       ,["CpyEngTest", g_VerboseL0] // -test 100
       ,["GpuPllLockTest", g_VerboseL0] // -test 170
       ,["CheckDma", { // -test 41
           TestConfiguration: {
               Verbose: true
               ,Loops: 100 // Uncomment for shorter runtime
            },
            TestDescription: "Level0"
        }]
       ,["CheckFbCalib", g_VerboseL0] // -test 48
       ,["ClockPulseTest", g_VerboseL0] // -test 117
       ,["EccErrorInjectionTest", g_VerboseL1] // -test 268
       ,["EccFbTest", g_VerboseL1] // -test 155
       ,["FuseCheck", g_VerboseL0] // -test 196
       ,["GrIdle", g_VerboseL1] // -test 328
       ,["I2CTest", g_VerboseL0] // -test 50
       ,["I2MTest", g_VerboseL0] // -test 205
       ,["LineTest", g_VerboseL0] // -test 108
       ,["LSFalcon", g_VerboseL0] // -test 55
       ,["MME64Random", g_VerboseL0] // -test 193
       ,["Random2dFb", { // -test 58
            // Uncomment for shorter runtime
            TestConfiguration: {
                Loops: 10,
                Verbose: true
            },
            TestDescription: "Level0"
            ,Golden : { SkipCount: 5}
        }]
       ,["Random2dNc", { // -test 9
            // Uncomment for shorter runtime
            TestConfiguration: {
                Loops: 10
                ,Verbose: true
            },
            TestDescription: "Level0"
            ,Golden : { SkipCount: 5}
        }]
        ,["WriteOrdering", g_VerboseL0] //-test 258
        ,["LWSpecs", { // -test 14
            Override: true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
    ]);
}
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

//Note: These tests require a pstate enabled VBIOS to run.
// For speed change we want to test each link speed 1-5 the same number of times instead of
// randomly picking a speed to test. This will insure that we will test all 5 speeds as long as
// Loops >= 5
function specPcie(testList)
{
    testList.AddTests([
        ["CpyEngTest", g_VerboseL0] // -test 100
        ,["PcieSpeedChange", {  // -test 30
            TestConfiguration : {
               Loops : 10
               , Verbose : true
            }
            , TestDescription: "Level1"
            , "FancyPickerConfig" : [ [ FPK_PEXSPEEDTST_LINKSPEED, [ "list", "wrap"
                      ,[1, FPK_PEXSPEEDTST_LINKSPEED_gen1 ]
                      ,[1, FPK_PEXSPEEDTST_LINKSPEED_gen2 ]
                      ,[1, FPK_PEXSPEEDTST_LINKSPEED_gen3 ]
                      ,[1, FPK_PEXSPEEDTST_LINKSPEED_gen4 ]
                      ,[1, FPK_PEXSPEEDTST_LINKSPEED_gen5 ]
                      ] ] ]
        }] 
        ,["PexBandwidth", { // -test 146
           SurfWidth : 128
          ,SurfHeight : 16
          ,IgnoreBwCheck : true
          ,LinkSpeedsToTest : 0x1f
          ,TestConfiguration : {
               Loops : 10
               ,Verbose : true
           }
           ,TestDescription: "Level1"
        }] 
        ,["PexAspm", g_VerboseL1] // -test 26
    ]);
}

function specGlrFull(testList)
{
    let glrArgsL0 =
    {
        FrameRetries : 0,
        LogShaderCombinations : true,
        TestConfiguration :
        {
            Verbose : true
            // Uncomment for shorter runtimes
            ,SurfaceWidth: 320
            ,SurfaceHeight: 240
        },
        TestDescription: "Level0"
    };
    let glrArgsL1 =
    {
        FrameRetries : 0,
        LogShaderCombinations : true,
        TestConfiguration :
        {
            Verbose : true
            // Uncomment for shorter runtimes
            ,SurfaceWidth: 320
            ,SurfaceHeight: 240
        },
        TestDescription: "Level1"
    };

    testList.AddTests([
       ["GLRandomCtxSw", glrArgsL1] // -test 54 hang, need to debug.
       ,["GLRandomOcg", glrArgsL1] // -test 126
       ,["GlrA8R8G8B8", glrArgsL0] // -test 130, runtime ~6 hours gpugen on trim_111
       ,["GlrR5G6B5", glrArgsL0] // -test 131
       ,["GlrFsaa2x", glrArgsL0] // -test 132
       ,["GlrFsaa4x", glrArgsL0] // -test 133
       ,["GlrMrtRgbU", glrArgsL0] // -test 135
       ,["GlrMrtRgbF", glrArgsL0] // -test 136
       ,["GlrY8", glrArgsL0] // -test 137
       ,["GlrFsaa8x", glrArgsL0] // -test 138
       ,["GlrFsaa4v4", glrArgsL0] // -test 139
       ,["GlrFsaa8v8", glrArgsL0] // -test 140
       ,["GlrFsaa8v24", glrArgsL0] // -test 141
       ,["GlrA8R8G8B8Sys", glrArgsL1] // -test 148
       ,["GlrReplay", { // -test 149
            FrameReplayMs: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level1"
        }]
       ,["GlrZLwll", glrArgsL1] // -test 162
       ,["GlrLayered", glrArgsL0] // -test 231
    ]);
}
specGlrFull.specArgs = "-regress_miscompare_count 2 -regress_using_gltrace -verbose"

// For memory tests that do not use LWCA
function specMemoryNoLwda(testList)
{
    testList.AddTests([
        ["MatsTest", { // -test 3
            Coverage : 0.05,
            MaxFbMb : 16,
            TestConfiguration : { Verbose : true },
            TestDescription: "Level0"
        }]
        ,["NewWfMats", { // -test 94
            MaxFbMb : 2,
            BoxHeight: 2,
            PatternSet: 1,
            VerifyChannelSwizzle : true,
            TestConfiguration : { Verbose : true },
            TestDescription: "Level0"
        }]
       ,["Bar1RemapperTest", { // -test 151
            TestConfiguration : {
                RestartSkipCount : 2,
                Loops : 2,
                Verbose : true
            },
            TestDescription: "Level0"
        }]
       ,["ByteTest", { // -test 18
            Coverage : 0.05,
            MaxFbMb : 2,
            UpperBound : 250,
            TestConfiguration : {
                Verbose : true
            },
            TestDescription: "Level0"
       }]
       ,["FastMatsTest", { // -test 19
            MaxFbMb : 32,
            TestConfiguration : { Verbose : true },
            TestDescription: "Level0"
        }]
       ,["MarchTest", { // -test 52
            Coverage : 0.05,
            MaxFbMb : 4,
            TestConfiguration : { "Verbose" : true },
            TestDescription : "Level0"
        }]
        ,["MemScrub", g_VerboseL0] // -test 159
        ,["NewWfMatsBusTest", { // -test 123
            MaxFbMb : 2,
            BoxHeight: 2,
            PatternSet: 1,
            VerifyChannelSwizzle : true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["NewWfMatsCEOnly", { // -test 161
            MaxFbMb: 2,
            BoxHeight: 2,
            PatternSet: 1,
            VerifyChannelSwizzle: true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["NewWfMatsNarrow", { // -test 180
            MaxFbMb : 2,
            BoxHeight: 2,
            VerifyChannelSwizzle : true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["NewWfMatsShort", { // -test 93
            MaxFbMb : 2,
            BoxHeight: 2,
            PatternSet: 1,
            VerifyChannelSwizzle : true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["PatternTest", { // -test 70
            Coverage : 0.05,
            MaxFbMb : 2,
            TestConfiguration : {
                Verbose : true
            },
            TestDescription: "Level0"
        }]
        ,["WfMatsBgStress", { // -test 178 (94 + 2)
            // test 94 parameters
            MaxFbMb : 2,
            InnerLoops : 1,
            BoxHeight : 2,
            PatternSet: 1,
            VerifyChannelSwizzle : true,
            // test 2 parameters
            bgTestArg : {
                UseTessellation : true,
                DumpPngOnError : true,
                DumpFrames : 5,
                TestConfiguration : { Verbose : true }
            },
            TestConfiguration: { Verbose: true },
            TestDescription: "Level1"
        }]
        ,["WfMatsMedium", { // -test 118
            MaxFbMb : 2,
            BoxHeight: 2,
            PatternSet: 1,
            VerifyChannelSwizzle : true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
    ]);
}

// For all LWCA tests except the CaskLinpack variants and LwLink tests
function specLwda(testList)
{
    testList.AddTests([
         ["LwdaAppleAddrTest", { // -test 115
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaAppleKHTest", { // -test 116
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaAppleMOD3Test", { // -test 114
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaBoxTest", { // -test 110
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaByteTest", { // -test 111
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaCbu", { // -test 226
            //StressFactor: 32, // Uncomment for shorter runtime
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaColumnTest", { // -test 127
            MaxFbMb : 2,
            Iterations : 3,
            ColumnStep : 3,
            Patterns : [0x0,0xFFFFFFFF],
            TestConfiguration: {
                Verbose: true,
                ShortenTestForSim: true
            },
            TestDescription: "Level0"
        }]
        ,["LwdaInstrPulse", g_VerboseL1] // -test 318
        ,["LwdaLdgTest", { // -test 184
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaMarchTest", { // -test 112
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaMatsPatCombi", { // -test 144
            Iterations : 1,
            TestConfiguration : {
                ShortenTestForSim : true,
                Verbose : true
            },
            TestDescription: "Level1"
        }]
        ,["LwdaMatsTest", { // -test 87
            NumThreads : 32,
            MaxFbMb : 2,
            Iterations : 1,
            TestConfiguration : {
                ShortenTestForSim : true,
                Verbose : true
            },
            TestDescription: "Level0"
        }]
        ,["LwdaMmioYield", g_VerboseL0] // -test 221
        ,["LwdaPatternTest", { // -test 113
            MaxFbMb: 2,
            Iterations: 1,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
         }]
        ,["LwdaRadixSortTest", { // -test 185
            PercentOclwpancy: 0.01,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaRandom", g_VerboseL0] // -test 119
        ,["LwdaStress2", { // -test 198
            Iterations: 1,
            MinFbTraffic: true,
            LoopMs: 1,
            MaxFbMb: 2,
            NumThreads: 32,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaSubctx", { // -test 223
            EndVal: 1024,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaXbar", {
            InnerIterations: 50,
            TestConfiguration: { Loops: 1, Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaGXbar", {
            VirtualTestId: "LwdaGXbar - ReadOnly",
            RuntimeMs: 0,
            InnerIterations: 32,
            ReadOnly: true,
            TestConfiguration: { Loops: 2, Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaGXbar", {
            VirtualTestId: "LwdaGXbar - ReadWrite",
            RuntimeMs: 0,
            InnerIterations: 32,
            ReadOnly: false,
            TestConfiguration: { Loops: 2, Verbose: true },
            TestDescription: "Level0"
        }]
        ,["DPStressTest", { // -test 190
            TestConfiguration: { ShortenTestForSim: true, Verbose: true },
            TestDescription: "Level0"
        }]
    ]);
}
specLwda.specArgs = g_LwdaWARArgs;

// These tests are going to take a long time on emulation.
function specLwdaSchmoo(testlist)
{
    testList.AddTests([
        ["LwdaColumnShmooTest", { // -test 227
            MaxFbMb : 2,
            Iterations : 3,
            ColumnStep : 3,
            Patterns : [0x0,0xFFFFFFFF],
            TestConfiguration: {
                Verbose: true,
                ShortenTestForSim: true
            },
            TestDescription: "Level1"
        }]
        ,["LwdaMatsShmooTest", { // -test 187
            MaxFbMb : 2,
            Iterations : 3,
            TestConfiguration : {
                ShortenTestForSim : true,
                Verbose : true
            },
            TestDescription: "Level1"
        }]
    ]);
}
specLwdaSchmoo.specArgs = g_LwdaWARArgs;

function specNewLwdaMats(testList)
{
    let ncmArgsL0 = {
       MaxFbMb : 2,
       Iterations : 3,
       TestConfiguration: {
           ShortenTestForSim : true,
           Verbose : true
       },
       TestDescription: "Level0"
    };
    let ncmArgsL1 = {
       MaxFbMb : 2,
       Iterations : 3,
       TestConfiguration: {
           ShortenTestForSim : true,
           Verbose : true
       },
       TestDescription: "Level1"
    };

    let ncmArgsL1Bg = {
       MaxFbMb : 2,
       Iterations : 3,
       TestConfiguration: {
           ShortenTestForSim : true,
           Verbose : true
       },
       TestDescription: "Level1",
       bgTestArg:{ "MNKMode": 2, "Msize": 256, "Nsize": 256, "Ksize": 128, "TestConfiguration":{Loops:10}}
    };

    testList.AddTests([
        ["NewLwdaMatsTest", ncmArgsL0] // -test 143
       ,["NewLwdaMatsReadOnly", ncmArgsL0] // -test 241
       ,["NewLwdaMatsRandom", ncmArgsL0] // -test 242
       ,["NewLwdaMatsStress", ncmArgsL1Bg] // -test 243
       ,["NewLwdaMatsReadOnlyPulse", ncmArgsL1] // -test 354
       ,["NewLwdaMatsRandomPulse", ncmArgsL1] // -test 355
       ,["NewLwdaL2Mats", ncmArgsL1] // -test 351
    ]);
}
specNewLwdaMats.specArgs = g_LwdaWARArgs;

// For all the CaskLinpack tests
function specCask(testList)
{
    let caskArgsL0 =
    {
        TestDescription: "Level0",
        RuntimeMs: 0,
        TestConfiguration: { Verbose: true, Loops : 10 },
        MNKMode: 2,
        Msize: 256,
        Nsize: 256,
        Ksize: 128
    };

    let caskArgsL1 =
    {
        TestDescription: "Level1",
        RuntimeMs: 0,
        TestConfiguration: { Verbose: true, Loops : 10 },
        MNKMode: 2,
        Msize: 256,
        Nsize: 256,
        Ksize: 128
    };

    testList.AddTests([
        ["CaskLinpackSgemm", caskArgsL0] // -test 600
       ,["CaskLinpackPulseSgemm", caskArgsL1] // -test 601
       ,["CaskLinpackIgemm", caskArgsL0] // -test 602
       ,["CaskLinpackDgemm", caskArgsL0] // -test 604
       ,["CaskLinpackHgemm", caskArgsL0] // -test 606
       ,["CaskLinpackHMMAgemm", caskArgsL0] // -test 610
       ,["CaskLinpackIMMAgemm", caskArgsL0] // -test 612
       ,["CaskLinpackDMMAgemm", caskArgsL0] // -test 614
       ,["CaskLinpackE8M10", caskArgsL0] // -test 620
       ,["CaskLinpackE8M7", caskArgsL0] // -test 622
       ,["CaskLinpackSparseHMMA", caskArgsL0] // -test 630
       ,["CaskLinpackSparseIMMA", caskArgsL0] // -test 632
       ,["CaskLinpackBMMA", caskArgsL0] // -test 640
       ,["CaskLinpackINT4", caskArgsL0] // -test 642  // LW_PGRAPH_PRI_GPC0_TPC0_SM0_HWW_WARP_ESR_ERROR_ILLEGAL_INSTR_ENCODING
    ]);
}
specCask.specArgs = g_LwdaWARArgs;

// Use Netlist: NET26/full_111_1LWDEC_1JPG_OFA_HBM3_fixECC_soqAP2102s2_2X_FV_6BD_AP_0.qt
function specVideo(testList)
{
    testList.AddTests([
        ["LwvidLWDEC", g_VerboseL0] // -test 360 Bug 3304823
       ,["LwvidLwOfa", { // -test 362
            TestDescription: "Level0",
            TestConfiguration: { Verbose: true }
        }]
       ,["LwvidLWJPG", g_VerboseL0] // -test 361
    ]);
}
specVideo.specArgs = g_LwdaWARArgs;

function specLwLink(testList)
{
    testList.AddTests([
        ["LwLinkBwStress", g_VerboseL0] // -test 246
       ,["LwdaLwLinkAtomics", g_VerboseL1] // -test 56
       ,["LwdaLwLinkGupsBandwidth", g_VerboseL1] // -test 251
       ,["LwlbwsBeHammer", g_VerboseL1] // -test 28
       ,["LwlbwsBgPulse", g_VerboseL1] // -test 37
       ,["LwlbwsLowPower", g_VerboseL1] // -test 64
       ,["LwlbwsLpModeHwToggle", g_VerboseL1] // -test 77
       ,["LwlbwsLpModeSwToggle", g_VerboseL1] // -test 16
       ,["LwlbwsSspmState", g_VerboseL2] // -test 5
       ,["LwLinkBwStressPerfSwitch", g_VerboseL2] // -test 5
       ,["LwLinkEyeDiagram", g_VerboseL0] // -test 248
       ,["LwLinkState", g_VerboseL0] // -test 252
       ,["LwlValidateLinks", g_VerboseL0] // -test 254
    ]);
}
specLwLink.specArgs = g_LwdaWARArgs;

// For OpenGL tests that are not GLRandom or GLStress variants
function specGLMisc(testList)
{
    testList.AddTests([
       ["PathRender", g_VerboseL0] // -test 176
       ,["GLVertexFS", g_VerboseL0] // -test 179
       ,["GLComputeTest", g_VerboseL0] // -test 201
       ,["GLHistogram", g_VerboseL0] // -test 274
       ,["FillRectangle", g_VerboseL0] // -test 286
       ,["MsAAPR", g_VerboseL1] // -test 287
       ,["GLPRTir", g_VerboseL1] // -test 289
    ]);
}

// For variants of GLStress
function specGLStress(testList)
{
    let glsArgsL0 =
    {
        LoopMs: 0,
        TestConfiguration: { Loops: 4, Verbose: true },
        TestDescription: "Level0"
    };
    let glsArgsL1 =
    {
        LoopMs: 0,
        TestConfiguration: { Loops: 4, Verbose: true },
        TestDescription: "Level1"
    };
    testList.AddTests([
        ["GLStress", glsArgsL0] // -test 2
       ,["GLStressDots", glsArgsL0] // -test 23
       ,["GLStressPulse", glsArgsL1] // -test 92
       ,["GLStressFbPulse", glsArgsL1] // -test 95
       ,["GLStressNoFb", glsArgsL1] // -test 152
       ,["GLPowerStress", glsArgsL1] // -test 153
       ,["GLStressZ", glsArgsL0] // -test 166
       ,["GLStressClockPulse", glsArgsL1] // -test 177
       ,["GLInfernoTest", glsArgsL1] // -test 222
    ]);
}

// For variants of VkStress and other Vulkan tests
function specVulkan(testList)
{
    let vkStressArgsL0 = {
        LoopMs: 0,
        FramesPerSubmit: 1,
        NumTextures: 2,
        TestConfiguration: {
            Loops: 4,
            Verbose: true
        },
        TestDescription: "Level0"
    };
    let vkStressArgsL1 = {
        LoopMs: 0,
        FramesPerSubmit: 1,
        NumTextures: 2,
        TestConfiguration: {
            Loops: 4,
            Verbose: true
        },
        TestDescription: "Level1"
    };

    testList.AddTests([
        ,["VkPowerStress", vkStressArgsL1] // -test 261
        ,["VkHeatStress", vkStressArgsL1] // -test 262
        ,["VkStressFb", vkStressArgsL1] // -test 263
        ,["VkStressMesh", vkStressArgsL1] // -test 264
        ,["VkTriangle", g_VerboseL0] // -test 266
        ,["VkStress", vkStressArgsL0] // -test 267
        ,["VkStressCompute", vkStressArgsL1] // -test 367
        ,["VkStressSCC", vkStressArgsL1] // -test 366
    ]);
}

function userSpec(testList)
{
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
    specDisplay(testList);
}

