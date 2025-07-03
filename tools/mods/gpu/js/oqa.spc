/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/oqa.spc#31 $");

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called
g_TestMode = (1<<0); // TM_OQA

//------------------------------------------------------------------------------
// OQA (Outgoing Quality Assurance) is the quick test run by systems assemblers.
//
// We will run only a few tests, with reduced duration, at only one pstate.
//------------------------------------------------------------------------------
function addModsGpuTests(testList, PerfPtList)
{
    testList.TestMode = (1<<0); // TM_OQA

    testList.AddTest("SetPState", { InfPts : PerfPtList[0] });

    testList.AddTests([
        "CheckConfig"
        ,"CheckClocks"
        ,"CheckInfoROM"
        ,"CheckFanSanity"
        ,["CheckAVFS", { AdcMargin:3,NegAdcMargin:-3.5,SkipAdcCalCheck:true,Verbose:true,LowTempLwtoff:59.999,LowTempAdcMargin:3,NegLowTempAdcMargin:-4.2 }] //$
        ,"I2CTest"
        ,["FastMatsTest", { Coverage : 1 }]
        ,"LwvidLWDEC"
        ,"PcieSpeedChange"
        ,["Random2dFb",     setLoops(5*200)]
        ,["GLStress",     { RuntimeMs: 2000 }]
        ,["GlrA8R8G8B8",    setLoops(10*20)]
        ,["NewLwdaMatsTest",setIterations(15)]
        ,["CaskLinpackSgemm",setLoops(100)]
        ,["LwdaRandom",     setLoops(15*75)]
        ,"LwdaL2Mats"
        ]);
}
