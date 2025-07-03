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

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/tu10xfmod.spc#7 $");

// Add this if you want to see what display engine outputs (in saurat format)
//-display_dump_images

// For automated runs disable CLS, otherwise enable it for the block you suspect.
SetElw("CSL","[disable]");
//SetElw("CSL","smm_core"); // enable lwca debugging messages

// -chipargs -texHandleErrorsLikeRTL prevents asserts in LwdaRandom's texture
// kernel with the tld4 asm instruction.
g_SpecArgs = "-devid_check_ignore -skip_pertest_pexcheck -chipargs -texHandleErrorsLikeRTL -chipargs -use_floorsweep -chipargs -num_gpcs=1 -chipargs -num_tpcs=2 -chipargs -num_fbps=1 -chipargs -num_ropltcs=2 -floorsweep gpc_enable:0x1:gpc_tpc_enable:0:0x3 -floorsweep fbp_rop_l2_enable:0:0x3:fbp_enable:0x1:fbio_enable:0x1:fbpa_enable:0x1 -maxwh 160 120";

// dvs uses this file to regress Fmodel. It does ilwocations of mods using the
// following command lines:
//mods -a -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom  gpugen.js -chipargs -chipPOR=tu102 -readspec tu10xsim.spc -spec dvsSpecNoDisplay -null_display
//mods -a -chip tulit2_fmodel_64.so -gpubios 0 tu102_sim_gddr6.rom gputest.js -chipargs -chipPOR=tu102 -readspec tu10xsim.spc -spec dvsSpecNoDisplay -null_display

//------------------------------------------------------------------------------
// Short list of tests to regress in dvs.
// This list requires a real display to run/
// Note: DVS only runs this list with gpugen.js not gputest.js.
function dvsSpecWithDisplay(testList)
{
    testList.AddTests([
        ["Random2dFb", { "TestConfiguration" : {"Loops" : 5} , "Golden" : {"SkipCount" : 5} }]
        ,"LwDisplayHtol"
        ,["LwDisplayRandom", {
			"UseSmallRaster" : true,
			"UseSimpleWindowSizes" : true,
			"SkipHeadMask" : 0x1,
			"EnableDSCRefMode" : false,
			"TestConfiguration" : {
				"StartLoop" : 0,
				"Loops" : 3, // Number of frames
				"RestartSkipCount" : 3 // Number of frames per modeset
			}
        }]
        ,["LwDisplayDPC", {
			"DPCFilePath" : "600x40_2Head1Or_DSC_fmodel.dpc"
        }]
    ]);
}
dvsSpecWithDisplay.specArgs = "-simulate_dfp TMDS_128x112 -use_mods_console";

//------------------------------------------------------------------------------
// List used for testing special args like "-inst_in_sys"
function dvsSpecSpecial(testList)
{
}
dvsSpecSpecial.specArgs = "-inst_in_sys";

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

// A queue of tests that's run on MODS' PVS test machines

function pvsSpecLwdaRandom(testList)
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

function pvsSpecLwdaMatsRandom(testList)
{
    testList.AddTests([
        ["NewLwdaMatsRandom", { // -test 242
            "MaxFbMb" : 2,
            "NumThreads" : 128,
            "Iterations" : 1
        }]
    ]);
}

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

function pvsSpecCbu_0(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0xFF
        }]
    ]);
}

function pvsSpecCbu_1(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0x3F00
        }]
    ]);
}

function pvsSpecTTU(testList)
{
    testList.AddTests([
        ["LwdaTTUStress", { // -test 340
            "InnerIterations" : 4,
            "NumBlocks" : 8,
            "NumThreads" : 64,
            "OctDepth" : 1,
            "RuntimeMs" : 0,
            "TestConfiguration" : {
                "Verbose" : true,
                "Loops"   : 2
            }
        }]
    ]);
}

function pvsSpecLinpack(testList)
{
    testList.AddTests([
        ["LwdaLinpackHgemm", { //-test 210
            "Ksize" : 8,
            "RuntimeMs": 0,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]

    ]);
}

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
pvsSpecGlrA8R8G8B8_0.specArgs = "-isolate 14 14";

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
pvsSpecGlrR5G6B5_0.specArgs = "-isolate 4 4";

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
pvsSpecGlrFsaa2x_0.specArgs = "-isolate 31 31";

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
pvsSpecGlrFsaa4x_0.specArgs = "-isolate 5 5";

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
pvsSpecGlrMrtRgbU_0.specArgs = "-isolate 28 28";

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
pvsSpecGlrMrtRgbF_0.specArgs = "-isolate 9 9";

function pvsSpecInstInSys(testList)
{
    // Set a custom pattern (Pattern 32) to reduce runtime
    PatternClass.AddUserPattern([0x0]);

    testList.AddTests([
        ["FastMatsTest", { //-test 19
            "Pattern" : 32,
            "MaxFbMb" : 2,
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
    ]);
}
pvsSpecInstInSys.specArgs = "-inst_in_sys";

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
pvsSpecFbBroken.specArgs = "-inst_in_sys -fb_broken";

function pvsSpecVkStress(testList)
{
    testList.AddTests([
        ["VkStress", { //-test 267
            "LoopMs" : 0,
            "FramesPerSubmit" : 4,
            "NumLights" : 1,
            "SampleCount" : 2,
            "NumTextures" : 2,
            "PpV" : 4000,
            "TestConfiguration" : {
                "Loops" : 4,
                "Verbose" : true,

                // We need a 30 minute because this task takes ~25 minutes for
                // gpugen and gputest each.
                "TimeoutMs": 1800000.0 // 30 minutes
            }
        }]
    ]);
}

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

function pvsSpecVideo(testList)
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
    ]);
}

function pvsSpelwnitTests(testList)
{
}
pvsSpelwnitTests.specArgs = "-test TestUnitTests -skip_unit_test SurfaceFillerTest";

function dvsSpecNoDisplayB(testList)
{
    var skipCount = 1;
    var frames = 1;
    var goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        goldenGeneration = true;
    }
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
                "Loops" : 24,
                "Verbose" : true
            }
        }]
    ]);
}

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
            PatternSet : [0],
            VerifyChannelSwizzle : true
         }]

        ,["NewLwdaMatsTest", { //-test 143
                MaxFbMb : 16/1024, Iterations : 1
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
            PatternSet : [0],
            VerifyChannelSwizzle : true
         }]
        ,["WfMatsMedium", { //-test 118
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true
         }]
        ,["NewWfMatsCEOnly", { //-test 161
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true
         }]
        ,["NewWfMatsNarrow", { //-test 180
            MaxFbMb : 128/1024, //
            BoxHeight: 2,
            PatternSet : [0],
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
