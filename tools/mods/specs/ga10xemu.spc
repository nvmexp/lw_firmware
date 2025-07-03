/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

g_SpecArgs = "";

let g_VerboseL0 = { TestDescription: "Level0", TestConfiguration: { Verbose: true }};
let g_VerboseL1 = { TestDescription: "Level1", TestConfiguration: { Verbose: true }};
let g_VerboseL2 = { TestDescription: "Level2", TestConfiguration: { Verbose: true }};

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
               //,Loops: 100, // Uncomment for shorter runtime
            },
            TestDescription: "Level0"
        }]
       ,["CheckFbCalib", g_VerboseL0] // -test 48
       ,["CheckOvertemp", g_VerboseL1] // -test 65, TODO: likely not supported on emu
       ,["CheckThermalSanity", g_VerboseL1] // -test 31, TODO: likely not supported on emu
       ,["ClockPulseTest", g_VerboseL0] // -test 117
       ,["EccErrorInjectionTest", g_VerboseL1] // -test 268
       ,["EccFbTest", g_VerboseL1] // -test 155
       ,["FuseCheck", g_VerboseL0] // -test 196
       ,["GpuResetTest", g_VerboseL1] // -test 175
       ,["GrIdle", g_VerboseL1] // -test 328
       ,["I2CTest", g_VerboseL0] // -test 50
       ,["I2MTest", g_VerboseL0] // -test 205
       ,["LineTest", g_VerboseL0] // -test 108
       ,["LSFalcon", g_VerboseL0] // -test 55
       ,["MME64Random", g_VerboseL0] // -test 193
       ,["Random2dFb", { // -test 58
            // Uncomment for shorter runtime
            TestConfiguration: {
                //Loops: 10,
                Verbose: true
            },
            TestDescription: "Level0"
            //Golden : { SkipCount: 5}
        }]
       ,["Random2dNc", { // -test 9
            // Uncomment for shorter runtime
            TestConfiguration: {
                //Loops: 10,
                Verbose: true
            },
            TestDescription: "Level0"
            //Golden : { SkipCount: 5}
        }]
        ,["Sec2", g_VerboseL1]
        ,["WriteOrdering", g_VerboseL0]
        ,["LWSpecs", {
            Override: true,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
    ]);
}

function specGlrFull(testList)
{
    let glrArgsL0 =
    {
        FrameRetries : 0,
        //LogShaderCombinations : true,
        TestConfiguration :
        {
            Verbose : true
            // Uncomment for shorter runtimes
            //,SurfaceWidth: 320
            //,SurfaceHeight: 240
        },
        TestDescription: "Level0"
    };
    let glrArgsL1 =
    {
        FrameRetries : 0,
        //LogShaderCombinations : true,
        TestConfiguration :
        {
            Verbose : true
            // Uncomment for shorter runtimes
            //,SurfaceWidth: 320
            //,SurfaceHeight: 240
        },
        TestDescription: "Level1"
    };

    testList.AddTests([
        ["GLRandomCtxSw", glrArgsL1] // -test 54
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
            PatternSet: 1,
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
        ,["LwdaColumnShmooTest", { // -test 227
            Iterations : 3,
            ColumnStep : 3,
            Patterns : [0x0,0xFFFFFFFF],
            TestConfiguration: {
                Verbose: true,
                ShortenTestForSim: true
            },
            TestDescription: "Level1"
        }]
        ,["LwdaInstrPulse", g_VerboseL1] // -test 318
        ,["LwdaL2Mats", { // -test 154
            Iterations: 50,
            TestConfiguration: { Verbose: true },
            TestDescription: "Level0"
        }]
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
            Iterations : 3,
            TestConfiguration : {
                ShortenTestForSim : true,
                Verbose : true
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
        ,["LwdaTTUBgStress", { // -test 341
            RuntimeMs: 0,
            InnerIterations: 32,
            TestConfiguration: { Loops: 1, Verbose: true },
            bgTestNum: 611,
            bgTestArg:{
                "MNKMode": 2, "Msize": 256, "Nsize": 256, "Ksize": 128, "TestConfiguration":{Loops:10}
            },
            TestDescription: "Level1"
        }]
        ,["LwdaTTUStress", { // -test 340
            RuntimeMs: 0,
            InnerIterations: 32,
            TestConfiguration: { Loops: 1, Verbose: true },
            TestDescription: "Level0"
        }]
        ,["LwdaXbar", {
            InnerIterations: 50,
            TestConfiguration: { Loops: 1, Verbose: true },
            TestDescription: "Level0"
        }]
        ,["DPStressTest", { // -test 190
            TestConfiguration: { ShortenTestForSim: true, Verbose: true },
            TestDescription: "Level0"
        }]
    ]);
}

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
    ]);
}

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
       ,["CaskLinpackINT4", caskArgsL0] // -test 642
    ]);
}

// TODO: MODS video engineers to fill out the args
function specVideo(testList)
{
    testList.AddTests([
        ["LwvidLWDEC", g_VerboseL0] // -test 360
       ,["LwvidLwOfa", {
            MaxImageWidth: 128,
            MaxImageHeight: 128, 
            TestDescription: "Level0",
            TestConfiguration: { Verbose: true }
        }] // -test 362
       ,["LwvidLWENC", g_VerboseL0] // -test 363
    ]);
}

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

// For OpenGL tests that are not GLRandom or GLStress variants
function specGLMisc(testList)
{
    testList.AddTests([
        ["GLCombustTest", g_VerboseL0] // -test 172
       ,["PathRender", g_VerboseL0] // -test 176
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
       //,["HeatStressTest", glsArgsL1] // -test 73 (not supported on emu)
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
         ["VkStressPulse", vkStressArgsL1] // -test 192
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

// TODO: MODS display engineers to fill this out
function specDisplay(testList)
{
    testList.AddTests([
         ["LwDisplayRandom", g_VerboseL0] // -test 304
        ,["LwDisplayHtol", g_VerboseL1] // -test 317
    ]);
}

function userSpec(testList)
{
    specBasic(testList);
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

