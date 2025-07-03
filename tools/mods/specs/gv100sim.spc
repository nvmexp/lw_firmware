/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "lwr_comm.h"

AddFileId(Out.PriNormal, "$Id: //sw/dev/gpu_drv/chips_a/diag/mods/gpu/js/gv100sim.spc#9 $");

// Add this if you want to see what display engine outputs (in saurat format)
//-display_dump_images

// For automated runs disable CLS, otherwise enable it for the block you suspect.
SetElw("CSL","[disable]");
//SetElw("CSL","smm_core"); // enable lwca debugging messages

// -chipargs -texHandleErrorsLikeRTL prevents asserts in LwdaRandom's texture 
// kernel with the tld4 asm instruction.
// Note: WAR 1843386 Lwca asserts in common_device.c,line 408 when using "-chipargs -num_tpcs=2", we need to set -num_tpcs=7 until its fixed.
g_SpecArgs = "-devid_check_ignore -skip_pertest_pexcheck -chipargs -texHandleErrorsLikeRTL -chipargs -use_floorsweep -chipargs -num_gpcs=1 -chipargs -num_tpcs=7 -chipargs -num_fbps=1 -chipargs -num_ropltcs=2 -maxwh 160 120";

// dvs uses this file to regress Fmodel. It does ilwocations of mods using the
// following command lines:
//mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm_dcb-ultimate.rom gpugen.js -chipargs -chipPOR=gv100 -readspec gv100sim.spc -spec dvsSpecWithDisplay 
//mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gpugen.js -chipargs -chipPOR=gv100 -readspec gv100sim.spc -spec dvsSpecSpecial 
//mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gpugen.js -chipargs -chipPOR=gv100 -readspec gv100sim.spc -spec dvsSpecNoDisplay -null_display
//mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gputest.js -chipargs -chipPOR=gv100 -readspec gv100sim.spc -spec dvsSpecNoDisplay -null_display
//mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gpugen.js -chipargs -chipPOR=gv100 -readspec gv100sim.spc -spec dvsSpecNoDisplayB -null_display
//mods -a -chip gvlit1_fmodel_64.so -gpubios 0 gv100_sim_hbm.rom gputest.js -chipargs -chipPOR=gv100 -readspec gv100sim.spc -spec dvsSpecNoDisplayB -null_display

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
            "TestConfiguration" : {
                "StartLoop" : 0,
                "Loops" : 3, // Number of frames
                "RestartSkipCount" : 3 // Number of frames per modeset
            }
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

//------------------------------------------------------------------------------
// This list can be run with or without a real display, However to get better
// test times, DVS will run with "-null_display". We see a 2x improvement in 
// test time with null_display.
function dvsSpecNoDisplay(testList)
{    
    var skipCount = 1;
    var frames = 1;
    var goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        goldenGeneration = true;
    }
    testList.AddTests([
        ["Random2dFb", { //-test 58
                "TestConfiguration" : {"Loops" : 5} , 
                "Golden" : {"SkipCount" : 5} 
                }]

        ,["CpyEngTest", { //-test 100
                "TestConfiguration" : {"Loops" : 1} 
                }]

/* Takes unreasonable long time to run (2171 seconds)
   Not worth ilwestigating for GV100, should be picked up by Turing
        ,["NewLwdaMatsTest", {
            "MaxFbMb" : 8/1024,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
*/        

        ,["GLStress", { //-test 2
                "LoopMs" : 0, 
                "TestConfiguration" : {"Loops" : 1} 
                }]
    ]);
}

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
                "GridWidth" : 1,
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
        ,["LwdaByteTest", { //-test 111
            "MaxFbMb" : 16/1024,
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true, 
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsTest", {
            "MaxFbMb" : 2,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackDgemm", { //-test 199
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackSgemm", { //-test 200
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackHgemm", { //-test 210
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackHMMAgemm", { //-test 310
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackIgemm", { //-test 212
            Ksize: 8,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseDP", { //-test 292
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseSP", { //-test 296
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseHP", { //-test 211
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseHMMA", { //-test 311
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseIP", { //-test 213
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
    ]);
}

// Zero FB Testing Requirements:
// 1.Must use a VBIOS that disables the display
// 2.Must include these command line arguements: -fb_broken -disable_acr -inst_in_sys -power_feature 0x54555555

function emulationSpec(testList)
{    
    var skipCount = 10;
    var frames = 10;
    var goldenGeneration = false;
    if (Golden.Store == Golden.Action)
    {
        goldenGeneration = true;
    }
    testList.AddTests([
        ["MatsTest", { //-test 3
            "Coverage" : 0.05,  
            "MaxFbMb" : 16, 
            "TestConfiguration" : {"Verbose" : true}
                }]
        ,["FastMatsTest", { // -test 19
            "MaxFbMb" : 32, // 
            "TestConfiguration" : { "Verbose" : true }
            }]
        ,["LwdaL2Mats", { //-test 154
            "Iterations" : 50,
            "TestConfiguration" : { "Verbose" : true }
            }]
        ,["GLRandomCtxSw", { //-test 54 (test 130 in fg, test 2 in bg)
            "ClearLines" : false,
            "FrameRetries" : 0,
            "LogShaderCombinations" : true,
            "UseTessellation" : true,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount, 
                "StartLoop" : 0,
                "Verbose" : true
            },
            // test 2 args
            "DumpPngOnError" : true,
            "LoopMs" : 5,
            "Golden" : { "SkipCount" : skipCount }
         }]

        ,["WfMatsBgStress", { // -test 178 (test 94 in fg, test 2 in bg)
            // test 94 parameters
            "MaxFbMb" : 64, // 
            "InnerLoops" : 1,
            "BoxHeight" : 2,
            "PatternSet" : [0],
            "VerifyChannelSwizzle" : true,
            // test 2 parameters
            "UseTessellation" : true,
            "DumpPngOnError" : true,
            "DumpFrames" : 5,
            "TestConfiguration" : { "Verbose" : true }
            }]
        ,["EvoLwrs", 
            { //-test 4
                "TestConfiguration" : {"Loops" : 1}
            }
         ]
        ,["EvoOvrl", { //-test 7
                "TestConfiguration" : {"Loops" : 1} 
                }]
        ,"LwDisplayRandom"
        ,"LwDisplayHtol"
        ,"I2MTest" //test 205
        ,["Random2dFb", { //-test 58
                "TestConfiguration" : {
                    "Loops" : 10,
                    "Verbose" : true
                },
                "Golden" : {"SkipCount" : 5} 
                }]
        ,["Random2dNc", // -test 9
             { 
                 "TestConfiguration" : {"Loops" : 10},
                 "Golden" : {"SkipCount" : 5}
             }
         ]
        ,["CpyEngTest", { //-test 100
                "TestConfiguration" : {
                    "Loops" : 1500,
                    "Verbose" : true
                }
         }]
         ,["CheckDma", { //-test 41
                "TestConfiguration" : {
                    "Loops" : 100,
                    "Verbose" : true
                }
         }]
        ,["NewWfMatsShort", { //-test 93
            MaxFbMb : 128/1024, // 
            BoxHeight: 2,
            PatternSet : [0],
            VerifyChannelSwizzle : true 
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
         ,["NewWfMatsBusTest", { //-test 123
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
        ,["NewWfMatsCEOnly", { //-test 161
             MaxFbMb : 2048/1024, // 
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
        ,["GLStressFbPulse", { //-test 95
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
         ,["GLComputeTest", { // test 201
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
                "StartLoop" : 0,
                "Verbose" : true
            },
            "Golden" : { "SkipCount" : 10 }
         }]
        ,["LwdaRandom", { //-test 119
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "RestartSkipCount" : 1,
                "Loops" : 24, // 12 // one loop for each kernel 
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
                "LoopMs" : 50,
                "GridWidth" : 1,
                "Iterations" : 2,
                "NumBlocks" : 1,
                "MinFbTraffic" : true,
                "TestConfiguration" : {
                    "ShortenTestForSim" : true,
                    "Verbose" : true
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
        ,["LwdaByteTest", { //-test 111
            "MaxFbMb" : 16/1024, 
            "Iterations" : 1,
            "TestConfiguration" : {
                "ShortenTestForSim" : true, 
                "Verbose" : true
            }
        }]
        ,["NewLwdaMatsTest", { //-test 143
            "MaxFbMb" : 2,
            "Iterations" : 3,
            "TestConfiguration" : {
                "ShortenTestForSim" : true,
                "Verbose" : true
            }
        }]
        ,["LWDECTest", { //-test 277
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
            "DisableEncryption" : true,
            "SaveFrames" : true,
            "StreamSkipMask" : ~((1 << LwencStrm.H264_BASELINE_CAVLC) | // run only H264 Baseline CAVLC &
                                 (1 << LwencStrm.H265_GENERIC)),        // H265 Generic streams
            "MaxFrames" : 10,       // no more that 10 frames per stream
            "EngineSkipMask" : 0xfffffffe, // only run on engine 0
            "TestConfiguration" : {
                "Verbose" : true
            }
        }]
        ,["Bar1RemapperTest", { //-test 151
            "TestConfiguration" : {
                "RestartSkipCount" : 2,
                "Loops" : 2, 
                "StartLoop" : 0,
                "Verbose" : true
            }
        }]
        ,["LwdaRadixSortTest", { //-test 185
            "PercentOclwpancy" : 0.1
        }]
        ,[ "LwdaLinpackDgemm", { //-test 199
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 20,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackSgemm", { //-test 200
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 30,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackHgemm", { //-test 210
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackHMMAgemm", { //-test 310
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackIgemm", { //-test 212
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseDP", { //-test 292
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseSP", { //-test 296
            Ksize: 128,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 2,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseHP", { //-test 211
            Ksize: 48,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseHMMA", { //-test 311
            Ksize: 48,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 1,
                "Verbose" : true
            }
        }]
        ,[ "LwdaLinpackPulseIP", { //-test 213
            Ksize: 96,
            CheckLoops: 1,
            "TestConfiguration" : {
                "Loops" : 2,
                "Verbose" : true
            }
        }]
    ]);
}

