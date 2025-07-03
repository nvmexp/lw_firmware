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

SetElw("OGL_ChipName","ga102");
SetElw("LW_DIRECTAMODEL_SIMCLASS","GA10x_AMPERE");

g_SpecArgs  = "-maxwh 160 120 -run_on_error"

let g_DefaultSpecArgs = "";

// dvs uses this file to regress LwvidLWENC test in Amodel. It does ilwocations of mods using the
// following command lines:
//mods -a gpugen.js  -readspec amodel_lwenc.spc -spec pvsSpecLwEnc
//mods -a gputest.js -readspec amodel_lwenc.spc -spec pvsSpecLwEnc


//------------------------------------------------------------------------------

function pvsSpecLwEnc(testList)
{
    testList.AddTests([
		["LwvidLWENC", {
            SizeMax : 5000,
            "TestConfiguration" : {
                "Loops" : 6
            }
        }]
    ]);
}
pvsSpecLwEnc.specArgs = g_DefaultSpecArgs;
