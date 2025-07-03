/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "lwr_comm.h"

AddFileId(Out.PriNormal, "$Id: $");

// For automated runs disable CSL, otherwise enable it for the block you suspect.
SetElw("CSL","[disable]");
//SetElw("CSL","smm_core"); // enable lwca debugging messages

// -chipargs -texHandleErrorsLikeRTL prevents asserts in LwdaRandom's texture
// kernel with the tld4 asm instruction.
g_SpecArgs  = "-chipargs -chipPOR=ga100 -chipargs -texHandleErrorsLikeRTL -exelwte_rm_devinit_on_pmu 0";
g_SpecArgs += " -chipargs -useVirtualization -chipargs -noinstance_devices=gr1,gr2,gr3,gr4,gr5,gr6,gr7";
g_SpecArgs += " -chipargs -use_floorsweep -chipargs -num_ropltcs=4 -chipargs -num_fbps=2 -chipargs -num_fbio=4 -chipargs -num_tpcs=2";
g_SpecArgs += " -chipargs -fbpa_en_mask=0x00030C -chipargs -ltc_en_mask=0x00030C -chipargs -fbp_en_mask=0x012";
g_SpecArgs += " -floorsweep gpc_enable:0x1:gpc_tpc_enable:0:0x3:hbm_enable:0x2 -adjust_fs_args";
g_SpecArgs += " -devid_check_ignore -skip_pertest_pexcheck -maxwh 160 120 -run_on_error";

g_SpecArgs += " -disable_acr -sec2_enable_rtos 0 "; // Bug 2335793
g_IsGenAndTest = true;
g_LoadGoldelwalues = false;

let g_DefaultSpecArgs = " -chipargs -noinstance_lwdec -chipargs -noinstance_lwenc -lwlink_force_disable ";

let g_GA100SpecArgs = " -chiplib_deepbind ";  // Bug 2424418

let g_LR10SpecArgs = " -chipargs -noinstance_lwdec -chipargs -noinstance_lwenc -multichip_paths galit1_fmodel_64.so:slit0_fmodel_64.so ";
g_LR10SpecArgs    += " -chipargs -ioctrl_config_file_name=lrsimple.ini -lwlink_topology_file lrsimple.topo -lwlink_topology_match_file -lwlink_auto_link_config -chipargs -sli_chipargs0=-ioctrl_gpu_num=0 ";
g_LR10SpecArgs    += " -chipargs -lwlink0=PEER0 -chipargs -lwlink_sockets -chipargs -lwlink_token_discovery -rmkey RMLwLinkSwitchLinks 0x1 -lwswitchkey ChiplibForcedLinkConfigMask 0x30000 -lwswitchkey ChiplibForcedLinkConfigMask2 0x0 ";
g_LR10SpecArgs    += " -lwswitchkey SoeDisable 0x1 ";

// dvs uses this file to regress Fmodel. It does ilwocations of mods using the
// following command lines:
//mods -a -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom  gpugen.js -readspec ga100fmod.spc -spec pvsSpecStandard
//mods -a -chip galit1_fmodel_64.so -gpubios 0 ga100_sim_hbm.rom gputest.js -readspec ga100fmod.spc -spec pvsSpecStandard


//------------------------------------------------------------------------------
// List used for testing special args like "-inst_in_sys"
function dvsSpecNoTest(testList)
{
}
dvsSpecNoTest.specArgs = g_GA100SpecArgs + g_DefaultSpecArgs + "-pmu_bootstrap_mode 1 -no_test";

//-----------------------------------------------------------------------------
function dvsSpecLRNoTest(testList)
{
}
dvsSpecLRNoTest.specArgs = g_LR10SpecArgs + "-no_test";

//-----------------------------------------------------------------------------
function dvsSpecLRLwLinkBwStress(testList)
{
    testList.AddTest("LwLinkBwStress", {"SkipBandwidthCheck":true,
                                        "TestConfiguration":{"ShortenTestForSim":true,
                                                             "SurfaceWidth":32,
                                                             "SurfaceHeight":32,
                                                             "Loops":2}});
}
dvsSpecLRLwLinkBwStress.specArgs = g_LR10SpecArgs;

//------------------------------------------------------------------------------
// List used for testing special args like "-inst_in_sys"
function dvsSpecSpecial(testList)
{
}
dvsSpecSpecial.specArgs = g_GA100SpecArgs +  "-inst_in_sys -no_test";

// A queue of tests that's run on MODS' PVS test machines
function pvsSpecStandard(testList)
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
pvsSpecStandard.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-pmu_bootstrap_mode 1";
 
// A queue of tests that's run on MODS' PVS test machines
function pvsSpecLwdaRandom(testList)
{
    testList.AddTests([
        ["LwdaRandom", { //-test 119
            "TestConfiguration" : {
                UseOldRNG: true,
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 1, // one loop for each kernel
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
            ] ] ]
        }]
    ]);
}
pvsSpecLwdaRandom.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecLwdaMatsRandom(testList)
{
    testList.AddTests([
        ["NewLwdaMatsRandom", { // -test 242
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "NumThreads" : 128
        }]
    ]);
}
pvsSpecLwdaMatsRandom.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

// A queue of tests that's run on MODS' PVS test machines
function pvsSpecXbar(testList)
{
    testList.AddTests([
        ["LwdaXbar2", { // -test 240
            "Iterations" : 1,
            "InnerIterations" : 1,
            "Mode" : 0x3
        }]
    ]);
}
pvsSpecXbar.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecCbu_0(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0xFF
        }]
    ]);
}
pvsSpecCbu_0.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecCbu_1(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0x3F00
        }]
    ]);
}
pvsSpecCbu_1.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecLinpack(testList)
{
    testList.AddTests([
        ["LwdaLinpackHgemm", { //-test 210
            "Msize" : 256,
            "Nsize" : 128,
            "Ksize" : 8,
            "MNKMode" : 2,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]

    ]);
}
pvsSpecLinpack.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecGlStress(testList)
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
pvsSpecGlStress.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecGlrA8R8G8B8_0(testList)
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
pvsSpecGlrA8R8G8B8_0.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-isolate_frames 14 14";

function pvsSpecGlrR5G6B5_0(testList)
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
pvsSpecGlrR5G6B5_0.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-isolate_frames 4 4";

function pvsSpecGlrFsaa2x_0(testList)
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
pvsSpecGlrFsaa2x_0.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-isolate_frames 31 31";

function pvsSpecGlrFsaa4x_0(testList)
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
pvsSpecGlrFsaa4x_0.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-isolate_frames 5 5";

function pvsSpecGlrMrtRgbU_0(testList)
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
pvsSpecGlrMrtRgbU_0.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-isolate_frames 28 28";

function pvsSpecGlrMrtRgbF_0(testList)
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
pvsSpecGlrMrtRgbF_0.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-isolate_frames 9 9";

function pvsSpecInstInSys(testList)
{
    testList.AddTests([
        ["NewWfMatsCEOnly", { //-test 161
            MaxFbMb : 512/1024, //
            BoxHeight: 2,
            PatternSet : 1,
            VerifyChannelSwizzle : true
         }]
    ]);
}
pvsSpecInstInSys.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-inst_in_sys";

function pvsSpecFbBroken(testList)
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
pvsSpecFbBroken.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-inst_in_sys -fb_broken -pmu_bootstrap_mode 1";

function pvsSpecVkStress(testList)
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
pvsSpecVkStress.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecVkStressMesh(testList)
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
pvsSpecVkStressMesh.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecVideo(testList)
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
pvsSpecVideo.specArgs = g_GA100SpecArgs +  "-chipargs -noinstance_lwenc -lwlink_force_disable";

function pvsSpelwnitTests(testList)
{
}
pvsSpelwnitTests.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-test TestUnitTests -skip_unit_test SurfaceFillerTest -pmu_bootstrap_mode 1";

function dvsSpecGlrShort(testList)
{
    var skipCount = 1;
    var frames = 1;
    testList.AddTests([
        ["GlrA8R8G8B8", { //-test 130
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
dvsSpecGlrShort.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

function pvsSpecMme64(testList)
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
pvsSpecMme64.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs + "-pmu_bootstrap_mode 1";

//------------------------------------------------------------------------------
// Default spec if not passed on the command line
function userSpec(testList)
{
    var skipCount = 2;
    var frames = 1;
    var goldenGeneration = false;
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
userSpec.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;

//------------------------------------------------------------------------------
// Use this spec is used to run all of the tests on Fmodel.
// Caution it will take a long time to complete!
function fmodelSpec(testList)
{
    var skipCount = 5;
    var frames = 3;
    var goldenGeneration = false;
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
                "LoopMs" : 0,
                "TestConfiguration" : {"Loops" : 20}
                }]
        ,["GLHistogram", { //-test 274
                "LoopMs" : 0,
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
        ,[ "LwdaLinpackDgemm", { //-test 199
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackSgemm", { //-test 200
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackHgemm", { //-test 210
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackHMMAgemm", { //-test 310
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackIgemm", { //-test 212
            Ksize: 8,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackIMMAgemm", { //-test 312
            Ksize: 32,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseDP", { //-test 292
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseSP", { //-test 296
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseHP", { //-test 211
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseHMMA", { //-test 311
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseIP", { //-test 213
            Ksize: 128,
            CheckLoops: 1,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseIMMA", { //-test 311
            Ksize: 128,
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
fmodelSpec.specArgs = g_GA100SpecArgs +  g_DefaultSpecArgs;
