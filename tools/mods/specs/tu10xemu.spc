/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "lwr_comm.h"

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/tu10xemu.spc#6 $");

// Instructions for running emulation tests:
//  - https://confluence.lwpu.com/display/GM/Pre-Silicon+Testing#Pre-SiliconTesting-EmulationTesting

// For automated runs disable CLS, otherwise enable it for the block you suspect.
SetElw("CSL","[disable]");
//SetElw("CSL","smm_core"); // enable lwca debugging messages

g_SpecArgs  = "-only_family turing -timeout_ms 0x150000 -rmkey RmCePceMap 0xFFFF32FF";
g_SpecArgs += " -rmkey RmCePceMap1 0xFFFFFFFF -rmkey RmCeGrceShared 0x32 -hw_speedo_check_ignore";
g_SpecArgs += " -force_repost -skip_pertest_pexcheck -null_display -no_vga";
// Note: -no_rcwd is not supported on SIM builds
g_SpecArgs += "  -no_rcwd";

// This spec requires a Video enabled emulator to test
function videoSpec(testList)
{
    testList.AddTests([
        ["LWDECTest", { //-test 277
            "DisableEncryption" : true,
            "SaveFrames" : true,
            "H264Streams" : [], //don't run any H264 streams
            "H265Streams" : [1], // use stream "in_to_tree-002.265"
            "TestConfiguration" : {
                "Verbose" : true,
                "ShortenTestForSim" : true
            }
        }]
        ,["LWENCTest", { //-test 278
            "StreamSkipMask" : ~((1 << LwencStrm.H264_BASELINE_CAVLC) | // run only H264 Baseline CAVLC &
                                 (1 << LwencStrm.H265_GENERIC)),        // H265 Generic streams
            "MaxFrames" : 10,       // no more that 10 frames per stream
            "EngineSkipMask" : 0xfffffffe, // only run on engine 0
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
    ]);
}

// This spec requires an ECC enabled ROM to test
function eccSpec(testList)
{
    testList.AddTests([
        ["LwdaEccL2", {         // Test 256 - Turing runtime: ? Bug 2060913
            TestConfiguration: { Loops: 2 },
            Iterations : 2
        }]
    ]);
}

// This spec requires TTU enabled emulator to test
function ttuSpec(testList)
{
    testList.AddTests([
        ["LwdaTTUStress"]       // Test 340 - Turing runtime: ?
    ]);
}

// This spec requires a 2+ SM emulator to test
function vkSpec(testList)
{
    testList.AddTests([
        ["VkStress", {          // Test 267 - Turing runtime: ?
            "LoopMs" : 0,
            "FramesPerSubmit" : 4,
            "NumLights" : 1,
            "SampleCount" : 2,
            "NumTextures" : 2,
            "TestConfiguration" : {
                "Loops" : 4,
                "Verbose" : true
            }
        }]
        ,["VkStressMesh", {     // Test 264 - Turing runtime: 640s
            "LoopMs" : 0,
            "FramesPerSubmit" : 4,
            "NumLights" : 1,
            "SampleCount" : 2,
            "NumTextures" : 2,
            "TestConfiguration" : {
                "Loops" : 4,
                "Verbose" : true
            }
        }]
    ]);
}

// This spec requires a 2+ SM emulator to test and a netlist the supports FP64 instructions
function glRandomSpec(testList)
{
    var skipCount = 10;
    var frames = 10;

    testList.AddTests([
        ["GLRandomCtxSw", { //-test 54 (test 130 in fg, test 2 in bg)
            "ClearLines" : false,
            "FrameRetries" : 0,
            "LogShaderCombinations" : true,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            // test 2 args
            "bgTestArg" : {
                "DumpPngOnError" : true,
                "UseTessellation" : true,
                "LoopMs" : 5,
                "Golden" : { "SkipCount" : skipCount }
            }
        }]
        ,["GlrA8R8G8B8", { //-test 130
            "ClearLines" : false,
            "FrameRetries" : 0,
            "LogShaderCombinations" : true,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrR5G6B5", { //-test 131
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrFsaa2x", { //-test 132
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrFsaa4x", { //-test 133
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrMrtRgbU", { //-test 135
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrMrtRgbF", { //-test 136
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrY8", { //-test 137
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrFsaa8x", { //-test 138
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
         ,["GlrZLwll", { //-test 162
             "ClearLines" : false,
             "LogShaderCombinations" : true,
             "FrameRetries" : 0,
             "TestConfiguration" : {
                 "ShortenTestForSim" : true,
                 "RestartSkipCount" : skipCount,
                 "Loops" : 5*skipCount,
                 "StartLoop" : 0,
                 "Verbose" : true
             },
             "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GlrLayered", { //-test 231
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["GLRandomOcg", { //-test 126
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
        ,["PathRender", { //-test 176
            "TestConfiguration" : {
               "RestartSkipCount" : 10,
               "Loops" : 10,
               "StartLoop" : 0,
               "Verbose" : true
            },
            "Golden" : { "SkipCount" : 10 }
         }]
       ,["FillRectangle", { //-test 286
         "TestConfiguration" : {
               "RestartSkipCount" : 1,
               "Loops" : 1,
               "StartLoop" : 0,
              "Verbose" : true
         }
       }]
       ,["MsAAPR", { //-test 287
          "TestConfiguration" : {
               "RestartSkipCount" : 10,
               "Loops" : 10,
               "StartLoop" : 0,
               "Verbose" : true
          },
          "Golden" : { "SkipCount" : 10 }
       }]
       ,["GLPRTir", { //-test 289
           "TestConfiguration" : {
               "RestartSkipCount" : 1,
               "Loops" : 6,
               "StartLoop" : 0,
               "Verbose" : true
           },
           "Golden" : { "SkipCount" : 10 }
        }]
    ]);
}

function testedSpec(testList)
{
    var skipCount = 10;
    var frames = 10;

    testList.AddTests([
        ["MatsTest", {              // Test 3   - Turing runtime: 371s
            "Coverage" : 0.05,
            "MaxFbMb" : 16,
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
        ,["FastMatsTest", {         // Test 19  - Turing runtime: 3411s
            "MaxFbMb" : 32,
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
        ,["CheckDma", {        // Test 41  - Turing runtime: 25586s
               "TestConfiguration" : {
                   "Loops" : 100,
                   "Verbose" : true
               }
        }]
        ,["WfMatsMedium", {         // Test 118 - Turing runtime: 1067s
            MaxFbMb : 128/1024,
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true
        }]
        ,["NewWfMatsBusTest", {     // Test 123 - Turing runtime: 1700s
            MaxFbMb : 128/1024,
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true
        }]
        ,["NewWfMatsNarrow", {      // Test 180 - Turing runtime: 1700s
            MaxFbMb : 128/1024,
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true
        }]
        ,["NewWfMatsCEOnly", {      // Test 161 - Turing runtime: 374s
            MaxFbMb : 2048/1024,
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true
        }]
        ,["GLStress", {             // Test   2 - Turing runtime: 7864s
            "LoopMs" : 0,
            "TestConfiguration" : {"Loops" : 20}
        }]
        ,["GLStressPulse", {        // Test  92 - Turing runtime: 8006s
            "LoopMs" : 0,
            "TestConfiguration" : {"Loops" : 20}
        }]
        ,["GLStressFbPulse", {      // Test  95 - Turing runtime: 8200s
            "LoopMs" : 0,
            "TestConfiguration" : {"Loops" : 20}
        }]
        ,["LwdaMatsTest", {         // Test  87 - Turing runtime: 119s
            "NumThreads" : 32,
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["LwdaByteTest", {         // Test 111 - Turing runtime: 1413s
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["LwdaRadixSortTest", {    // Test 185 - Turing runtime: 1282s
            "PercentOclwpancy" : 0.1
        }]
        ,["PathRender", {           // Test 176 - Turing runtime: 8730s
            "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            }
        }]
        ,["FillRectangle", {        // Test 286 - Turing runtime: 24616s
            "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            }
        }]
        ,["MsAAPR", {               // Test 287 - Turing runtime: 34273s
            "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            }
        }]
        ,["GLPRTir", {              // Test 289 - Turing runtime: 25708s
            "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            }
        }]
        ,["LwdaLinpackHgemm", {    // Test 210 - Turing runtime: 210s
            Ksize: 128,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,["CpyEngTest", {           // Test 100 - Turing runtime: 5600s
            "TestConfiguration" : {
                "Loops" : 1500,
                "Verbose" : true
            }
        }]
        ,["GLPowerStress", {        // Test 153 - Turing runtime: 20000s (perf issue?)
            "LoopMs" : 0,
            "TestConfiguration" : {"Loops" : 20}
        }]
        ,["GLStressZ", {            // Test 166 - Turing runtime: 406s
            "LoopMs" : 0,
            "TestConfiguration" : {"Loops" : 20}
        }]
        ,["LwdaStress2", {          // Test 198 - Turing runtime: 11100s
            "LoopMs" : 50,
            "Iterations" : 2,
            "NumBlocks" : 1,
            "MinFbTraffic" : true,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["WfMatsBgStress", {       // Test 178 (94 + 2) - Turing runtime: 4665s
            // test 94 parameters
            "MaxFbMb" : 2,
            "InnerLoops" : 1,
            "BoxHeight" : 2,
            "PatternSet" : [0],
            "VerifyChannelSwizzle" : true,
            // test 2 parameters
            "bgTestArg" : {
                "UseTessellation" : true,
                "DumpPngOnError" : true,
                "DumpFrames" : 5,
                "TestConfiguration" : { "Verbose" : true }
            }
        }]
        ,["LwdaLinpackIgemm", {     // Test 212 - Turing runtime: 152s
            Ksize: 128,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,["LwdaXbar2", {            // Test 240 - Turing runtime: 402s
            "Iterations" : 2,
            "InnerIterations" : 1024,
            "Mode" : 0x3
        }]
        ,["LwdaBoxTest", {          // Test 111 - Turing runtime: 186s
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["CheckConfig"]            // Test 1   - Turing runtime: <1s
        ,["LwdaCbu", {              // Test 226 - Turing runtime: 1526s
            "StressFactor" : 32
        }]
        ,["MemScrub", {             // Test 159 - Turing runtime: 35s
            "MaxFbMb" : 16
        }]
        ,["LwdaColumnTest", {       // Test 127 - Turing runtime: 369s
            "MaxFbMb" : 2,
            "Iterations" : 3,
            "ColumnStep" : 3,
            "Patterns" : [0x0,0xFFFFFFFF],
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["I2MTest"]                // Test 205 - Turing runtime: 353
        ,["MarchTest", {            // Test 52  - Turing runtime: 61765
            "Coverage" : 0.05,
            "MaxFbMb" : 4,
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
        ,["ByteTest", {             // Test 18  - Turing runtime: 107032s
            "Coverage" : 0.05,
            "MaxFbMb" : 4,
            "UpperBound" : 250,
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
    ]);
}

// These tests requires a netlist with FP64 support! The last netlist used was:
// NET21/full_111_2SLICE_NoDV_GDDR6_16Gb_soqAP1651_p_2X_FV_2BD_AP_newpads_2.qt on Missouri22
// emulator.
function regressSpecFP64_0(testList)
{
    testList.AddTests([
        ,["LwdaRandom", {           // Test 119 - Turing runtime: 1770s
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 12, // one loop for each kernel
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : 1 },
            "FancyPickerConfig" : [ [ CRTST_CTRL_KERNEL, [ "list","wrap"
                ,[1, CRTST_CTRL_KERNEL_SeqMatrix         ]
                ,[1, CRTST_CTRL_KERNEL_SeqMatrixDbl      ]
                ,[1, CRTST_CTRL_KERNEL_SeqMatrixStress   ]
                ,[1, CRTST_CTRL_KERNEL_SeqMatrixDblStress]
                ,[1, CRTST_CTRL_KERNEL_SeqMatrixInt      ]
                ,[1, CRTST_CTRL_KERNEL_Stress            ]
                ,[1, CRTST_CTRL_KERNEL_Branch            ]
                ,[1, CRTST_CTRL_KERNEL_Texture           ]
                ,[1, CRTST_CTRL_KERNEL_Surface           ]
                ,[1, CRTST_CTRL_KERNEL_MatrixInit        ]
                ,[1, CRTST_CTRL_KERNEL_Atom              ]
                ,[1, CRTST_CTRL_KERNEL_GpuWorkCreation   ]
            ]]]
        }]
        ,["LwdaLinpackDgemm", {     // Test 199 - Turing runtime: 442s
            Ksize: 128,
            UseCrcToVerify : false,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 20,
                "Verbose" : true
            }
        }]
        ,["LwdaLinpackPulseDP", { //-test 292
            Ksize: 24,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
    ]);
}

function regressSpec_0(testList)
{
    testList.AddTests([
        ["Random2dFb", {            // Test 58  - Turing runtime: 128s
            "TestConfiguration" : {
                "Loops" : 10,
                "Verbose" : true
            },
            "Golden" : {"SkipCount" : 5}
        }]
        ,["Random2dNc", {           // Test 9   - Turing runtime: 76s
            "TestConfiguration" : {"Loops" : 10},
            "Golden" : {"SkipCount" : 5}
        }]
        ,["DPStressTest", {         // Test 190 - Turing runtime: 134s
            "LoopMs" : 1,
            "InnerLoops" : 1,
            "UseSharedMemory" : true,
            "TestConfiguration" : {
                "Loops" : 2, // must be an even number.
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["NewWfMats", {            // Test 94  - Turing runtime: 1067s
            MaxFbMb : 128/1024,
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true
        }]
        ,["NewLwdaMatsTest", {      // Test 143 - Turing runtime: 408s
            "MaxFbMb" : 2,
            "Iterations" : 3,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["WriteOrdering"]          // Test 258 - Turing runtime: 672s
    ]);
}

function regressSpec_1(testList)
{
    testList.AddTests([
        ["GLStressDots", {          // Test  23 - Turing runtime: 491s
            "LoopMs" : 0,
            "TestConfiguration" : {"Loops" : 20}
        }]
        ,["LwdaLinpackSgemm", {     // Test 200 - Turing runtime: 430s
            Ksize: 128,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 30,
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsRandom", {    // Test 242 - Turing runtime: 234s
            "MaxFbMb" : 2,
            "Iterations" : 3,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["GLComputeTest", {        // Test 201 - Turing runtime: 665s
            "LoopMs" : 3000,
            "KeepRunning" : false,
            "MinSCGWorkLoad" : 51060,
            "PrintOwnership" : true,
            "TestSCG" : true,
            "TestConfiguration" : {
                "Loops" : 2,
                "ShortenTestForSim" : true
            }
        }]
        ,["LineTest", {             // Test 108 - Turing runtime: 108s
            LineSegments:       10,
            TestConfiguration:  {Loops: 2}
        }]
        ,["ClockPulseTest"]         // Test 117 - Turing runtime: 117s
    ]);
}

function untestedSpec(testList)
{
    testList.AddTests([
        ["LwdaLinpackPulseSP", { //-test 296
            Ksize: 24,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 2,
                "Verbose" : true
            }
        }]
        ,["LwdaLinpackPulseHP", { //-test 211
            Ksize: 24,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,["LwdaLinpackPulseIP", { //-test 213
            Ksize: 24,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 2,
                "Verbose" : true
            }
        }]
    ]);
}

function brokenSpec(testList)
{
    testList.AddTests([
        ["LwdaL2Mats", {           // Test 154 - Blocked on Bug 2041044
            "Iterations" : 50,
            "TestConfiguration" : { "Verbose" : true }
        }]
        ,["Bar1RemapperTest", {     // Test 151 - Blocked on Bug 2042433
            "TestConfiguration" : {
                "RestartSkipCount" : 2,
                "Loops" : 2,
                "StartLoop" : 0,
                "Verbose" : true
            }
        }]
        ,["GpuPllLockTest"]         // Test 170 - Turing runtime: ? Bug 2060961
        ,["CheckFbCalib"]           // Test 48  - Turing runtime: ? Bug 2060962

        // Push these tests to the end of the spec, since they hang/crash
        ,["GLInfernoTest"]          // Test 222 - Turing runtime: ?
        ,["GLHistogram", {          // Test 274 - Turing runtime: ? Bug 2061515
            "TestConfiguration" : {"Loops" : 20}
        }]
        ,["LwdaLinpackHMMAgemm", {  // Test 310 - Turing runtime: ? Bug 2060967
            Ksize: 128,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,["LwdaLinpackPulseHMMA", { // Test 311 - Turing runtime: ? Bug 2060967
            Ksize: 24,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,["LwdaLinpackIMMAgemm", {  // Test 312 - Turing runtime: ? Bug 2060969
            Ksize: 128,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 2,
                "Verbose" : true
            }
        }]
        ,["LwdaLinpackPulseIMMA", { // Test 313 - Turing runtime: ? Bug 2060969
            Ksize: 96,
            DumpMiscompares : true,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 2,
                "Verbose" : true
            }
        }]
        ,["PatternTest", {          // Test 70  - Turing runtime: ? Bug 2060975
            "Coverage" : 0.05,
            "MaxFbMb" : 1,
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsStress", {    // Test 243 - Turing runtime: ?
            "MaxFbMb" : 2,
            "Iterations" : 1,
            "NumBlocks" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["MME64Random", {      // Test 193
            "MacroSize" : 256,
            "SimOnly" : false,
            "SimTrace" : false,
            "DumpMacros" : false,
            "DumpSimEmissions" : false,
            "DumpMmeEmissions" : false,
            "NumMacrosPerLoop" : 12, //  Turing MME Instr RAM holds 1024 groups
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
            "TestConfiguration" : {
                "Loops" : 32,
                "Verbose" : true
            }
        }]
    ]);
}

function userSpec(testList)
{
    regressSpec_0(testList);
    regressSpec_1(testList);
    testedSpec(testList);
    untestedSpec(testList);
    brokenSpec(testList);
    videoSpec(testList);
    glRandomSpec(testList);
    vkSpec(testList);
    ttuSpec(testList);
    eccSpec(testList);
}

function emulationSpec(testList)
{
    userSpec(testList);
}
