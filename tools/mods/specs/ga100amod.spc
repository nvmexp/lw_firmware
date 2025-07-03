/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/ga100amod.spc#15 $");

SetElw("OGL_ChipName","ga100");

g_SpecArgs  = "-maxwh 160 120 -run_on_error"

g_IsGenAndTest = true;
g_LoadGoldelwalues = false;

let g_DefaultSpecArgs = "";

// dvs uses this file to regress Amodel. It does ilwocations of mods using the
// following command lines:
//mods -a gpugen.js  -readspec ga100amod.spc -spec pvsSpecStandard
//mods -a gputest.js -readspec ga100amod.spc -spec pvsSpecStandard


//------------------------------------------------------------------------------
function dvsSpecNoTest(testList)
{
}
dvsSpecNoTest.specArgs = g_DefaultSpecArgs + "-no_test";

// A queue of tests that's run on MODS' PVS test machines
function pvsSpecLwdaRandom(testList)
{
    testList.AddTests([
        ["LwdaRandom", {"TestConfiguration" : {"Loops" : 30*75, "StartLoop" : 0}}]
    ]);
}
pvsSpecLwdaRandom.specArgs = g_DefaultSpecArgs;

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
pvsSpecLwdaMatsRandom.specArgs = g_DefaultSpecArgs;

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
pvsSpecXbar.specArgs = g_DefaultSpecArgs;

function pvsSpecCbu_0(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0xFF
        }]
    ]);
}
pvsSpecCbu_0.specArgs = g_DefaultSpecArgs;

function pvsSpecCbu_1(testList)
{
    testList.AddTests([
        ["LwdaCbu", { // -test 226
            "StressFactor" : 1,
            "SubtestMask" : 0x3F00
        }]
    ]);
}
pvsSpecCbu_1.specArgs = g_DefaultSpecArgs;

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
pvsSpecLinpack.specArgs = g_DefaultSpecArgs;

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
pvsSpecGlStress.specArgs = g_DefaultSpecArgs;

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
pvsSpecGlrA8R8G8B8_0.specArgs = g_DefaultSpecArgs + "-isolate_frames 14 14";

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
pvsSpecGlrR5G6B5_0.specArgs = g_DefaultSpecArgs + "-isolate_frames 4 4";

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
pvsSpecGlrFsaa2x_0.specArgs = g_DefaultSpecArgs + "-isolate_frames 31 31";

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
pvsSpecGlrFsaa4x_0.specArgs = g_DefaultSpecArgs + "-isolate_frames 5 5";

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
pvsSpecGlrMrtRgbU_0.specArgs = g_DefaultSpecArgs + "-isolate_frames 28 28";

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
pvsSpecGlrMrtRgbF_0.specArgs = g_DefaultSpecArgs + "-isolate_frames 9 9";

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
pvsSpecVkStress.specArgs = g_DefaultSpecArgs;

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
pvsSpecVkStressMesh.specArgs = g_DefaultSpecArgs;

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
pvsSpecVideo.specArgs = "";

function pvsSpelwnitTests(testList)
{
}
pvsSpelwnitTests.specArgs = g_DefaultSpecArgs + "-test TestUnitTests -skip_unit_test SurfaceFillerTest";

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
        ,["LwdaRandom", {"TestConfiguration" : {"Loops" : 1*75, "StartLoop" : 0}}]
    ]);

}
dvsSpecGlrShort.specArgs = g_DefaultSpecArgs;

function addOcgTest(startFrame, numFrames, testList)
{
    var skipCount = 50;
    testList.AddTests([
        ["GLRandomOcg", { //-test 126
//            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : numFrames*skipCount,
                "StartLoop" : startFrame*skipCount
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
    ]);
}

function dvsSpecGlrOcg_0(testList)
{
    addOcgTest(0, 3, testList);
}
dvsSpecGlrOcg_0.specArgs = g_DefaultSpecArgs;

// Using the new Random Generator causes frame 3 to take way too long, so skip it.
// Note: it doesn't fail on silicon and we don't get any RefShader errors either.
function dvsSpecGlrOcg_1(testList)
{
    addOcgTest(4, 3, testList);
}
dvsSpecGlrOcg_1.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_2(testList)
{
    addOcgTest(7, 4, testList);
}
dvsSpecGlrOcg_2.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_3(testList)
{
    addOcgTest(11, 5, testList);
}
dvsSpecGlrOcg_3.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_4(testList)
{
    addOcgTest(16, 3, testList);
}
dvsSpecGlrOcg_4.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_5(testList)
{
    addOcgTest(19, 3, testList);
}
dvsSpecGlrOcg_5.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_6(testList)
{
    addOcgTest(22, 3, testList);
}
dvsSpecGlrOcg_6.specArgs = g_DefaultSpecArgs;
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
userSpec.specArgs = g_DefaultSpecArgs;

//------------------------------------------------------------------------------
// Use this spec is used to run all of the tests on Amodel.
function amodelSpec(testList)
{
    var skipCount = 5;
    var frames = 3;
    var goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        goldenGeneration = true;
    }
    testList.AddTests([
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
amodelSpec.specArgs = g_DefaultSpecArgs;
