/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before userSpec is
// called
g_TestMode = (1<<2); // TM_MFG_BOARD
g_SpecArgs = "-no_gold -run_on_error"

var g_BoardDbTests = [
     "ValidSkuCheck"
    ,"ValidSkuCheck2"
    ,"CheckInfoROM"
    ,"CheckGc6EC"
    ,"CheckPwrSensors"
];

function BoardDb_LogTestWrapper(testName, rc)
{
    for (var ii = 0; ii < g_BoardDbTests.length; ii++)
    {
        if (testName == g_BoardDbTests[ii])
        {
            // Log the test for the confirmation string
            BoardDB.LogTest(testName, rc);
            return;
        }
    }
}
var LogTestWrapper = BoardDb_LogTestWrapper;

function ConfirmBoard()
{
    var board = new Object;
    var rc = RC.OK;
    rc = this.BoundGpuSubdevice.GetBoardInfo(board);
    if (RC.OK == rc)
        BoardDB.ConfirmBoard(board.BoardName, board.BoardDefIndex);
    return rc;
}

function ConfirmEntry()
{
    if (Log.FirstError == RC.OK)
        return BoardDB.ConfirmEntry();
    return Log.FirstError;
}

//------------------------------------------------------------------------------
// BoardsDB test regressions
//------------------------------------------------------------------------------
function userSpec(testList, PerfPtList)
{    
    testList.TestMode = (1<<2); // TM_MFG_BOARD
    
    if ((PerfPtList.length > 0) && (PerfPtList[0] !== null))
    {
        testList.AddTest("SetPState", { InfPts : PerfPtList[0] });
    }

    // Still need to verify that we are running on the correct board/index
    // even if not verifying all the other items in ValidSkuCheck
    if (!(BoardDB.ConfirmTestRequired("ValidSkuCheck"))
        && !(BoardDB.ConfirmTestRequired("ValidSkuCheck2")))
    {
        testList.AddTest("RunUserFunc", {"UserFunc" : ConfirmBoard});
    }

    for (var testIdx = 0; testIdx < g_BoardDbTests.length; testIdx++)
    {
        if (BoardDB.ConfirmTestRequired(g_BoardDbTests[testIdx]))
        {
            testList.AddTest(g_BoardDbTests[testIdx]);
        }
    }
    if (!BoardDB.BoardChangeRequested)
    {
        Out.Printf(Out.PriHigh, 
            "******************************************************\n" +
            "* boarddb.spc is used to confirm pending boards.db   *\n" +
            "* changes.  The boards.db or boards.dbe in the       *\n" +
            "* runspace was not generated for this purpose.  If   *\n" +
            "* you are trying to confirm a boards.db change       *\n" +
            "* ensure that you have deleted any existing          *\n" +
            "* boards.db or boards.dbe files from the MODS        *\n" +
            "* runspace and that you have correctly copied the    *\n" +
            "* website boards.dbe file into the MODS runspace     *\n" +
            "* before running MODS                                *\n" +
            "******************************************************\n");
        return RC.BAD_COMMAND_LINE_ARGUMENT;
    }
    else
    {
        testList.AddTest("RunUserFunc", {"UserFunc" : ConfirmEntry});
        g_MustBeSupported = (BoardDB.BCRVersion != 0);
    }
    return RC.OK;
}

var g_SpecFileIsSupported = SpecFileIsSupportedAll;
