/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
AddFileId(Out.PriNormal, "$Id: $");

SetElw("OGL_ChipName","ga100");

g_SpecArgs  = "-maxwh 160 120 -run_on_error"

let g_DefaultSpecArgs = "";

// dvs uses this file to regress LwvidLwJpg test in Amodel. It does ilwocations of mods using the
// following command lines:
//mods -a gpugen.js  -readspec ga100lwjpg.spc -spec pvsSpecLwJpg
//mods -a gputest.js -readspec ga100lwjpg.spc -spec pvsSpecLwJpg


//------------------------------------------------------------------------------

function pvsSpecLwJpg(testList)
{
    testList.AddTests([
        // Skip high resolution streams in Amodel PVS by default
        ["LwvidLWJPG", { StreamSkipMask: 0xffffffc0, UseLwda: false, UseEvents: false }]
    ]);
}
pvsSpecLwJpg.specArgs = g_DefaultSpecArgs;
