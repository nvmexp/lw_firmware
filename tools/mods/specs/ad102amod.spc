/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
AddFileId(Out.PriNormal, "$Id$");

SetElw("OGL_ChipName","ad102");
SetElw("LW_DIRECTAMODEL_SIMCLASS","AD10x_ADA");

g_SpecArgs  = "-maxwh 160 120 -run_on_error"

// Comment out the below two lines when using "-text x" argument
// for the tests that don't use goldens.
g_IsGenAndTest = true;
g_LoadGoldelwalues = false;

let g_DefaultSpecArgs = "";

// dvs uses this file to regress Amodel. It does ilwocations of mods using the
// following command lines:
//mods gputest.js -readspec ad102amod.spc -spec dvsSpecGlrShort

function dvsSpecGlrShort(testList)
{
    var skipCount = 1;
    var frames = 1;
    testList.AddTests([
        ["LwdaLinpackCask",
        { //-test 314
            "KernelName" : "lwtlass_simt_hgemm_128x128_8x2_nt_align1",
            "Msize" : 128,
            "Nsize" : 128,
            "Ksize" : 8,
            "MNKMode" : 2,
            "ReportFailingSmids" : false,
            "RuntimeMs": 0,
            "TestConfiguration" :
            {
                "Loops" : 5
            }
        }]
        ,["GlrA8R8G8B8", { //-test 130
            "ClearLines" : false,
            "LogShaderCombinations" : true,
            "FrameRetries" : 0,
            "TestConfiguration" : {
                "RestartSkipCount" : skipCount,
                "Loops" : frames*skipCount,
                "StartLoop" : 0
            },
            "Golden" : { "SkipCount" : skipCount }
         }]
        ,["LwdaRandom", {"TestConfiguration" : {"Loops" : 1*75, "StartLoop" : 0}}] // -test 119
        ,"VkTriangle" // -test 266
        ,["VkStressRay", { // -test 368
            Debug:              1,
            LoopMs:             0,
            FramesPerSubmit:    1,
            Raytracer:          { FramesPerSubmit: 1 },
            TestConfiguration: {
                Loops: 4
            }
        }]
    ]);

}
dvsSpecGlrShort.specArgs = g_DefaultSpecArgs;

function addOcgTest(startFrame, numFrames, testList)
{
    var skipCount = 50;
    testList.AddTests([
        ["GLRandomOcg", { //-test 126
            "LogShaderCombinations" : true,
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
    addOcgTest(0, 1, testList);
    addOcgTest(2, 2, testList);
}
dvsSpecGlrOcg_0.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_1(testList)
{
    addOcgTest(4, 3, testList);
}
dvsSpecGlrOcg_1.specArgs = g_DefaultSpecArgs;

// Frame 10 times out on the 38th loop, so skip that one too.
function dvsSpecGlrOcg_2(testList)
{
    addOcgTest(7, 3, testList);
    addOcgTest(11, 1, testList);
}
dvsSpecGlrOcg_2.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_3(testList)
{
    addOcgTest(12, 5, testList);
}
dvsSpecGlrOcg_3.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_4(testList)
{
    addOcgTest(17, 4, testList);
}
dvsSpecGlrOcg_4.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_5(testList)
{
    addOcgTest(1, 1, testList);
    addOcgTest(21, 1, testList);
    addOcgTest(23, 2, testList);
}
dvsSpecGlrOcg_5.specArgs = g_DefaultSpecArgs;

function dvsSpecGlrOcg_6(testList)
{
    addOcgTest(22, 1, testList);
}
dvsSpecGlrOcg_6.specArgs = g_DefaultSpecArgs;

function dvsSpecVideoLwjpg(testList)
{
    testList.AddTests([
        // test 361 - skip high resolution streams in Amodel by default:
        ["LwvidLWJPG", { StreamSkipMask: 0xffffffc0, UseLwda: false, UseEvents: false }]
    ]);
}
dvsSpecVideoLwjpg.specArgs = g_DefaultSpecArgs;

function dvsSpecVideoLwofa(testList)
{
    testList.AddTests([
        "LwvidLwOfa" // test 362
    ]);
}
dvsSpecVideoLwofa.specArgs = g_DefaultSpecArgs;

function dvsSpecVideoLwenc(testList)
{
    testList.AddTests([
        ["LwvidLWENC", {  // test 363
            SizeMax : 5000,
            "TestConfiguration" : {
                "Loops" : 6
            }
         }]
    ]);
}
dvsSpecVideoLwenc.specArgs = g_DefaultSpecArgs;

function dvsSpecVideoLwdecLegacy(testList)
{
    testList.AddTests([
        // Legacy test includes only single H.264 stream
        ["LwvidLWDEC", {  // test 360
            // Running only H264_Perf_world_of_warcraft_1280x720.yuv_HD15Mbps_30.264 stream
            "SkipStreamMask" : "0xFFFFFFF7"
         }]
    ]);
}
dvsSpecVideoLwdecLegacy.specArgs = g_DefaultSpecArgs;

function dvsSpecVideoLwdec(testList)
{
    testList.AddTests([
        // test includes single H.265 stream
        ["LwvidLWDEC", {  // test 360
            // Running only old-town-cross-001.265 stream
            "SkipStreamMask" : "0xFFFFFFDF"
         }]
    ]);
}
dvsSpecVideoLwdec.specArgs = g_DefaultSpecArgs;