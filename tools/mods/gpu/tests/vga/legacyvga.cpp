//
//    LEGACYVGA.CPP - Interface functions to the legacy VGA and Elpin test suite functions
//
//    Written by:    Kwun Han, Konstantin Bukin, Larry Coffey
//    Date:       12 January 2005
//    Last modified: 26 August 2005
//
//    Routines in this file:
//    LegacyVGATest_RunTest         Runs a given subtest with the given test settings
//    LegacyVGATest_ExceptionFile      Set the exception file to the given file
//    LegacyVGATest_SubTarget       Set the subtarget name
//    LegacyVGATest_SetHeadDac      Set the head and DAC indexes
//    LegacyVGATest_SetHeadSor      Set the head and SOR indexes
//    VGA_CRC_CaptureFrame       Capture a frame to a file
//    VGA_CRC_WaitCapture           Wait until a capture is complete
//    VGA_CRC_ResetCRCValid         Resets the CRC valid flag
//    VGA_CRC_SetFrameState         Set the blink state for the next frame
//    VGA_CRC_SetClockChanged       Flag that a mode set has oclwrred and the RG may be sleeping
//    VGA_CRC_Check              Validate a test run by verifying CRC(s)
//    VGA_CRC_SetHeadDac            Set the head and DAC indexes
//    VGA_CRC_SetHeadSor            Set the head and SOR indexes
//    VGA_CRC_SetInt             Set the interrupt mask bit
//    HandleReturnCode           Log results from the test
//    callfunc                Entry point to call an Elpin function by name
//
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/dispchan.h"
#include "core/include/color.h"
#include "core/include/utility.h"
#include "core/include/imagefil.h"
#include "random.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include <math.h>
#include <list>
#include <map>

#include "mdiag/tests/test.h"
#include "mdiag/tests/test_state_report.h"
#include "core/utility/trep.h"
#include "mdiagrpt.h"

#ifdef LW_MODS
#include "core/include/disp_test.h"
#endif
#include "vgacore.h"
#include "testlist.h"
#include "crccheck.h"
#include "vgasim.h"

// Prototypes local to this module
int HandleReturnCode(int code);
int callfunc(string subtest);

//////////////////////////////////////////////////////
// JavaScript class: LegacyVGATest
//
// Methods:
//    RunTest        Runs a given subtest with the given test settings
//    ExceptionFile  Set the exception file to the given file
//    SubTarget      Set the subtarget name
//    SetHeadDac     Set the head and DAC indexes
//
JS_CLASS(LegacyVGATest);

// Class definition: LegacyVGATest
static SObject LegacyVGATest_Object
(
    "LegacyVGATest",
    LegacyVGATestClass,
    0,
    0,
    "Legacy VGA Test."
);

// Method definition: LegacyVGATest::RunTest
C_(LegacyVGATest_RunTest);
static STest LegacyVGATest_RunTest
(
    LegacyVGATest_Object,
    "RunTest",
    C_LegacyVGATest_RunTest,
    0,
    "Runs Legacy VGA Test"
);

// Method definition: LegacyVGATest::ExceptionFile
C_(LegacyVGATest_ExceptionFile);
static STest LegacyVGATest_ExceptionFile
(
    LegacyVGATest_Object,
    "ExceptionFile",
    C_LegacyVGATest_ExceptionFile,
    0,
    "Sets exception file"
);

// Method definition: LegacyVGATest::SubTarget
C_(LegacyVGATest_SubTarget);
static STest LegacyVGATest_SubTarget
(
    LegacyVGATest_Object,
    "SubTarget",
    C_LegacyVGATest_SubTarget,
    0,
    "Sub Target for the test"
);

// Method definition: LegacyVGATest::SetHeadDac
C_(LegacyVGATest_SetHeadDac);
static STest LegacyVGATest_SetHeadDac
(
    LegacyVGATest_Object,
    "SetHeadDac",
    C_LegacyVGATest_SetHeadDac,
    0,
    "Set head and dac index"
);

// Method definition: LegacyVGATest::SetHeadSor
C_(LegacyVGATest_SetHeadSor);
static STest LegacyVGATest_SetHeadSor
(
    LegacyVGATest_Object,
    "SetHeadSor",
    C_LegacyVGATest_SetHeadSor,
    0,
    "Set head and sor index"
);

//
//    LegacyVGATest_RunTest - Runs a given subtest with the given test settings
//
//    Entry:   subtest           ASCII test name to run
//          SmallFrame        Small frame flag (0 = Use full frames, 1 = Use small frames)
//          SimRun            Simulation run flag (0 = Take no short lwts, 1 = May take simulation shortlwts)
//          BackDoor       Backdoor accesses flag (0 = Use full hardware access, 1 = Use backdoor shortlwts)
//          BackDoorFileRoot  Memory image file root directory
//          Crlwpdate         Update CRC flag
//          TraceRoot         Trace file(s) root directory
//          timeout           Time out value
//    Exit: <RC>           MODS return code
//
//    Notes:   Pass in "TraceRoot" as NULL to skip CRC checks.
//
C_(LegacyVGATest_RunTest)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string subtest;
    UINT32 SmallFrame;
    UINT32 SimRun;
    UINT32 BackDoor;
    UINT32 Crlwpdate;
    string BackDoorFileRoot = "";
    string TraceRoot = "";
    string lwt_file;
    UINT16 wSimType;
    UINT32 timeout;
    UINT32 CrcCheck;

    if( (NumArguments != 9) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &subtest)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &SmallFrame)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &SimRun)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &BackDoor)) ||
        (OK != pJavaScript->FromJsval(pArguments[4], &BackDoorFileRoot)) ||
        (OK != pJavaScript->FromJsval(pArguments[5], &Crlwpdate)) ||
        (OK != pJavaScript->FromJsval(pArguments[6], &TraceRoot)) ||
        (OK != pJavaScript->FromJsval(pArguments[7], &timeout)) ||
        (OK != pJavaScript->FromJsval(pArguments[8], &CrcCheck)) )
    {
        JS_ReportError(pContext, "Usage: LegacyVGATest::RunTest(subtest, SmallFrame, SimRun, BackDoor, BackDoorFileRoot, Crlwpdate, TraceRoot, TimeOut, CrcCheck)");

        return JS_FALSE;
    }
    else
    {
        Printf(Tee::PriHigh,
           "LegacyVGATest::RunTest(subtest=\"%s\", SmallFrame=%d, SimRun=%d, BackDoor=%d, BackDoorFileRoot=\"%s\", Crlwpdate=%d, TraceRoot=\"%s, TimeOut=%d, CrcCheck=%d\")\n",
           subtest.c_str(), SmallFrame, SimRun, BackDoor, BackDoorFileRoot.c_str(), Crlwpdate, TraceRoot.c_str(), timeout, CrcCheck);
    }

    LegacyVGA::vga_timeout_ms(timeout);

   // Set default behavior
   LegacyVGA::SimSetFrameSize (!SmallFrame);
   LegacyVGA::SimSetCaptureMode (CAP_COMPOSITE);

   //
   // Get the VGA library simulation "type" and modify it to match the command line flags.
   //
   // Basically, if in simulation or backdoor mode, set the simulation and the "turbo access" flags.
   // One exception though, if not in backdoor mode, set treat all I/O writes as physical, and therefore
   // clear the "turbo access" flag.
   //
    wSimType = LegacyVGA::SimGetType();
//    if( SimRun ){
    if (SimRun || BackDoor)
    {
   // Should be just SimRun once elpin tests start checking SIM_BACKDOOR
   // Change this logic later - LGC
        wSimType |= SIM_SIMULATION;
        wSimType |= SIM_TURBOACCESS;
    }
    else
    {
        wSimType &= ~SIM_SIMULATION;   // Make it SIM_PHYSICAL
        wSimType &= ~SIM_TURBOACCESS;  // Make it SIM_FULLACCESS
    }
    if (BackDoor)
        wSimType |= SIM_BACKDOOR;
    else
    {
        wSimType &= ~SIM_BACKDOOR;     // Make it SIM_NOBACKDOOR
        wSimType &= ~SIM_TURBOACCESS;  // Treat non-backdoor just like physical
   }
    LegacyVGA::SimSetType(wSimType);

   // Loads the VGA memory with the memory image file
   // Untested, probably won't work (some memory image files are wrong, missing) - LGC
    if (BackDoorFileRoot != "")
    {
        if (SmallFrame)
            lwt_file = BackDoorFileRoot + "/lw50_FullRaster_SmallFrame_00.lwt";
        else
            lwt_file = BackDoorFileRoot + "/lw50_FullRaster_FullFrame_00.lwt";

        Printf (Tee::PriHigh, "Loading FB init file %s\n", lwt_file.c_str());
        if (OK != DispTest::FillVGABackdoor(lwt_file, (UINT32)0, (UINT32)0x40000))     // Hardwired value? Fix this - LGC
        {
            JS_ReportError (pContext, "Error from DispTest::FillVGABackdoor\n");
            return (JS_FALSE);
        }
    }

    // Execute the subtest!
    int ret_code = callfunc(subtest);

   // Check the results of the test
   // Note that "TraceRoot" is NULL if this function is called from "vgaself.js" and that "vgacrc.js" passes a non-NULL value
    if (CrcCheck)
    {
        if (TraceRoot == "")
        {
            // writing status in trep.txt
            FILE * trep = 0;
            if (OK == Utility::OpenFile("trep.txt", &trep, "a"))
            {
                switch(ret_code)
                {
                case ERROR_NONE:
                    fprintf(trep, "test #0: TEST_SUCCEEDED (gold)\n");
                    fprintf(trep, "summary = all tests passed\n");
                    break;
                case ERROR_CRCLEAD:
                    fprintf(trep, "test #0: TEST_SUCCEEDED\n");
                    fprintf(trep, "summary = all tests passed\n");
                    break;
                case ERROR_CRCMISSING:
                    fprintf(trep, "test #0: TEST_CRC_NON_EXISTANT\n");
                    fprintf(trep, "summary = tests failed\n");
                    break;
                case ERROR_CHECKSUM:
                    fprintf(trep, "test #0: TEST_FAILED_CRC\n");
                    fprintf(trep, "summary = tests failed\n");
                    break;
                default:
                    fprintf(trep, "test #0: TEST_FAILED\n");
                    fprintf(trep, "summary = tests failed\n");
                    break;
                }
                fprintf(trep, "expected results = 1\n");
                fclose(trep);
            }
            else
            {
                Printf(Tee::PriHigh, "*** ERROR: can not open trep.txt file for writing\n");
            }
        }
        else
        {
            ret_code = LegacyVGA::CRCManager()->Check(TraceRoot,
                                                      FALSE,
                                                      SmallFrame != 1,
                                                      Crlwpdate == 1,
                                                      "dac1_pixel_trace.out");
        }
    }
    HandleReturnCode(ret_code);     // This may need to be revisited - LGC
    RETURN_RC(OK);
}

//
//    LegacyVGATest_ExceptionFile - Set the exception file to the given file
//
//    Entry:   exception_file    File name of the exception file
//    Exit: <RC>           MODS return code
//
C_(LegacyVGATest_ExceptionFile)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string exception_file;

    if (NumArguments != 1 ||
            (OK != pJavaScript->FromJsval(pArguments[0], &exception_file)))
    {
        JS_ReportError(pContext, "Usage: LegacyVGATest.ExceptionFile(ExceptionFile)");
        return JS_FALSE;
    }

    LegacyVGA::ExceptionFile (exception_file);

    RETURN_RC(OK);
}

//
//    LegacyVGATest_SubTarget - Set the subtarget name
//
//    Entry:   sub_target     Name of the subtarget test
//    Exit: <RC>        MODS return code
//
C_(LegacyVGATest_SubTarget)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string sub_target;

    if (NumArguments != 1 ||
            (OK != pJavaScript->FromJsval(pArguments[0], &sub_target)))
    {
        JS_ReportError(pContext, "Usage: LegacyVGATest.SubTarget(SubTarget)");
        return JS_FALSE;
    }

    LegacyVGA::SubTarget (sub_target);

    RETURN_RC(OK);
}

//
//    LegacyVGATest_SetHeadDac - Set the head and DAC indexes
//
//    Entry:   head     Head index
//          dac         DAC index
//    Exit: <RC>     MODS return code
//
//    Notes:   This is called from both "vgacrc.js" and "vgaself.js".
//
C_(LegacyVGATest_SetHeadDac)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    UINT32  head;
    UINT32  dac;

    if ((NumArguments != 2) ||
            (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
            (OK != pJavaScript->FromJsval(pArguments[1], &dac)))
    {
        JS_ReportError(pContext, "Usage: LegacyVGATest.SetHeadDac(head_index, dac_index)");
        return JS_FALSE;
    }

   Printf (Tee::PriNormal, "LegacyVGATest setting head index to %d and dac index to %d.\n", head, dac);
   LegacyVGA::CRCManager()->SetHeadDac (head, dac);

    RETURN_RC(OK);
}

//
//    LegacyVGATest_SetHeadSor - Set the head and SOR indexes
//
//    Entry:   head     Head index
//          sor         SOR index
//    Exit: <RC>     MODS return code
//
//    Notes:   This is called from both "vgacrc.js" and "vgaself.js".
//
C_(LegacyVGATest_SetHeadSor)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    UINT32  head;
    UINT32  sor;

    if ((NumArguments != 2) ||
            (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
            (OK != pJavaScript->FromJsval(pArguments[1], &sor)))
    {
        JS_ReportError(pContext, "Usage: LegacyVGATest.SetHeadSor(head_index, sor_index)");
        return JS_FALSE;
    }

   Printf (Tee::PriNormal, "LegacyVGATest setting head index to %d and sor index to %d.\n", head, sor);
   LegacyVGA::CRCManager()->SetHeadSor (head, sor);

    RETURN_RC(OK);
}

//////////////////////////////////////////////////////
// JavaScript class: VGA_CRC
//
// Methods:
//    CaptureFrame         Capture a frame to a file
//    WaitCapture          Wait until a capture is complete
//    ResetCRCValid        Resets the CRC valid flag
//    SetFrameState        Set the blink state for the next frame
//    SetClockChanged         Flag that a mode set has oclwrred and the RG may be sleeping
//    Check             Validate a test run by verifying CRC(s)
//    SetHeadDac           Set the head and DAC indexes
//    SetHeadSor           Set the head and SOR indexes
//    SetInt               Set the interrupt mask bit
//
// ------------------------------------------
JS_CLASS_NO_ARGS(VGA_CRC, "class to collect CRCs in VGA tests", "Usage: new VGACRC");

// Method definition: VGA_CRC::CaptureFrame
C_(VGA_CRC_CaptureFrame);
static STest VGA_CRC_CaptureFrame
(
    VGA_CRC_Object,
    "CaptureFrame",
    C_VGA_CRC_CaptureFrame,
    0,
    "Capture both CRC and image of next frame"
);

// Method definition: VGA_CRC::WaitCapture
C_(VGA_CRC_WaitCapture);
static STest VGA_CRC_WaitCapture
(
    VGA_CRC_Object,
    "WaitCapture",
    C_VGA_CRC_WaitCapture,
    0,
    "Wait until capture is done"
);

// Method definition: VGA_CRC::ResetCRCValid
C_(VGA_CRC_ResetCRCValid);
static STest VGA_CRC_ResetCRCValid
(
    VGA_CRC_Object,
    "ResetCRCValid",
    C_VGA_CRC_ResetCRCValid,
    0,
    "Reset CRC Valid signal"
);

// Method definition: VGA_CRC::SetFrameState
C_(VGA_CRC_SetFrameState);
static STest VGA_CRC_SetFrameState
(
    VGA_CRC_Object,
    "SetFrameState",
    C_VGA_CRC_SetFrameState,
    0,
    "set frame set"
);

// Method definition: VGA_CRC::SetClockChanged
C_(VGA_CRC_SetClockChanged);
static STest VGA_CRC_SetClockChanged
(
    VGA_CRC_Object,
    "SetClockChanged",
    C_VGA_CRC_SetClockChanged,
    0,
    "set clock changed"
);

// Method definition: VGA_CRC::Check
C_(VGA_CRC_Check);
static STest VGA_CRC_Check
(
    VGA_CRC_Object,
    "Check",
    C_VGA_CRC_Check,
    0,
    "Checks VGA CRC"
);

// Method definition: VGA_CRC::SetHeadDac
C_(VGA_CRC_SetHeadDac);
static STest VGA_CRC_SetHeadDac
(
    VGA_CRC_Object,
    "SetHeadDac",
    C_VGA_CRC_SetHeadDac,
    0,
    "set active head and DAC"
);

// Method definition: VGA_CRC::SetHeadSor
C_(VGA_CRC_SetHeadSor);
static STest VGA_CRC_SetHeadSor
(
    VGA_CRC_Object,
    "SetHeadSor",
    C_VGA_CRC_SetHeadSor,
    0,
    "set active head and SOR"
);

// Method definition: VGA_CRC::SetInt
C_(VGA_CRC_SetInt);
static STest VGA_CRC_SetInt
(
    VGA_CRC_Object,
    "SetInt",
    C_VGA_CRC_SetInt,
    0,
    "set interrupts"
);

//
//    VGA_CRC_CaptureFrame - Capture a frame to a file
//
//    Entry:   wait     Wait flag (0 = Return control immediately, 1 = Wait for frame to finish capture)
//          update      Update flag (0 = Don't send UpdateNow, 1 = Send UpdateNow method)
//          sync     Sync flag (0 = Don't wait for vsync, 1 = Wait for vsync)
//          timeout_ms  Time out value
//    Exit: <RC>     MODS return code
//
C_(VGA_CRC_CaptureFrame)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 wait;
    UINT32 update;
    UINT32 sync;
    UINT32 timeout_ms;
    if ((NumArguments != 4) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &wait)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &update)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &sync)) ||
        (OK != pJavaScript->FromJsval(pArguments[3], &timeout_ms)))
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.CaptureFrame(wait, update, sync, timeout_ms)");
        return JS_FALSE;
    }

   // Capture the next frame
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->CaptureFrame(wait, update, sync, timeout_ms);

    RETURN_RC(rc);
}

//
//    VGA_CRC_WaitCapture - Wait until a capture is complete
//
//    Entry:   last     Last frame flag (0 = Not the last frame, 1 = Last frame in a series)
//          update      Update flag (0 = Don't send UpdateNow, 1 = Send UpdateNow method)
//          timeout_ms  Time out value
//    Exit: <RC>     MODS return code
//
C_(VGA_CRC_WaitCapture)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 last;
    UINT32 update;
    UINT32 timeout_ms;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &last)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &update)) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &timeout_ms)))
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.WaitCapture(last, update, timeout_ms)");
        return JS_FALSE;
    }

    // Wait for the capture to complete
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->WaitCapture (last, update, timeout_ms);

    RETURN_RC(rc);
}

//
//    VGA_CRC_ResetCRCValid - Resets the CRC valid flag
//
//    Entry:   None
//    Exit: <RC>     MODS return code
//
C_(VGA_CRC_ResetCRCValid)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.ResetCRCValid()");
        return JS_FALSE;
    }

    // Reset the CRC valid bit
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->ResetCRCValid ();

    RETURN_RC(rc);
}

//
//    VGA_CRC_SetFrameState - Set the blink state for the next frame
//
//    Entry:   cursor      Cursor state (0 = Wait for cursor off, 1 = Wait for cursor on)
//          attr     Attribute blink state (0 = Wait for blink off, 1 = Wait for blink on)
//    Exit: <RC>     MODS return code
//
C_(VGA_CRC_SetFrameState)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 cursor;
    UINT32 attr;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &cursor)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &attr)))
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.SetFrameState(cursor, attr)");
        return JS_FALSE;
    }

    // Set the blink state
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->SetFrameState (cursor == 1, attr == 1);

    RETURN_RC(rc);
}

//
//    VGA_CRC_SetClockChanged - Flag that a mode set has oclwrred and the RG may be sleeping
//
//    Entry:   None
//    Exit: <RC>     MODS return code
//
C_(VGA_CRC_SetClockChanged)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.SetClockChanged()");
        return JS_FALSE;
    }

    // Set the clock changed flag
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->SetClockChanged ();

    RETURN_RC(rc);
}

//
//    VGA_CRC_Check - Validate a test run by verifying CRC(s)
//
//    Entry:   test_dir    Test directory
//          active_raster  Active raster flag (0 = Composite raster, 1 = Active raster only)
//          full_frame     Full frame flag (0 = Small frame, 1 = Full frame)
//          dump        Dump flag (0 = Don't save CRC's, 1 = Save CRC's to be leaded)
//          pixel_trace    Trace file name for the image file
//    Exit: <RC>        MODS return code
//
C_(VGA_CRC_Check)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    string   test_dir;
    UINT32   active_raster;
    UINT32   full_frame;
    UINT32   dump;
    string   pixel_trace;

    if (NumArguments != 5 ||
        OK != pJavaScript->FromJsval(pArguments[0], &test_dir) ||
        OK != pJavaScript->FromJsval(pArguments[1], &active_raster) ||
        OK != pJavaScript->FromJsval(pArguments[2], &full_frame) ||
        OK != pJavaScript->FromJsval(pArguments[3], &dump) ||
        OK != pJavaScript->FromJsval(pArguments[4], &pixel_trace))
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.Check(test_dir, active_raster, full_frame, dump, pixel_trace)");
        return JS_FALSE;
    }

   // Check the run and flag any errors
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    HandleReturnCode(vgacrc->Check(test_dir, active_raster == 1, full_frame == 1, dump == 1, pixel_trace));

    RETURN_RC(OK);
}

//
//    VGA_CRC_SetHeadDac - Set the head and DAC indexes
//
//    Entry:   head        Head index
//          dac            DAC index
//    Exit: <RC>        MODS return code
//
C_(VGA_CRC_SetHeadDac)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head, dac;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &dac)))
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.SetHeadDac(head, dac)");
        return JS_FALSE;
    }

    // Set the head and DAC index
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->SetHeadDac(head, dac);

    RETURN_RC(rc);
}

//
//    VGA_CRC_SetHeadSor - Set the head and SOR indexes
//
//    Entry:   head        Head index
//          sor            SOR index
//    Exit: <RC>        MODS return code
//
C_(VGA_CRC_SetHeadSor)
{
   // Standard JS context
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 head, sor;
    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &head)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &sor)))
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.SetHeadSor(head, sor)");
        return JS_FALSE;
    }

    // Set the head and SOR index
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->SetHeadSor(head, sor);

    RETURN_RC(rc);
}

//
//    VGA_CRC_SetInt - Set the interrupt mask bit
//
//    Entry:   int_mask    Interrupt mask
//    Exit: <RC>        MODS return code
//
C_(VGA_CRC_SetInt)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    // parse arguments
    JavaScriptPtr pJavaScript;
    UINT32 int_mask;
    if ((NumArguments != 1) || (OK != pJavaScript->FromJsval(pArguments[0], &int_mask)))
    {
        JS_ReportError(pContext, "Usage: VGA_CRC.SetInt(int_mask)");
        return JS_FALSE;
    }

    // Set the mask
    VGA_CRC* vgacrc = (VGA_CRC*) JS_GetPrivate(pContext, pObject);
    RC rc = vgacrc->SetInt (int_mask);

    RETURN_RC(rc);
}

//
//    HandleReturnCode - Log results from the test
//
//    Entry:   code     Error code
//    Exit: <int>    Error code
//
int HandleReturnCode(int code)
{
    switch (code)
    {
   case ERROR_NONE:        // No error oclwrred
        TestReport::GetTestReport()->crcCheckPassedGold();
        break;
   case ERROR_USERABORT:         // User aborted test
        Printf(Tee::PriHigh, "returned ERROR_USERABORT\n");
        TestReport::GetTestReport()->runInterrupted();
        break;
   case ERROR_UNEXPECTED:        // Unexpected behavior
        Printf(Tee::PriHigh, "returned ERROR_UNEXPECTED\n");
        TestReport::GetTestReport()->runFailed();
        break;
   case ERROR_IOFAILURE:            // I/O Failure
        Printf(Tee::PriHigh, "returned ERROR_IOFAILURE\n");
        TestReport::GetTestReport()->runFailed();
        break;
   case ERROR_CAPTURE:        // Capture buffer didn't match
        Printf(Tee::PriHigh, "returned ERROR_CAPTURE\n");
        TestReport::GetTestReport()->crcCheckFailed();
        break;
   case ERROR_ILWALIDFRAMES:        // Wrong or no frame rate
        Printf(Tee::PriHigh, "returned ERROR_ILWALIDFRAMES\n");
        TestReport::GetTestReport()->crcCheckFailed();
        break;
   case ERROR_INTERNAL:       // Test couldn't complete (malloc failure, etc.)
        Printf(Tee::PriHigh, "returned ERROR_INTERNAL\n");
        TestReport::GetTestReport()->runFailed();
        break;
   case ERROR_MEMORY:         // Video memory failure
        Printf(Tee::PriHigh, "returned ERROR_MEMORY\n");
        TestReport::GetTestReport()->runFailed();
        break;
   case ERROR_VIDEO:       // Video data failure
        Printf(Tee::PriHigh, "returned ERROR_VIDEO\n");
        TestReport::GetTestReport()->runFailed();
        break;
   case ERROR_ILWALIDCLOCK:         // Wrong dot clock frequency
        Printf(Tee::PriHigh, "returned ERROR_ILWALIDCLOCK\n");
        TestReport::GetTestReport()->runFailed();
        break;
   case ERROR_SIM_INIT:       // Initialization error in simulation
        Printf(Tee::PriHigh, "returned ERROR_SIM_INIT\n");
        TestReport::GetTestReport()->runFailed();
        break;
   case ERROR_CHECKSUM:       // Checksum error
        TestReport::GetTestReport()->crcCheckFailed();
        Printf(Tee::PriHigh, "returned ERROR_CHECKSUM\n");
        break;
   case ERROR_FILE:        // File open/read/write/close error
        TestReport::GetTestReport()->runFailed();
        Printf(Tee::PriHigh, "returned ERROR_FILE\n");
        break;
   case ERROR_CRCMISSING:        // no lead crc
        TestReport::GetTestReport()->crcNoFileFound();
        Printf(Tee::PriHigh, "returned ERROR_CRCMISSING\n");
        break;
   case ERROR_CRCLEAD:        // test passed lead crc
        TestReport::GetTestReport()->runSucceeded();
        Printf(Tee::PriHigh, "returned ERROR_CRCLEAD\n");
        break;
    default:
        Printf(Tee::PriHigh, "invalid error code %d\n", code);
        break;
    }

    if (code != ERROR_NONE)
    {
        LegacyVGA::DisplayError();
    }

    return code;
}

//
//    callfunc - Entry point to call an Elpin function by name
//
//    Entry:   subtest     Test name
//    Exit: <int>    Error code from test
//
//    Notes:   1) Allow case insensitive compare for the test name
//          2) Allow test numbers to be substituted for test name
//
int callfunc (string subtest)
{
   typedef int (* TESTFUNC) (void);
   typedef struct _FUNCTIONENTRY
   {
      const char *name;
      TESTFUNC fn;
      int         part;
      int         test;
   } FUNCTIONENTRY;

   // Function names are cAsE iNsEnSiTiVe
   static FUNCTIONENTRY tblFunctionLookup[] =
   {
      {"MotherboardSetupTest",      LegacyVGA::MotherboardSetupTest,    2, 1},      // 0201
      {"AdapterSetupTest",       LegacyVGA::AdapterSetupTest,        2, 2},      // 0202
      {"LimitedSetupTest",       LegacyVGA::LimitedSetupTest,        2, 3},      // 0203

      {"LwrsorTypeTest",            LegacyVGA::LwrsorTypeTest,          3, 1},      // 0301
      {"LwrsorLocationTest",        LegacyVGA::LwrsorLocationTest,         3, 2},      // 0302
      {"LwrsorDisableTest",         LegacyVGA::LwrsorDisableTest,       3, 3},      // 0303
      {"SyncPulseTimingTest",       LegacyVGA::SyncPulseTimingTest,        3, 4},      // 0304
      {"TextModeSkewTest",       LegacyVGA::TextModeSkewTest,        3, 5},      // 0305
      {"VerticalTimesTwoTest",      LegacyVGA::VerticalTimesTwoTest,    3, 6},      // 0306
      {"DotClock2Test",          LegacyVGA::DotClock2Test,           3, 7},      // 0307
      {"Vload2Vload4Test",       LegacyVGA::Vload2Vload4Test,        3, 8},      // 0308
      {"CharWidthTest",          LegacyVGA::CharWidthTest,           3, 9},      // 0309
      {"CRTCWriteProtectTest",      LegacyVGA::CRTCWriteProtectTest,    3, 10},     // 0310
      {"DoubleScanTest",            LegacyVGA::DoubleScanTest,          3, 11},     // 0311
      {"VerticalInterruptTest",     LegacyVGA::VerticalInterruptTest,      3, 12},     // 0312
      {"PanningTest",               LegacyVGA::PanningTest,             3, 13},     // 0313
      {"UnderLineLocationTest",     LegacyVGA::UnderLineLocationTest,      3, 14},     // 0314
      {"SyncDisableTest",           LegacyVGA::SyncDisableTest,            3, 15},     // 0315
      {"LineCompareTest",           LegacyVGA::LineCompareTest,            3, 16},     // 0316
      {"Panning256Test",            LegacyVGA::Panning256Test,          3, 17},     // 0317
      {"CountBy4Test",           LegacyVGA::CountBy4Test,            3, 18},     // 0318
      {"SkewTest",               LegacyVGA::SkewTest,             3, 19},     // 0319
      {"PresetRowScanTest",         LegacyVGA::PresetRowScanTest,       3, 20},     // 0320
      {"LwrsorSkewTest",            LegacyVGA::LwrsorSkewTest,          3, 21},     // 0321
      {"NonBIOSLineCompareTest",    LegacyVGA::NonBIOSLineCompareTest,     3, 22},     // 0322

      {"SyncResetTest",          LegacyVGA::SyncResetTest,           4, 1},      // 0401
      {"CPUMaxBandwidthTest",       LegacyVGA::CPUMaxBandwidthTest,        4, 2},      // 0402
      {"WriteMapReadPlaneTest",     LegacyVGA::WriteMapReadPlaneTest,      4, 3},      // 0403
      {"Std256CharSetTest",         LegacyVGA::Std256CharSetTest,       4, 4},      // 0404
      {"Ext512CharSetTest",         LegacyVGA::Ext512CharSetTest,       4, 5},      // 0405
      {"EightLoadedFontsTest",      LegacyVGA::EightLoadedFontsTest,    4, 6},      // 0406
      {"Text64KTest",               LegacyVGA::Text64KTest,             4, 7},      // 0407
      {"LineCharTest",           LegacyVGA::LineCharTest,            4, 8},      // 0408
      {"LargeCharTest",          LegacyVGA::LargeCharTest,           4, 9},      // 0409
      {"FontFetchStressTest",       LegacyVGA::FontFetchStressTest,        4, 10},     // 0410

      {"SetResetTest",           LegacyVGA::SetResetTest,            5, 1},      // 0501
      {"ColorCompareTest",       LegacyVGA::ColorCompareTest,        5, 2},      // 0502
      {"ROPTest",                LegacyVGA::ROPTest,                 5, 3},      // 0503
      {"WriteMode1Test",            LegacyVGA::WriteMode1Test,          5, 4},      // 0504
      {"WriteMode2Test",            LegacyVGA::WriteMode2Test,          5, 5},      // 0505
      {"WriteMode3Test",            LegacyVGA::WriteMode3Test,          5, 6},      // 0506
      {"VideoSegmentTest",       LegacyVGA::VideoSegmentTest,        5, 7},      // 0507
      {"BitMaskTest",               LegacyVGA::BitMaskTest,             5, 8},      // 0508
      {"NonPlanarReadModeTest",     LegacyVGA::NonPlanarReadModeTest,      5, 9},      // 0509
      {"NonPlanarWriteMode1Test",      LegacyVGA::NonPlanarWriteMode1Test,    5, 10},     // 0510
      {"NonPlanarWriteMode2Test",      LegacyVGA::NonPlanarWriteMode2Test,    5, 11},     // 0511
      {"NonPlanarWriteMode3Test",      LegacyVGA::NonPlanarWriteMode3Test,    5, 12},     // 0512
      {"ShiftRegisterModeTest",     LegacyVGA::ShiftRegisterModeTest,      5, 13},     // 0513

      {"PaletteAddressSourceTest",  LegacyVGA::PaletteAddressSourceTest,   6, 1},      // 0601
      {"BlinkVsIntensityTest",      LegacyVGA::BlinkVsIntensityTest,    6, 2},      // 0602
      {"VideoStatusTest",           LegacyVGA::VideoStatusTest,            6, 3},      // 0603
      {"InternalPaletteTest",       LegacyVGA::InternalPaletteTest,        6, 4},      // 0604
      {"OverscanTest",           LegacyVGA::OverscanTest,            6, 5},      // 0605
      {"V54Test",                LegacyVGA::V54Test,                 6, 6},      // 0606
      {"V67Test",                LegacyVGA::V67Test,                 6, 7},      // 0607
      {"PixelWidthTest",            LegacyVGA::PixelWidthTest,          6, 8},      // 0608
      {"ColorPlaneEnableTest",      LegacyVGA::ColorPlaneEnableTest,    6, 9},      // 0609
      {"GraphicsModeBlinkTest",     LegacyVGA::GraphicsModeBlinkTest,      6, 10},     // 0610

      {"SyncPolarityTest",       LegacyVGA::SyncPolarityTest,        7, 1},      // 0701
      {"PageSelectTest",            LegacyVGA::PageSelectTest,          7, 2},      // 0702
      {"ClockSelectsTest",       LegacyVGA::ClockSelectsTest,        7, 3},      // 0703
      {"RAMEnableTest",          LegacyVGA::RAMEnableTest,           7, 4},      // 0704
      {"CRTCAddressTest",           LegacyVGA::CRTCAddressTest,            7, 5},      // 0705
      {"SwitchReadbackTest",        LegacyVGA::SwitchReadbackTest,         7, 6},      // 0706
      {"SyncStatusTest",            LegacyVGA::SyncStatusTest,          7, 7},      // 0707
      {"VSyncOrVDispTest",       LegacyVGA::VSyncOrVDispTest,        7, 8},      // 0708

      {"DACMaskTest",               LegacyVGA::DACMaskTest,             8, 1},      // 0801
      {"DACReadWriteStatusTest",    LegacyVGA::DACReadWriteStatusTest,     8, 2},      // 0802
      {"DACAutoIncrementTest",      LegacyVGA::DACAutoIncrementTest,    8, 3},      // 0803
      {"DACSparkleTest",            LegacyVGA::DACSparkleTest,          8, 4},      // 0804
      {"DACStressTest",          LegacyVGA::DACStressTest,           8, 5},      // 0805

      {"IORWTest",               LegacyVGA::IORWTest,             9, 1},      // 0901
      {"ByteModeTest",           LegacyVGA::ByteModeTest,            9, 2},      // 0902
      {"Chain2Chain4Test",       LegacyVGA::Chain2Chain4Test,        9, 3},      // 0903
      {"CGAHerlwlesTest",           LegacyVGA::CGAHerlwlesTest,            9, 4},      // 0904
      {"LatchTest",              LegacyVGA::LatchTest,               9, 5},      // 0905
      {"Mode0Test",              LegacyVGA::Mode0Test,               9, 6},      // 0906
      {"Mode3Test",              LegacyVGA::Mode3Test,               9, 7},      // 0907
      {"Mode5Test",              LegacyVGA::Mode5Test,               9, 8},      // 0908
      {"Mode6Test",              LegacyVGA::Mode6Test,               9, 9},      // 0909
      {"Mode7Test",              LegacyVGA::Mode7Test,               9, 10},     // 0910
      {"ModeDTest",              LegacyVGA::ModeDTest,               9, 11},     // 0911
      {"Mode12Test",             LegacyVGA::Mode12Test,              9, 12},     // 0912
      {"ModeFTest",              LegacyVGA::ModeFTest,               9, 13},     // 0913
      {"Mode13Test",             LegacyVGA::Mode13Test,              9, 14},     // 0914
      {"HiResMono1Test",            LegacyVGA::HiResMono1Test,          9, 15},     // 0915
      {"HiResMono2Test",            LegacyVGA::HiResMono2Test,          9, 16},     // 0916
      {"HiResColor1Test",           LegacyVGA::HiResColor1Test,            9, 17},     // 0917
      {"ModeXTest",              LegacyVGA::ModeXTest,               9, 18},     // 0918
      {"MemoryAccessTest",       LegacyVGA::MemoryAccessTest,        9, 19},     // 0919
      {"RandomAccessTest",       LegacyVGA::RandomAccessTest,        9, 20},     // 0920

      {"UndocLightpenTest",         LegacyVGA::UndocLightpenTest,       10, 1},     // 1001
      {"UndocLatchTest",            LegacyVGA::UndocLatchTest,          10, 2},     // 1002
      {"UndocATCToggleTest",        LegacyVGA::UndocATCToggleTest,         10, 3},     // 1003
      {"UndocATCIndexTest",         LegacyVGA::UndocATCIndexTest,       10, 4},     // 1004

      {"TestTest",               LegacyVGA::TestTest,             11, 0},     // 1100
      {"IORWTestLW50",           LegacyVGA::IORWTestLW50,            11, 1},     // 1101
      {"IORWTestGF100",           LegacyVGA::IORWTestGF100,            11, 1},     // 1101
      {"IORWTestGF11x",           LegacyVGA::IORWTestGF11x,            11, 1},     // 1101
      {"IORWTestMCP89",           LegacyVGA::IORWTestMCP89,            11, 1},     // 1101
      {"SyncPulseTimingTestLW50",      LegacyVGA::SyncPulseTimingTestLW50,    11, 2},     // 1102
      {"TextModeSkewTestLW50",      LegacyVGA::TextModeSkewTestLW50,    11, 3},     // 1103
      {"VerticalTimesTwoTestLW50",  LegacyVGA::VerticalTimesTwoTestLW50,   11, 4},     // 1104
      {"SkewTestLW50",           LegacyVGA::SkewTestLW50,            11, 5},     // 1105
      {"LwrsorBlinkTestLW50",       LegacyVGA::LwrsorBlinkTestLW50,        11, 6},     // 1106
      {"GraphicsBlinkTestLW50",     LegacyVGA::GraphicsBlinkTestLW50,      11, 7},     // 1107
      {"MethodGenTestLW50",         LegacyVGA::MethodGenTestLW50,       11, 8},     // 1108
      {"MethodGenTestGF11x",         LegacyVGA::MethodGenTestGF11x,       11, 8},     // 1108
      {"BlankStressTestLW50",       LegacyVGA::BlankStressTestLW50,        11, 9},     // 1109
      {"LineCompareTestLW50",       LegacyVGA::LineCompareTestLW50,        11, 10}, // 1110
      {"StartAddressTestLW50",      LegacyVGA::StartAddressTestLW50,    11, 11}, // 1111
      {"InputStatus1TestLW50",      LegacyVGA::InputStatus1TestLW50,    11, 12}, // 1112
      {"DoubleBufferTestLW50",      LegacyVGA::DoubleBufferTestLW50,    11, 13}, // 1113

      {"FBPerfMode03TestLW50",      LegacyVGA::FBPerfMode03TestLW50,    12, 1},     // 1201
      {"FBPerfMode12TestLW50",      LegacyVGA::FBPerfMode12TestLW50,    12, 2},     // 1202
      {"FBPerfMode13TestLW50",      LegacyVGA::FBPerfMode13TestLW50,    12, 3},     // 1203
      {"FBPerfMode101TestLW50",     LegacyVGA::FBPerfMode101TestLW50,      12, 4}      // 1204
   };
   static int     cFunctionLookup = sizeof (tblFunctionLookup) / sizeof (FUNCTIONENTRY);
   int            i, nRet;
   char        szNumName[32];

   for (i = 0; i < cFunctionLookup; i++)
   {
      sprintf (szNumName, "%02d%02d", tblFunctionLookup[i].part, tblFunctionLookup[i].test);
      if ((stricmp (subtest.c_str (), tblFunctionLookup[i].name) == 0) ||
         (strcmp (subtest.c_str (), szNumName) == 0))
      {
         Printf (Tee::PriNormal, "Starting test: %s\n", tblFunctionLookup[i].name);
         LegacyVGA::SimStart ();
         nRet = tblFunctionLookup[i].fn ();
         LegacyVGA::SimEnd ();
         return (nRet);
      }
   }

   Printf (Tee::PriHigh, "Error: cannot find -subtest \"%s\"\n", subtest.c_str());
   return (RC::BAD_COMMAND_LINE_ARGUMENT);
}

