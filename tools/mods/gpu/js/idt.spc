/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
*/
AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/idt.spc#6 $");

//g_SpecArgs = ""; Not suported with legacy IDT command line

//------------------------------------------------------------------------------
function check_display(testList)
{
    testList.AddTest("IDT", {Displays : '["primary"]', Image: "diag_stripes", Resolution : "native"});
}

function check_displays(testList)
{
    testList.AddTest("IDT", {Displays : '["all"]', Image: "diag_stripes", Resolution : "native"});
}

function check_displays_stereo(testList)
{
    testList.AddTest("IDT", {Displays : '["all"]', Image: "diag_stripes", RtImage: "fp_gray", Resolution : "native"});
}

function check_displays_dvs(testList)
{
    testList.AddTest("IDT", {Displays : '["all"]', Image: "diag_stripes", Resolution : "native", "SkipHDCP" : true, "EnableDVSRun" : true, "TestUnsupportedDisplays" : true});
}

function check_display_bar(testList)
{
    testList.AddTest("IDT", {Displays : '["primary"]', Image: "horiz_bars", Resolution : "max"});
}

function check_display_bars(testList)
{
    testList.AddTest("IDT", {Displays : '["all"]', Image: "horiz_bars", Resolution : "max"});
}

function check_fp_gray(testList)
{
    testList.AddTest("IDT", {Displays : '["all"]', Image: "fp_gray", Resolution : "max"});

}

function check_fp_stripes(testList)
{
    testList.AddTest("IDT", {Displays : '["all"]', Image: "horiz_stripes", Resolution : "max"});
}

function check_displays_all(testList)
{
    testList.AddTest("IDT", {
                             	Displays : '["all"]',
                                Image: "diag_stripes",
                                Resolution : "native",
                                SkipSingleDisplays : true,
                                PromptTimeout : 10,
                                AssumeSuccessOnPromptTimeout : true
                            });
}

function check_display_lwdisplay(testList)
{
    testList.AddTest("IDT", {Displays : '["primary"]', Image: "diag_stripes", Resolution : "native", "SkipHDCP" : true});
}

function another_check_displays_test(testList)
{
    var displaysExVal = JSON.stringify(
                                [
                                    {"0x200": [1024, 768, 32, 60]},
                                    {"0x1000" : "max"}
                                ] );
        
    testList.AddTest("IDT", {DisplaysEx : displaysExVal, Image: "diag_stripes"});
}
