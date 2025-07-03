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
#include "lwr_comm.h"

AddFileId(Out.PriNormal, "$Id: $");

// DVS uses this file to regress Fmodel with the following single command
// that both generates goldens and runs the specified test list:
// ./mods -chip adlit1_fmodel_64.so -gpubios 0 ad102_sim_gddr6.rom gputest.js -readspec ad102fmod.spc -spec specBasic //$
g_IsGenAndTest = true;
g_LoadGoldelwalues = false;

// For automated runs disable CSL, otherwise enable it for the block you suspect.
SetElw("CSL","[disable]");
//SetElw("CSL","smm_core"); // enable lwca debugging messages

SetElw("SC_COPYRIGHT_MESSAGE","DISABLE"); // WAR bug 2687346

// Chiplib args for FMODEL
g_SpecArgs  = " -chipargs -chipPOR=ad102";
g_SpecArgs += " -chipargs -texHandleErrorsLikeRTL"; // For textures in LwdaRandom
g_SpecArgs += " -chipargs -useVirtualization";
g_SpecArgs += " -chipargs -noinstance_devices=gr1,gr2,gr3,gr4,gr5,gr6,gr7,gr8,gr9,gr10,gr11";
g_SpecArgs += " -chipargs -use_floorsweep";
g_SpecArgs += " -chipargs -num_ropltcs=2";
g_SpecArgs += " -chipargs -num_fbps=1";
g_SpecArgs += " -chipargs -num_fbio=2";
g_SpecArgs += " -chipargs -num_tpcs=2";
g_SpecArgs += " -chipargs -fbpa_en_mask=0x0003";
g_SpecArgs += " -chipargs -ltc_en_mask=0x0003";
g_SpecArgs += " -chipargs -fbp_en_mask=0x01";
g_SpecArgs += " -chiplib_deepbind"; // Bug 2424418

// MODS args
g_SpecArgs += " -floorsweep gpc_enable:0x1:gpc_tpc_enable:0:0x3:gpc_pes_enable:0:0x1:fbp_ltc_enable:0:0x3:fbp_enable:0x1:fbio_enable:0x1"; //$
g_SpecArgs += " -devid_check_ignore";
g_SpecArgs += " -skip_pertest_pexcheck";
g_SpecArgs += " -run_on_error";
g_SpecArgs += " -maxwh 160 120";
g_SpecArgs += " -rmkey RMDisable1to4ComptaglineAllocation 1"; // See https://lwbugs/3363550

// Args to help speed things up
let g_FastArgsWithDisp = "";
let g_FastArgs = "";
g_FastArgsWithDisp += " -chipargs -noinstance_lwdec";
g_FastArgsWithDisp += " -chipargs -noinstance_lwenc";
g_FastArgsWithDisp += " -lwlink_force_disable";
g_FastArgsWithDisp += " -simulate_dfp TMDS_128x112";
g_FastArgsWithDisp += " -use_mods_console";
g_FastArgs = g_FastArgsWithDisp;
g_FastArgs += " -null_display";

// We can save several minutes from FMODEL runs by avoiding initializing or
// using the PMU, but some tests require it and we should be regressing it.
let g_NoPmuArgs = " -exelwte_rm_devinit_on_pmu 0 -pmu_bootstrap_mode 1";

function specNoTest(testList)
{
}
specNoTest.specArgs = "-no_test" + g_FastArgsWithDisp;

function specNoTestFast(testList)
{
}
specNoTestFast.specArgs = g_FastArgs + g_NoPmuArgs + " -no_test";

function specNoTestNullDisplay(testList)
{
}
specNoTestNullDisplay.specArgs = "-no_test -null_display";

function specBasic(testList)
{
    testList.AddTests([
        ["Random2dFb", { //-test 58
            "TestConfiguration" : {"Loops" : 5},
            "Golden" : {"SkipCount" : 5}
        }]

        ,["CpyEngTest", { //-test 100
            "TestConfiguration" : {"Loops" : 1}
        }]
    ]);
}
specBasic.specArgs = g_FastArgs + g_NoPmuArgs;

function specDisp(testList)
{
    testList.AddTests([
        ["LwDisplayDPC", { //-test 327
             "DPCFilePath" : "600x40_2Head1Or_DSC_fmodel.dpc"
        }],
        ["LwDisplayDPC", { //-test 327
             "DPCFilePath" : "128x112_fmodel.dpc"
        }],
        ["LwDisplayRandom", { //-test 304
         "UseSmallRaster" : true,
         "UseSimpleWindowSizes" : true,
         "SkipHeadMask" : 0x3, // Skipping to keep runtime under control
         "EnableDSCRefMode" : false,
         "TestConfiguration" : {
            "StartLoop" : 0,
            "Loops" : 1, // Number of frames
            "RestartSkipCount" : 3 // Number of frames per modeset
        }
        }]
    ]);
}
specDisp.specArgs = g_FastArgsWithDisp;

function specLwdaRandom(testList)
{
    testList.AddTests([
        ["LwdaRandom", { //-test 119
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 1, // one loop for each kernel
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : 1 },
            // skipping the first 2 subtests as they have longer times
            "FancyPickerConfig" : [ [ CRTST_CTRL_KERNEL, [ "list","wrap"
                ,[0, CRTST_CTRL_KERNEL_SeqMatrix         ]
                ,[0, CRTST_CTRL_KERNEL_SeqMatrixDbl      ]
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
            ] ] ]
        }]
    ]);
}
specLwdaRandom.specArgs = g_FastArgs + g_NoPmuArgs;

function specLwdaTTUStress(testList)
{
    testList.AddTests([
        ["LwdaTTUStress", {
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1
            },
            "InnerIterations": 2,
            "NumBlocks" : 4,
            "NumThreads" : 64,
            "BVHWidth": 2,
            "BVHDepth": 2,
            "NumTris": 7,
            "Verbose": true,
            "TestBVHMicromesh": true,
            "UseMotionBlur": true,
            "TriGroupSize": 4,
            "ClusterRays": true,
            "ShearComplets": true
        }]
    ]);
}
specLwdaTTUStress.specArgs = g_FastArgs + g_NoPmuArgs;

function specLwdaMatsRandom(testList)
{
    testList.AddTests([
        ["NewLwdaMatsRandom", { // -test 242
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "NumThreads" : 128
        }]
    ]);
}
specLwdaMatsRandom.specArgs = g_FastArgs + g_NoPmuArgs;

// For testing the new 256-bit wide loads/stores
function specLwdaMatsReadOnly(testList)
{
    testList.AddTests([
        ["NewLwdaMatsReadOnly", { // -test 241
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "NumThreads" : 128
        }]
    ]);
}
specLwdaMatsReadOnly.specArgs = g_FastArgs + g_NoPmuArgs;

function specXbar(testList)
{
    testList.AddTests([
        ["LwdaXbar2", { // -test 240
            "Iterations" : 1,
            "InnerIterations" : 1,
            "Mode" : 0x3
        }]
    ]);
}
specXbar.specArgs = g_FastArgs + g_NoPmuArgs;

function specCbu_0(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0xFF
        }]
    ]);
}
specCbu_0.specArgs = g_FastArgs + g_NoPmuArgs;

function specCbu_1(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0x3F00
        }]
    ]);
}
specCbu_1.specArgs = g_FastArgs + g_NoPmuArgs;

function specLinpack(testList)
{
    testList.AddTests([
        [ "CaskLinpackHgemm", {
            CheckLoops: 1,
            "MNKMode": 2,
            "Msize": 256,
            "Nsize": 128,
            "Ksize": 32,
            "NaiveInit": 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
    ]);
}
specLinpack.specArgs = g_FastArgs + g_NoPmuArgs;

function specLinpackFull(testList)
{
    testList.AddTests([
        [ "CaskLinpackHgemm", {
            CheckLoops: 1,
            "MNKMode": 2,
            "Msize": 256,
            "Nsize": 128,
            "Ksize": 32,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackHMMAgemm", {
            CheckLoops: 1,
            "MNKMode": 2,
            "Msize": 256,
            "Nsize": 128,
            "Ksize": 32,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackIMMAgemm", {
            CheckLoops: 1,
            "MNKMode": 2,
            "Msize": 256,
            "Nsize": 128,
            "Ksize": 32,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseHgemm", {
            CheckLoops: 1,
            "MNKMode": 2,
            "Msize": 256,
            "Nsize": 128,
            "Ksize": 32,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseHMMAgemm", {
            CheckLoops: 1,
            "MNKMode": 2,
            "Msize": 256,
            "Nsize": 128,
            "Ksize": 32,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseIMMAgemm", {
            CheckLoops: 1,
            "MNKMode": 2,
            "Msize": 256,
            "Nsize": 128,
            "Ksize": 32,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
    ]);
}
specLinpackFull.specArgs = g_FastArgs + g_NoPmuArgs;

function specGlStress(testList)
{
    testList.AddTests([
        ["GLStress", { //-test 2
            "LoopMs" : 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
    ]);
}
specGlStress.specArgs = g_FastArgs + g_NoPmuArgs;

function specGlrA8R8G8B8_0(testList)
{
    testList.AddTests([
        ["GlrA8R8G8B8", { //-test 130
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "Verbose" : true
                ,"RestartSkipCount" : 10
            },
            "Golden" : { "SkipCount" : 10 }
         }]
    ]);
}
specGlrA8R8G8B8_0.specArgs = g_FastArgs + g_NoPmuArgs + " -isolate_frames 14 14";

function specGlrR5G6B5_0(testList)
{
    testList.AddTests([
        ["GlrR5G6B5", { //-test 131
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "Verbose" : true
            }
         }]
    ]);
}
specGlrR5G6B5_0.specArgs = g_FastArgs + g_NoPmuArgs + " -isolate_frames 4 4";

function specGlrFsaa2x_0(testList)
{
    testList.AddTests([
        ["GlrFsaa2x", { //-test 132
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "Verbose" : true
            }
         }]
    ]);
}
specGlrFsaa2x_0.specArgs = g_FastArgs + g_NoPmuArgs + " -isolate_frames 31 31";

function specGlrFsaa4x_0(testList)
{
    testList.AddTests([
        ["GlrFsaa4x", { //-test 133
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "Verbose" : true
            }
         }]
    ]);
}
specGlrFsaa4x_0.specArgs = g_FastArgs + g_NoPmuArgs + " -isolate_frames 5 5";

function specGlrMrtRgbU_0(testList)
{
    testList.AddTests([
        ["GlrMrtRgbU", { //-test 135
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "Verbose" : true
                ,"RestartSkipCount" : 10
            },
            "Golden" : { "SkipCount" : 10 }
         }]
    ]);
}
specGlrMrtRgbU_0.specArgs = g_FastArgs + g_NoPmuArgs + " -isolate_frames 28 28";

function specGlrMrtRgbF_0(testList)
{
    testList.AddTests([
        ["GlrMrtRgbF", { //-test 136
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "Verbose" : true
            }
         }]
    ]);
}
specGlrMrtRgbF_0.specArgs = g_FastArgs + g_NoPmuArgs + " -isolate_frames 9 9";

function specInstInSys(testList)
{
    testList.AddTests([
        ["NewWfMatsCEOnly", { //-test 161
            MaxFbMb : 256/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
         }]
    ]);
}
specInstInSys.specArgs = g_FastArgs + " -inst_in_sys";

function specFbBroken(testList)
{
    testList.AddTests([
        ["Random2dNc", { //-test 9
            "TestConfiguration" : {"Loops" : 5},
            "Golden" : {"SkipCount" : 5}
        }]

        ,["CpyEngTest", { //-test 100
            "TestConfiguration" : {"Loops" : 1}
        }]
    ]);
}
specFbBroken.specArgs = g_FastArgs + " -inst_in_sys -fb_broken";

function specVkStress(testList)
{
    testList.AddTests([
        ["VkStress", { //-test 267
            "LoopMs" : 0,
            "FramesPerSubmit" : 4,
            "NumLights" : 1,
            "NumTextures" : 2,
            "PpV" : 4000,
            "TexReadsPerDraw": 1,
            "UseMipMapping": false,
            "TxD": -2.0,
            "ApplyFog": false,
            "TestConfiguration" : {
                "Loops" : 4,
                "Verbose" : true,
                "TimeoutMs": 2400000.0 // 40 minutes
            }
        }]
    ]);
}
specVkStress.specArgs = g_FastArgs + g_NoPmuArgs;

function specVkStressMesh(testList)
{
    testList.AddTests([
        ["VkStressMesh", { //-test 264
            "LoopMs" : 0,
            "FramesPerSubmit" : 4,
            "NumLights" : 1,
            "SampleCount" : 1,
            "NumTextures" : 2,
            "PpV" : 4000,
            "TestConfiguration" : {
                "Loops" : 4,
                "Verbose" : true,
                "TimeoutMs": 1800000.0 // 30 minutes
            }
        }]
    ]);
}
specVkStressMesh.specArgs = g_FastArgs + g_NoPmuArgs;

function specVideo(testList)
{
    testList.AddTests([
        ["LWDECTest", { //-test 277
            "DisableEncryption" : true,
            "SaveFrames" : true,
            "H264Streams" : [], //don't run any H264 streams
            "H265Streams" : [1], // use stream "in_to_tree-002.265"
            "EngineSkipMask" : 0x1c,
            "TestConfiguration" : {
                "Verbose" : true,
                "ShortenTestForSim" : true
            }
        }]
    ]);
}
specVideo.specArgs = "-chipargs -noinstance_lwenc -lwlink_force_disable";

function spelwnitTests(testList)
{
}
spelwnitTests.specArgs = g_FastArgs + " -test TestUnitTests -skip_unit_test SurfaceFillerTest";

function specGlrShort(testList)
{
    let skipCount = 1;
    let frames = 1;
    testList.AddTests([
        ["GlrA8R8G8B8", { //-test 130
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 13
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
    ]);

}
specGlrShort.specArgs = g_FastArgs + g_NoPmuArgs;

function specMme64(testList)
{
    testList.AddTests([
        ["MME64Random", { //-test 193
            "MacroSize" : 256,
            "SimOnly" : false,
            "SimTrace" : false,
            "DumpMacros" : false,
            "DumpSimEmissions" : false,
            "DumpMmeEmissions" : false,
            "NumMacrosPerLoop" : 12, // Turing MME Instr RAM holds 1024 groups
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
specMme64.specArgs = g_FastArgs;

//------------------------------------------------------------------------------
// Default spec if not passed on the command line
function userSpec(testList)
{
    let skipCount = 2;
    let frames = 1;
    let goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        goldenGeneration = true;
    }
    testList.AddTests([
        ["MatsTest", { //-test 3
                Coverage : 0.005
                }]
        ,["Random2dFb", { //-test 58
                "TestConfiguration" : {"Loops" : 10} ,
                "Golden" : {"SkipCount" : 5}
                }]

        ,["CpyEngTest", { //-test 100
                "TestConfiguration" : {"Loops" : 1}
                }]

        ,["NewWfMats", { //-test 94
            MaxFbMb : 128/1024,
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
         }]

        ,["NewLwdaMatsTest", { //-test 143
                MaxFbMb : 16/1024, Iterations : 1, NumThreads : 128
                }]

        // Keep GLStress & GLRandom back-back
        ,["GLStress", { //-test 2
                "LoopMs" : 0,
                "TestConfiguration" : {"Loops" : 1}
                }]

        ,["GlrA8R8G8B8", { //-test 130
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
    ]);
}
userSpec.specArgs = g_FastArgs;

// This spec is used to run all of the tests on Fmodel.
// Caution: it will take a long time to complete!
function fmodelSpec(testList)
{
    let skipCount = 5;
    let frames = 3;
    let goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        goldenGeneration = true;
    }
    testList.AddTests([
        ["MatsTest", { //-test 3
                Coverage : 0.005
                }]
        ,["FastMatsTest", { // -test 19
            "MaxFbMb" : 32, //
            "TestConfiguration" : { "Verbose" : true }
            }]
        ,["Random2dFb", { //-test 58
                "TestConfiguration" : {"Loops" : 10} ,
                "Golden" : {"SkipCount" : 5}
                }]
        ,["CpyEngTest", { //-test 100
                "TestConfiguration" : {"Loops" : 1}
                }]
        ,["NewWfMats", { //-test 94
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
         }]
        ,["WfMatsMedium", { //-test 118
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
         }]
        ,["NewWfMatsCEOnly", { //-test 161
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
         }]
        ,["NewWfMatsNarrow", { //-test 180
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
         }]
        ,["GLStress", { //-test 2
                "LoopMs" : 0,
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GLStressDots", { //-test 23
                "LoopMs" : 0,
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GLStressPulse", { //-test 92
                "LoopMs" : 0,
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GLPowerStress", { //-test 153
                "LoopMs" : 0,
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GLStressZ", { //-test 166
                "LoopMs" : 0,
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GlrZLwll", { //-test 162
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GLHistogram", { //-test 274
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GlrA8R8G8B8", { //-test 130
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrA8R8G8B8Sys", { //-test 148
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
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
                "StartLoop" : 0
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
                "StartLoop" : 0
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
                "StartLoop" : 0
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
                "StartLoop" : 0
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
                "StartLoop" : 0
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
                "StartLoop" : 0
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
                "StartLoop" : 0
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
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GLRandomCtxSw", { //-test 54
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
         ,["PathRender", { //-test 176
             "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
             }
          }]
        ,["FillRectangle", { //-test 286
          "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
               "Verbose" : true
          }
        }]
        ,["MsAAPR", { //-test 287
           "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
           }
        }]
        ,["GLPRTir", { //-test 289
            "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            }
         }]
         ,["MMERandomTest",{  //-test 150
            "TestConfiguration" : {
                "RestartSkipCount" : 10,
                "Loops" : 5*10,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : 10 }
         }]
        ,["LwdaRandom", { //-test 119
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 1, // 10 // one loop for each kernel
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
                      ,[1,  CRTST_CTRL_KERNEL_MatrixInit       ]
                      ,[1, CRTST_CTRL_KERNEL_Atom              ]
                      //,[1, CRTST_CTRL_KERNEL_GpuWorkCreation   ]
                      ] ] ]
        }]
        ,["LwdaStress2", { // test 198
            "LoopMs" : 1,
            "Iterations" : 2,
            "NumBlocks" : 1,
            "MinFbTraffic" : true,
            "TestConfiguration" : {
                "ShortenTestForSim" : true
            }
        }]
        ,["LwdaGrfTest", { //-test 89
            "TestConfiguration" : {
                "Loops" : 1,
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["DPStressTest", { //-test 190
            "LoopMs" : 1,
            "InnerLoops" : 1,
            "UseSharedMemory" : true,
            "TestConfiguration" : {
                "Loops" : 2, // must be an even number.
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["LwdaMatsTest", { //-test 87
            "NumThreads" : 32,
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["LwdaByteTest", {     //-test 111
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsTest", {  //-test 143
            "MaxFbMb" : 2,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsStress", { //-test 243
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "NumBlocks" : 1,
            "StressBlocks" : 1,
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackHgemm", {
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackHMMAgemm", {
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackIMMAgemm", {
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseHgemm", {
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseHMMAgemm", {
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseIMMAgemm", {
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,["LwdaRadixSortTest", {    //-test 185
            Size : 32000
        }]
    ]);
}
fmodelSpec.specArgs = g_FastArgs;
