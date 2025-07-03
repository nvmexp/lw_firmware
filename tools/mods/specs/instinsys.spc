/*
* LWIDIA_COPYRIGHT_BEGIN
** Copyright 2016,2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
** LWIDIA_COPYRIGHT_END
*/

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/instinsys.spc#1 $");

// This spec file is created to catch any inst in sys related regressions.

//------------------------------------------------------------------------------
function userSpec(testList, PerfPtList)
{
    // run only 1 pstate
    PerfPtList = PerfPtList.slice(0,1);

    for (var i = 0; i < PerfPtList.length; i++)
    {
        testList.AddTest("SetPState", { InfPts : PerfPtList[i] });
        addBoardMfgTestsOnePState(testList, i, PerfPtList);
    }
}

//------------------------------------------------------------------------------
function addBoardMfgTestsOnePState(
    testList          // (TestList)
    ,pStateIdx        // (integer) how many pstates we've tested already
    ,perfPts
)
{
    testList.AddTests([
            [ "GLPowerStress"     /*153*/, {}]
            ,[ "NewWfMats"        /*94*/,  {}]
            ,[ "NewWfMatsShort"   /*93*/,  {}]
            ,[ "WfMatsBgStress"   /*178*/, {}]
            ,[ "WfMatsMedium"     /*118*/, {}]
            ,[ "NewWfMatsCEOnly"  /*161*/, {}]
            ,[ "GlrA8R8G8B8"      /*130*/, {}]
            ,[ "PerfSwitch"       /*145*/, {}] 
            ,[ "NewLwdaMatsTest"  /*143*/, {}] 
            ]);
}
