/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "lwr_comm.h"

// DVS uses this file to regress Fmodel with the following single command
// that both generates goldens and runs the specified test list:
// ./mods -chip ghlit1_fmodel_64.so -gpubios 0 gh100_sim_hbm.rom gputest.js -verbose -readspec gh100fmod.spc -spec specNoTest //$
g_IsGenAndTest = true;
g_LoadGoldelwalues = false;
// For automated runs disable CSL, otherwise enable it for the block you suspect.
SetElw("CSL", "[disable]");
//SetElw("CSL", "smm_core"); // enable lwca debugging messages

SetElw("SC_COPYRIGHT_MESSAGE", "DISABLE"); // WAR bug 2687346

// Chiplib args for FMODEL
// - FBP, FBIO and LTC args should be set consistently with eachother
//  (Each FBP has 2 FBIOs and 2 LTCs associated with it)
// - Also, FBP L2 NOC pairing must be followed. For reference -
//   https://confluence.lwpu.com/display/SC/Floorsweeping+Args+-+Hopper#FloorsweepingArgs-Hopper-GH100FBPfloorsweepingnotes
g_SpecArgs  = " -chipargs -chipPOR=gh100";
g_SpecArgs += " -chipargs -texHandleErrorsLikeRTL"; // For textures in LwdaRandom
g_SpecArgs += " -chipargs -useVirtualization";
g_SpecArgs += " -chipargs -use_gen5";
g_SpecArgs += " -chipargs -noinstance_devices=gr1,gr2,gr3,gr4,gr5,gr6,gr7"; //$ no spaces allowed between ','
g_SpecArgs += " -chipargs -use_floorsweep";
g_SpecArgs += " -chipargs -num_ropltcs=4";
g_SpecArgs += " -chipargs -num_fbps=2";
g_SpecArgs += " -chipargs -num_fbio=4";
g_SpecArgs += " -chipargs -num_tpcs=3"; // Set to 3 to use all of the Gfx capable TPCs.
//g_SpecArgs += " -chipargs -fbpa_en_mask=0x00C3";
g_SpecArgs += " -chipargs -ltc_en_mask=0x00C3";
g_SpecArgs += " -chipargs -fbp_en_mask=0x09";

// MODS args
// Note: GPC:0, TPCs:0-2 are the only graphics capable TPCs, so GPC:0 can't be floorswept.
g_SpecArgs += " -floorsweep gpc_enable:0x1:gpc_tpc_enable:0:0x7:fbp_enable:0x9:gpc_cpc_enable:0:0x1 -adjust_fs_args"; //$
g_SpecArgs += " -floorsweep c2c_disable:0xff"; // WAR bug 200760531
g_SpecArgs += " -devid_check_ignore";
g_SpecArgs += " -skip_pertest_pexcheck";
g_SpecArgs += " -run_on_error";
g_SpecArgs += " -maxwh 160 120";
g_SpecArgs += " -skip_pex_checks"; // skip all pex checks on FModel

// Required FModel args:
g_SpecArgs += " -chipargs -sriov_BDFShiftSizeInSysmemAddr=56";
g_SpecArgs += " -chipargs -gpc_gfx_en_mask=0x1";
g_SpecArgs += " -chipargs -gpc_rop_en_mask=0x1";
g_SpecArgs += " -chipargs -zlwll_bank=4";
g_SpecArgs += " -chipargs -use_gen5";
g_SpecArgs += " -chipargs -forceSplinterHWOpextForSPAVersion=1" // For SM90 merc finalizer on fmodel
g_SpecArgs += " -lwlink_force_optimization_algorithm 0x1";
g_SpecArgs += " -timeout_ms 2400000.0";

// Args to help speed things up
// GH100 does not have any display heads.
let g_FastArgs = "";
g_FastArgs += " -chipargs -noinstance_lwdec";
g_FastArgs += " -chipargs -noinstance_lwenc";
g_FastArgs += " -chipargs -noinstance_lwjpg";
g_FastArgs += " -chipargs -noinstance_ofa";
g_FastArgs += " -lwlink_force_disable";

// We can save several minutes from FMODEL runs by avoiding initializing or
// using the PMU, but some tests require it and we should be regressing it.
let g_NoPmuArgs = " -exelwte_rm_devinit_on_pmu 0 -pmu_bootstrap_mode 1";

let g_GH100SpecArgs = " -chiplib_deepbind"; // Bug 2424418

// Failed PMU init.
function specNoTest(testList)
{
}
specNoTest.specArgs = g_FastArgs + g_GH100SpecArgs + " -no_test";

// Passed
function specNoTestFast(testList)
{
}
specNoTestFast.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs + " -no_test";

// Passed MODS end  : Thu Jan 14 09:07:49 2021  [624.571 seconds (00:10:24.571 h:m:s)]
function specBasic(testList)
{
    testList.AddTests([
        ["Random2dFb",
        { //-test 58
            "TestConfiguration" : { "Loops" : 5 },
            "Golden" : { "SkipCount" : 5 }
        }]

        ,["CpyEngTest",
        { //-test 100
            "TestConfiguration" : { "Loops" : 1 }
        }]
    ]);
}
specBasic.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

function specSriovBasic(testList)
{
    testList.AddTests([
        ["Random2dFb",
        { //-test 58
            "TestConfiguration" : { "Loops" : 5 },
            "Golden" : { "SkipCount" : 5 }
        }]
    ])
}
// -chiplib_deepbind not supported with SR-IOV
specSriovBasic.specArgs = g_FastArgs + g_NoPmuArgs;

//MODS end  : Thu Feb 11 11:43:37 2021  [983.955 seconds (00:16:23.955 h:m:s)]
function specLwdaRandom(testList)
{
    testList.AddTests([
        ["LwdaRandom",
        { //-test 119
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 11, // set by ShortenTestForSim
                "StartLoop" : 0,
                "Verbose" : true
            },
            "SurfSizeOverride" : 16*16*8, // 1 BLOCK large enough to for double precision instrs.
            "Golden" : { "SkipCount" : 1 },
            "FancyPickerConfig" : [ [ CRTST_CTRL_KERNEL, [ "list", "wrap"
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
                //,[1, CRTST_CTRL_KERNEL_GpuWorkCreation   ] // failed see below
            ] ] ]
        }]
    ]);
}
specLwdaRandom.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

// failed see bug TDB
function specLwdaRandomWorkCreation(testList)
{
    testList.AddTests([
        ["LwdaRandom",
        { //-test 119
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 1, // set by ShortenTestForSim
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : 1 },
            "FancyPickerConfig" : [ [ CRTST_CTRL_KERNEL, [ "list", "wrap"
                ,[1, CRTST_CTRL_KERNEL_GpuWorkCreation   ]
            ] ] ]
        }]
    ]);
}
specLwdaRandomWorkCreation.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;


//Passed, MODS end  : Thu Jan 14 10:25:51 2021  [916.562 seconds (00:15:16.562 h:m:s)]
function specLwdaMatsRandom(testList)
{
    testList.AddTests([
        ["NewLwdaMatsRandom",
        { // -test 242
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "NumThreads" : 128
        }]
    ]);
}
specLwdaMatsRandom.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

//Passed, MODS end  : Thu Jan 14 13:01:09 2021  [2992.638 seconds (00:49:52.638 h:m:s)]
function specXbar(testList)
{
    testList.AddTests([
        ["LwdaXbar2",
        { // -test 240
            "Iterations" : 1,
            "InnerIterations" : 1,
            "Mode" : 0x3
        }]
    ]);
}
specXbar.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

// failed, see http://lwbugs/3230652
function specCbu_0(testList)
{
    testList.AddTests([
        ["LwdaCbu",
        { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0xFF
        }]
    ]);
}
specCbu_0.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

// not tested, see http://lwbugs/3230652
function specCbu_1(testList)
{
    testList.AddTests([
        ["LwdaCbu",
        { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0x3F00
        }]
    ]);
}
specCbu_1.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

function specLinpack(testList)
{
    testList.AddTests([
        ["CaskLinpackSgemm",
        { //-test 610
            "Msize" : 128,
            "Nsize" : 128,
            "Ksize" : 64,
            "MNKMode" : 2,
            "ReportFailingSmids" : false,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
   ]);
}
specLinpack.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

//Passed MODS end  : Fri Jan 15 09:18:43 2021  [912.072 seconds (00:15:12.072 h:m:s)]
function specGlStress(testList)
{
    testList.AddTests([
        ["GLStress",
        { //-test 2
            "LoopMs" : 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
    ]);
}
specGlStress.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

// Passed, MODS end  : Fri Jan 15 19:59:41 2021  [17910.697 seconds (04:58:30.697 h:m:s)]
function specGlrA8R8G8B8_0(testList)
{
    testList.AddTests([
        ["GlrA8R8G8B8",
        { //-test 130
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "Verbose" : true
                ,"RestartSkipCount" : 10
            },
            "Golden" : { "SkipCount" : 10 }
        }]
    ]);
}
specGlrA8R8G8B8_0.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs + " -isolate_frames 14 14";

function specGlrR5G6B5_0(testList)
{
    testList.AddTests([
        ["GlrR5G6B5",
        { //-test 131
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : { "Verbose" : true }
        }]
    ]);
}
specGlrR5G6B5_0.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs + " -isolate_frames 4 4";

function specGlrFsaa2x_0(testList)
{
    testList.AddTests([
        ["GlrFsaa2x",
        { //-test 132
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : { "Verbose" : true }
        }]
    ]);
}
specGlrFsaa2x_0.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs + " -isolate_frames 31 31";

function specGlrFsaa4x_0(testList)
{
    testList.AddTests([
        ["GlrFsaa4x",
        { //-test 133
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "Verbose" : true
            }
        }]
    ]);
}
specGlrFsaa4x_0.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs + " -isolate_frames 5 5";

function specGlrMrtRgbU_0(testList)
{
    testList.AddTests([
        ["GlrMrtRgbU",
        { //-test 135
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "Verbose" : true
                ,"RestartSkipCount" : 10
            },
            "Golden" : { "SkipCount" : 10 }
        }]
    ]);
}
specGlrMrtRgbU_0.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs + " -isolate_frames 28 28";

function specGlrMrtRgbF_0(testList)
{
    testList.AddTests([
        ["GlrMrtRgbF",
        { //-test 136
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "Verbose" : true
            }
        }]
    ]);
}
specGlrMrtRgbF_0.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs + " -isolate_frames 9 9";

function specInstInSys(testList)
{
    testList.AddTests([
        ["NewWfMatsCEOnly",
        { //-test 161
            MaxFbMb : 512/1024,
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
        }]
    ]);
}
specInstInSys.specArgs = g_FastArgs + g_GH100SpecArgs + " -inst_in_sys";

function specFbBroken(testList)
{
    testList.AddTests([
        ["Random2dNc",
        { //-test 9
            "TestConfiguration" : { "Loops" : 5 },
            "Golden" : { "SkipCount" : 5 }
        }]
        ,["CpyEngTest",
        { //-test 100
            "TestConfiguration" : { "Loops" : 1 }
        }]
    ]);
}
specFbBroken.specArgs = g_FastArgs + g_GH100SpecArgs + " -inst_in_sys -fb_broken";

//Failed, LWRM lwAssertFailed: Assertion failed: 0 @ memory_manager.h:767
function specVkStress(testList)
{
    testList.AddTests([
        ["VkStress",
        { //-test 267
            "LoopMs" : 0,
            "FramesPerSubmit" : 4,
            "NumLights" : 1,
            "NumTextures" : 2,
            "PpV" : 4000,
            "TexReadsPerDraw": 1,
            "UseMipMapping": false,
            "TxD": -2.0,
            "ApplyFog": false,
            "TestConfiguration" :
            {
                "Loops" : 4,
                "Verbose" : true,
                "TimeoutMs": 2400000.0, // 40 minutes
                "DisplayWidth" : 160,
                "DisplayHeight" : 120
            }
        }]
    ]);
}
specVkStress.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

function specVkStressMesh(testList)
{
    testList.AddTests([
        ["VkStressMesh",
        { //-test 264
            "LoopMs" : 0,
            "FramesPerSubmit" : 4,
            "NumLights" : 1,
            "SampleCount" : 1,
            "NumTextures" : 2,
            "PpV" : 4000,
            "TestConfiguration" :
            {
                "Loops" : 4,
                "Verbose" : true,
                "TimeoutMs": 1800000.0 // 30 minutes
            }
        }]
    ]);
}
specVkStressMesh.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

// Failed, Test only supports upto LWC4B0_VIDEO_DECODER and needs to be updated to support
// LWB8B0_VIDEO_DECODER
function specVideo(testList)
{
    testList.AddTests([
        ["LWDECTest",
        { //-test 277
            "DisableEncryption" : true,
            "SaveFrames" : true,
            "H264Streams" : [], //don't run any H264 streams
            "H265Streams" : [1], // use stream "in_to_tree-002.265"
            "EngineSkipMask" : 0x1c,
            "TestConfiguration" :
            {
                "Verbose" : true,
                "ShortenTestForSim" : true
            }
        }]
    ]);
}
specVideo.specArgs = g_GH100SpecArgs + "-chipargs -noinstance_lwenc -lwlink_force_disable";

function spelwnitTests(testList)
{
}
spelwnitTests.specArgs = g_FastArgs + g_GH100SpecArgs + " -test TestUnitTests -skip_unit_test SurfaceFillerTest";

//Passed MODS end  : Fri Jan 15 11:59:58 2021  [1380.361 seconds (00:23:00.361 h:m:s)]
function specGlrShort(testList)
{
    let skipCount = 1;
    let frames = 1;
    testList.AddTests([
        ["GlrA8R8G8B8",
        { //-test 130
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 13
            },
            "Golden" : { "SkipCount" : skipCount }
        }]
    ]);

}
specGlrShort.specArgs = g_FastArgs + g_NoPmuArgs + g_GH100SpecArgs;

//------------------------------------------------------------------------------
// Default spec if not passed on the command line
function userSpec(testList)
{
    let skipCount = 2;
    let frames = 1;
    let goldenGeneration = false;
    if (Golden.Store === Golden.Action)
    {
        goldenGeneration = true;
    }
    testList.AddTests([
        ["MatsTest",
        { //-test 3
            Coverage : 0.005
        }]
        ,["Random2dFb",
        { //-test 58
            "TestConfiguration" : { "Loops" : 10 },
            "Golden" : { "SkipCount" : 5 }
        }]

        ,["CpyEngTest",
        { //-test 100
            "TestConfiguration" : { "Loops" : 1 }
        }]

        ,["NewWfMats",
        { //-test 94
            MaxFbMb : 128/1024,
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
        }]

        ,["NewLwdaMatsTest",
        { //-test 143
            MaxFbMb : 16/1024, Iterations : 1, NumThreads : 128
        }]

        // Keep GLStress & GLRandom back-back
        ,["GLStress",
        { //-test 2
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 1 }
        }]

        ,["GlrA8R8G8B8",
        { //-test 130
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
    ]);
}
userSpec.specArgs = g_FastArgs + g_GH100SpecArgs;

// This spec is used to run all of the tests on Fmodel.
// Caution: it will take a long time to complete!
function fmodelSpec(testList)
{
    let skipCount = 5;
    let frames = 3;
    let goldenGeneration = false;
    if (Golden.Store === Golden.Action)
    {
        goldenGeneration = true;
    }
    testList.AddTests([
        ["MatsTest",
        { //-test 3
            Coverage : 0.005
        }]
        ,["FastMatsTest",
        { // -test 19
            "MaxFbMb" : 32, //
            "TestConfiguration" : { "Verbose" : true }
        }]
        ,["Random2dFb",
        { //-test 58
            "TestConfiguration" : { "Loops" : 10 },
            "Golden" : { "SkipCount" : 5 }
        }]
        ,["CpyEngTest",
        { //-test 100
            "TestConfiguration" : { "Loops" : 1 }
        }]
        ,["NewWfMats",
        { //-test 94
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
        }]
        ,["WfMatsMedium",
        { //-test 118
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
        }]
        ,["NewWfMatsCEOnly",
        { //-test 161
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
        }]
        ,["NewWfMatsNarrow",
        { //-test 180
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
        }]
        ,["GLStress",
        { //-test 2
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 20 }
        }]
        ,["GLStressDots",
        { //-test 23
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 20 }
        }]
        ,["GLStressPulse",
        { //-test 92
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 20 }
        }]
        ,["GLPowerStress",
        { //-test 153
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 20 }
        }]
        ,["GLStressZ",
        { //-test 166
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 20 }
        }]
        ,["GlrZLwll",
        { //-test 162
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 20 }
        }]
        ,["GLHistogram",
        { //-test 274
            "LoopMs" : 0,
            "TestConfiguration" : { "Loops" : 20 }
        }]
        ,["GlrA8R8G8B8",
        { //-test 130
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrA8R8G8B8Sys",
        { //-test 148
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrR5G6B5",
        { //-test 131
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrFsaa2x",
        { //-test 132
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrFsaa4x",
        { //-test 133
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrMrtRgbU",
        { //-test 135
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrMrtRgbF",
        { //-test 136
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrY8",
        { //-test 137
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrFsaa8x",
        { //-test 138
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GlrLayered",
        { //-test 231
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["GLRandomCtxSw",
         { //-test 54
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
         ,["PathRender",
         { //-test 176
             "TestConfiguration" :
             {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
             }
          }]
        ,["FillRectangle",
        { //-test 286
          "TestConfiguration" :
          {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
               "Verbose" : true
          }
        }]
        ,["MsAAPR",
        { //-test 287
           "TestConfiguration" :
           {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
           }
        }]
        ,["GLPRTir",
        { //-test 289
            "TestConfiguration" :
            {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0,
                "Verbose" : true
            }
         }]
         ,["MMERandomTest",
         {  //-test 150
            "TestConfiguration" :
            {
                "RestartSkipCount" : 10,
                "Loops" : 5*10,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : 10 }
         }]
        ,["LwdaRandom",
        { //-test 119
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 1, // 10 // one loop for each kernel
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : 1 },
            "FancyPickerConfig" : [ [ CRTST_CTRL_KERNEL, [ "list", "wrap"
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
                      ,[1, CRTST_CTRL_KERNEL_GpuWorkCreation   ]
                      ] ] ]
        }]
        ,["LwdaStress2",
        { // test 198
            "LoopMs" : 1,
            "Iterations" : 2,
            "NumBlocks" : 1,
            "MinFbTraffic" : true,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true
            }
        }]
        ,["LwdaGrfTest",
        { //-test 89
            "TestConfiguration" :
            {
                "Loops" : 1,
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["DPStressTest",
        { //-test 190
            "LoopMs" : 1,
            "InnerLoops" : 1,
            "UseSharedMemory" : true,
            "TestConfiguration" :
            {
                "Loops" : 2, // must be an even number.
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["LwdaMatsTest",
        { //-test 87
            "NumThreads" : 32,
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["LwdaByteTest",
        {     //-test 111
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsTest",
        {  //-test 143
            "MaxFbMb" : 2,
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsStress",
        { //-test 243
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "NumBlocks" : 1,
            "TestConfiguration" :
            {
                "Verbose" : true
            },
            bgTestArg :
            {
                "Msize"   : 128,
                "Ksize"   : 8,
                "MNKMode" : 2,
                "TestConfiguration" :
                {
                    "Loops" : 1
                }
            }
        }]
        ,[ "CaskLinpackHgemm",
        { // -test 606
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackHMMAgemm",
        {
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackIMMAgemm",
        { // -test 612
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseHgemm",
        { // -test 607
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseHMMAgemm",
        { // -test 611
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "CaskLinpackPulseIMMAgemm",
        {   //-test 613
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,["LwdaRadixSortTest",
        {    //-test 185
            Size : 32000
        }]
        ,["LwdaMmioYield",
        {   //-test 221
            "TestConfiguration" :
            {
                "ShortenTestForSim" : true,
                "Loops" : 1,
                "Verbose" : true
            },
            TestMode : 0x1
        }]
        ,["LwdaInstrPulse",
        {   //-test 318
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            },
            "RuntimeMs":0
        }]
        ,["LwdaColumnTest",
        {   //-test 127
            "MaxFbMb" : 16/1024,
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            },
            "Iterations" : 1,
            "ColumnToTest" : 1,
            "ColumnSleep" : 0,
            "Direction" : "ascending"
        }]
        ,["LwdaLinpackCombi",
        { //-test 330
            "TestConfiguration" :
            {
                "Loops" : 1,
                "Verbose" : true
            },
            "RuntimeMs":0,
            bgTestArg :
            {
                "Msize":128,
                "Ksize":8,
                "MNKMode":2,
                "TestConfiguration" :
                {
                    "Loops" : 1
                }
            },
            fgTestArg :
            {
                "Msize":128,
                "Ksize":16,
                "MNKMode":2,
                "TestConfiguration" :
                {
                    "Loops" : 1
                }
            }
        }]
    ]);
}
fmodelSpec.specArgs = g_FastArgs + g_GH100SpecArgs;
