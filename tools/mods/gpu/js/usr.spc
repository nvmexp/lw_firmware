/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/usr.spc#9 $");

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called
g_TestMode = (1<<5); // TM_END_USER

//------------------------------------------------------------------------------
// End-User tests 
//------------------------------------------------------------------------------
function addModsGpuTests(testList, PerfPtList)
{
    testList.TestMode = (1<<5); // TM_END_USER
    
    g_LoadGoldelwalues = false;
    g_IsGenAndTest = true;
    
    testList.AddTest("SetPState", { InfPts : PerfPtList[0] });

    testList.AddTests([
        "FastMatsTest"
        ,["Random2dFb",         setLoops(5*200)]
        ,["Elpg",               setLoops(16)]
        ,["GlrA8R8G8B8",        setLoops(5*20)]
        ]);    
}
