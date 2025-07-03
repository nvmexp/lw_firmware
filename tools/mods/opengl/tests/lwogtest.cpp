/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2009,2011-2012,2014,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "opengl/modsgl.h"
#include "core/include/massert.h"
#include "gpu/tests/gputestc.h"       // TestConfiguration
#include "random.h"
#include "core/include/xp.h"
#include "core/include/display.h"
#include "core/include/jscript.h"
#include "core/include/imagefil.h"
#include "core/include/utility.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/include/gpusbdev.h"
#include <math.h>

// Hacks to get this to compile and run
#define LWOG_TEST_PATH_BASE             0
#define LWOG_TEST_PATH_DLIST            1
#define LWOG_TEST_PATH_DLIST_CE         2
#define LWOG_TEST_PATH_AE               3 // Path for glArrayElement
#define LWOG_TEST_PATH_DE               4 // Path for glDrawElements
#define LWOG_TEST_PATH_DA               5 // Path for glDrawArrays
#define LWOG_TEST_PATH_MAX              6

#define LWOG_TEST_PATH_MASK_BASE        0x00001
#define LWOG_TEST_PATH_MASK_DLIST       0x00002
#define LWOG_TEST_PATH_MASK_DLIST_CE    0x00004
#define LWOG_TEST_PATH_MASK_AE          0x00008
#define LWOG_TEST_PATH_MASK_DE          0x00010
#define LWOG_TEST_PATH_MASK_DA          0x00020
#define LWOG_TEST_PATH_MASK_LAME        0x10000
#define LWOG_TEST_PATH_MASK_DEBUG_TEST  0x200000

#define LWOG_TEST_PATH_MASK_ALL         (LWOG_TEST_PATH_MASK_BASE| \
                                         LWOG_TEST_PATH_MASK_DLIST| \
                                         LWOG_TEST_PATH_MASK_DLIST_CE| \
                                         LWOG_TEST_PATH_MASK_AE| \
                                         LWOG_TEST_PATH_MASK_DE| \
                                         LWOG_TEST_PATH_MASK_DA)

// XXXX beware: this structure needs to match the corresponding structure in
//       //sw/main/tests/OpenGL/lwogtest/elw/elw.h
typedef struct _OGTEST {
   void (*getDescription)(char *str, const char *testName);
   int (*isSupportedFunc)(const char *testName);
   void (*initFunc)(float, const char *testName, int path);
   void (*doFunc)(float, const char *testName, int path);
   void (*exitFunc)(const char *testName, int path);
   const char *name[LWOG_TEST_PATH_MAX];
   unsigned int pathMask;
   unsigned int duration;
   unsigned int crcValidMask;
   unsigned int crc[LWOG_TEST_PATH_MAX];
} OGTEST;

typedef struct _Cell {
   int x;
   int y;
} Cell;

//
// Lwogtest: LWPU OpenGL Tests.
//
namespace Lwogtest
{
   // JavaScript options:
   TestDevicePtr d_pTestDevice;
   GpuTestConfiguration d_TstCfg;
   UINT32 d_PathMask;
   bool d_DoubleBuffer;
   bool d_DumpTga;
   bool d_DumpPng;
   bool d_RunLameTests;
   bool d_NoSwapBuffers;
   bool d_UseWindow;
   bool d_Cmodel;
   bool d_NoIntrusiveComment;
   bool d_OneCell;
   bool d_SWRead;
   INT32 d_CellX, d_CellY;
   INT32 d_WindowX, d_WindowY;
   INT32 d_WindowWidth, d_WindowHeight;

   // test information imported from the lwogtest DLL
   const OGTEST *TestStruct;
   int TestStructEntries;

   // lwogtest DLL imports
   void *dll_Handle;
   void (*dll_InitEntrypoints)(void);
   void (*dll_QueryImplementationExtensions)(void);
   void (*dll_QueryImplementationCaps)(void);
   int  (*dll_lwRand)(void);
   void (*dll_lwSRand)(unsigned int n);
   void (*dll_lwSetRandFunc)(int (*f)(void));
   int  (*dll_filterInit)(int first, int last);
   OGTEST *dll_global_p_TestStruct;
   int *dll_global_p_TestStructEntries;
   int *dll_global_p_cmodel;
   int *dll_global_p_noIntrusiveComment;
   int *dll_global_p_testPath;
   int *dll_global_p_lwrrentWindowWidth;
   int *dll_global_p_lwrrentWindowHeight;
   int *dll_global_p_nOneCells;
   Cell *dll_global_p_oneCells;
   int *dll_global_p_useSWReadPixels;

   mglTestContext d_mglTestContext;

   // local functions:
   RC RunAll();
   RC InnerRunAll();
   RC RunTests(uintN NumArguments, jsval *pArguments);
   RC InnerRunTests(uintN NumArguments, jsval *pArguments);

   RC InitProperties();
   RC InitDLL();
   RC RunOneTest(int i);
   RC GenList(string FileName);

}; // end namespace Lwogtest

#define GET_DLL_PROC(procName, dataType)                            \
do {                                                                \
    dll_##procName =                                                \
        (dataType) Xp::GetDLLProc(dll_Handle, #procName);           \
    if (!dll_##procName) {                                          \
        Printf(Tee::PriHigh, "Cannot load lwogtest symbol (%s)\n",  \
               #procName);                                          \
        return RC::DLL_LOAD_FAILED;                                 \
    }                                                               \
} while (0)

#define GET_DLL_GLOBAL(varName, dataType)                           \
do {                                                                \
    dll_global_p_##varName =                                        \
        (dataType) Xp::GetDLLProc(dll_Handle, #varName);            \
    if (!dll_global_p_##varName) {                                  \
        Printf(Tee::PriHigh, "Cannot load lwogtest symbol (%s)\n",  \
               #varName);                                           \
        return RC::DLL_LOAD_FAILED;                                 \
    }                                                               \
} while (0)

RC Lwogtest::InitDLL()
{
    string dllname = "liblwogtest" + Xp::GetDLLSuffix();
    RC rc = Xp::LoadDLL(dllname, &dll_Handle, Xp::UnloadDLLOnExit);
    if (OK != rc) {
        Printf(Tee::PriHigh, "Cannot load lwogtest library %s\n",
               dllname.c_str());
        return false;
    }

    GET_DLL_PROC(InitEntrypoints, void(*)(void));
    GET_DLL_PROC(QueryImplementationExtensions, void(*)(void));
    GET_DLL_PROC(QueryImplementationCaps, void(*)(void));
    GET_DLL_PROC(lwRand, int(*)(void));
    GET_DLL_PROC(lwSRand, void(*)(unsigned int));
    GET_DLL_PROC(lwSetRandFunc, void(*)(int (*)(void)));
    GET_DLL_PROC(filterInit, int(*)(int, int));

    GET_DLL_GLOBAL(TestStruct, OGTEST *);
    GET_DLL_GLOBAL(TestStructEntries, int *);
    GET_DLL_GLOBAL(cmodel, int *);
    GET_DLL_GLOBAL(noIntrusiveComment, int *);
    GET_DLL_GLOBAL(testPath, int *);
    GET_DLL_GLOBAL(lwrrentWindowWidth, int *);
    GET_DLL_GLOBAL(lwrrentWindowHeight, int *);
    GET_DLL_GLOBAL(nOneCells, int *);
    GET_DLL_GLOBAL(oneCells, Cell *);
    GET_DLL_GLOBAL(useSWReadPixels, int *);

    TestStruct = dll_global_p_TestStruct;
    TestStructEntries = *dll_global_p_TestStructEntries;

    *dll_global_p_cmodel = d_Cmodel;
    *dll_global_p_noIntrusiveComment = d_NoIntrusiveComment;
    *dll_global_p_lwrrentWindowWidth = d_WindowWidth;
    *dll_global_p_lwrrentWindowHeight = d_WindowHeight;
    *dll_global_p_nOneCells = d_OneCell ? 1 : 0;
    *dll_global_p_useSWReadPixels = d_SWRead ? 1 : 0;
    dll_global_p_oneCells->x = d_CellX;
    dll_global_p_oneCells->y = d_CellY;

    dll_InitEntrypoints();
    dll_QueryImplementationExtensions();
    dll_QueryImplementationCaps();
    dll_lwSetRandFunc(dll_lwRand);

    Printf(Tee::PriNormal, "lwogtest loaded successfully.\n");
    return OK;
}

//------------------------------------------------------------------------------
// JavaScript linkage

JS_CLASS(Lwogtest);

static SObject Lwogtest_Object
(
   "Lwogtest",
   LwogtestClass,
   0,
   0,
   "LWPU OpenGL Tests."
);

/**
 * To add recognition of a new switch:
 *      1) Add SProperty below where the second element matches ArgXxx info.
 *         in diag\mods\gpu\js\lwogtest.js
 *      2) in lwogtest.js add AddArg() call, and ArgXxx() function below that.
 *      3) if you call a function in the lwogtest lib, you may have to add
 *         an entry to tests\OpenGL\lwogtest\mods\liblwogtest.def
 *      4) if you change the .def, you have to check in new .lib files for win32.
 *         if testing on win32, copy the liblwogtest.lib file from
 *         tests\OpenGL\lwogtest\mods\debug\winsim\x86 to diag\mods\win32\x86
 *         before testing.
 */

/**
 * @brief Which paths for each test should be run.
 */
static SProperty Lwogtest_PathMask
(
   Lwogtest_Object,
   "PathMask",
   0,
   LWOG_TEST_PATH_MASK_BASE,
   0,
   0,
   0,
   "Which paths for each test should be run."
);

/**
 * @brief Double buffer mode.
 */
static SProperty Lwogtest_DoubleBuffer
(
   Lwogtest_Object,
   "DoubleBuffer",
   0,
   GL_TRUE,
   0,
   0,
   0,
   "Render to back buffer & swap (otherwise, render to front buffer)"
);

/**
 * @brief Enable dumping of TGA image files.
 */
static SProperty Lwogtest_DumpTga
(
   Lwogtest_Object,
   "DumpTga",
   0,
   false,
   0,
   0,
   0,
   "Enable dumping of TGA image files."
);

/**
 * @brief Enable dumping of PNG image files.
 */
static SProperty Lwogtest_DumpPng
(
   Lwogtest_Object,
   "DumpPng",
   0,
   false,
   0,
   0,
   0,
   "Enable dumping of PNG image files."
);

/**
 * @brief Enable running 'lame' tests.
 */
static SProperty Lwogtest_RunLameTests
(
   Lwogtest_Object,
   "RunLameTests",
   0,
   false,
   0,
   0,
   0,
   "Enable running 'lame' tests."
);

/**
 * @brief Equivalent of the "-cmodel" flag in lwogtest.
 */
static SProperty Lwogtest_Cmodel
(
   Lwogtest_Object,
   "Cmodel",
   0,
   false,
   0,
   0,
   0,
   "Equivalent of the '-cmodel' flag in lwogtest."
);

/**
 * @brief Equivalent of the "-nocomment" flag in lwogtest.
 */
static SProperty Lwogtest_NoComment
(
   Lwogtest_Object,
   "NoComment",
   0,
   false,
   0,
   0,
   0,
   "Equivalent of the '-nocomment' flag in lwogtest."
);

/**
 * @brief Equivalent of the "-filter" flag in lwogtest.
 */
static SProperty Lwogtest_Filter
(
   Lwogtest_Object,
   "Filter",
   0,
   false,
   0,
   0,
   0,
   "Equivalent of the '-filter' flag in lwogtest."
);

/**
 * @brief Equivalent of the First component of the "-filter" flag in lwogtest.
 */
static SProperty Lwogtest_FilterFirst
(
   Lwogtest_Object,
   "FilterFirst",
   0,
   0,
   0,
   0,
   0,
   "Equivalent of the First component of the '-filter' flag in lwogtest."
);

/**
 * @brief Equivalent of the Last component of the "-filter" flag in lwogtest.
 */
static SProperty Lwogtest_FilterLast
(
   Lwogtest_Object,
   "FilterLast",
   0,
   0,
   0,
   0,
   0,
   "Equivalent of the Last component of the '-filter' flag in lwogtest."
);

/**
 * @brief Equivalent of the "-oneCell" flag in lwogtest.
 */
static SProperty Lwogtest_OneCell
(
   Lwogtest_Object,
   "OneCell",
   0,
   false,
   0,
   0,
   0,
   "Equivalent of the '-oneCell' flag in lwogtest."
);

/**
 * @brief Equivalent of the X component of the "-oneCell" flag in lwogtest.
 */
static SProperty Lwogtest_OneCellX
(
   Lwogtest_Object,
   "OneCellX",
   0,
   0,
   0,
   0,
   0,
   "Equivalent of the X component of the '-oneCell' flag in lwogtest."
);

/**
 * @brief Equivalent of the Y component of the "-oneCell" flag in lwogtest.
 */
static SProperty Lwogtest_OneCellY
(
   Lwogtest_Object,
   "OneCellY",
   0,
   0,
   0,
   0,
   0,
   "Equivalent of the Y component of the '-oneCell' flag in lwogtest."
);

/**
 * @brief Equivalent of the "-swread" flag in lwogtest.
 */
static SProperty Lwogtest_SWRead
(
   Lwogtest_Object,
   "Swread",
   0,
   false,
   0,
   0,
   0,
   "Equivalent of the '-swread' flag in lwogtest."
);

/**
 * @brief Disable SwapBuffers calls when running double-buffered.
 */
static SProperty Lwogtest_NoSwapBuffers
(
   Lwogtest_Object,
   "NoSwapBuffers",
   0,
   false,
   0,
   0,
   0,
   "Disable SwapBuffers calls when running double-buffered."
);
/**
 * @brief Use a sub-window of the screen when running lwogtests.
 */
static SProperty Lwogtest_UseWindow
(
   Lwogtest_Object,
   "UseWindow",
   0,
   false,
   0,
   0,
   0,
   "Use a sub-window of the screen when running lwogtests."
);
/**
 * @brief Signed offset of the sub-window's upper left corner in X.
 */
static SProperty Lwogtest_WindowX
(
   Lwogtest_Object,
   "WindowX",
   0,
   0,
   0,
   0,
   0,
   "Signed offset of the sub-window's upper left corner in X."
);
/**
 * @brief Signed offset of the sub-window's upper left corner in Y.
 */
static SProperty Lwogtest_WindowY
(
   Lwogtest_Object,
   "WindowY",
   0,
   0,
   0,
   0,
   0,
   "Signed offset of the sub-window's upper left corner in Y."
);
/**
 * @brief Width of the sub-window used for rendering.
 */
static SProperty Lwogtest_WindowW
(
   Lwogtest_Object,
   "WindowW",
   0,
   0,
   0,
   0,
   0,
   "Width of the sub-window used for rendering."
);
/**
 * @brief Height of the sub-window used for rendering.
 */
static SProperty Lwogtest_WindowH
(
   Lwogtest_Object,
   "WindowH",
   0,
   0,
   0,
   0,
   0,
   "Height of the sub-window used for rendering."
);

/**
 * @brief Height of the sub-window used for rendering.
 */
static SProperty Lwogtest_DumpECover
(
   Lwogtest_Object,
   "DumpECover",
   0,
   false,
   0,
   0,
   0,
   "Instruments MGL to trigger dumping of ECover"
);

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------
static SProperty Lwogtest_PathMaskBase
(
   Lwogtest_Object,
   "Base",
   0,
   LWOG_TEST_PATH_MASK_BASE,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: run base path"
);
static SProperty Lwogtest_PathMaskAll
(
   Lwogtest_Object,
   "AllPaths",
   0,
   LWOG_TEST_PATH_MASK_ALL,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: run all paths"
);
static SProperty Lwogtest_PathMaskDlist
(
   Lwogtest_Object,
   "Dlist",
   0,
   LWOG_TEST_PATH_MASK_DLIST,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: run dlist path"
);
static SProperty Lwogtest_PathMaskDlistCE
(
   Lwogtest_Object,
   "DlistCE",
   0,
   LWOG_TEST_PATH_MASK_DLIST_CE,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: run dlist/COMPILE_AND_EXELWTE path"
);
static SProperty Lwogtest_PathMaskArrayElement
(
   Lwogtest_Object,
   "ArrayElement",
   0,
   LWOG_TEST_PATH_MASK_AE,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: run ArrayElement path"
);
static SProperty Lwogtest_PathMaskDrawArrays
(
   Lwogtest_Object,
   "DrawArrays",
   0,
   LWOG_TEST_PATH_MASK_DA,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: run DrawArrays path"
);
static SProperty Lwogtest_PathMaskDrawElements
(
   Lwogtest_Object,
   "DrawElements",
   0,
   LWOG_TEST_PATH_MASK_DE,
   0,
   0,
   JSPROP_READONLY,
   "CONSTANT: run DrawElements path"
);

//------------------------------------------------------------------------------
// Methods and Tests
//------------------------------------------------------------------------------
C_(Lwogtest_RunAll);
static STest Lwogtest_RunAll
(
   Lwogtest_Object,
   "RunAll",
   C_Lwogtest_RunAll,
   0,
   "Run all the lwogtests."
);

C_(Lwogtest_RunTest);
static STest Lwogtest_RunTest
(
   Lwogtest_Object,
   "RunTest",
   C_Lwogtest_RunTest,
   0,
   "Run a single lwogtest."
);

C_(Lwogtest_RunTests);
static STest Lwogtest_RunTests
(
   Lwogtest_Object,
   "RunTests",
   C_Lwogtest_RunTests,
   0,
   "Run one or more lwogtests."
);

C_(Lwogtest_GenList);
static STest Lwogtest_GenList
(
   Lwogtest_Object,
   "GenList",
   C_Lwogtest_GenList,
   0,
   "Generate a list of valid tests."
);

C_(Lwogtest_BindTestDevice);
static STest Lwogtest_BindTestDevice
(
   Lwogtest_Object,
   "BindTestDevice",
   C_Lwogtest_BindTestDevice,
   0,
   "Bind TestDevice DevInst to run with"
);

// STest
C_(Lwogtest_RunAll)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // This is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: Lwogtest.RunAll()");
      return JS_FALSE;
   }

   RETURN_RC(Lwogtest::RunAll());
}

// STest
C_(Lwogtest_RunTest)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 1)
   {
      JS_ReportError(pContext, "Usage: Lwogtest.RunTest(testNameOrNum)");
      return JS_FALSE;
   }

   RETURN_RC(Lwogtest::RunTests(NumArguments, pArguments));
}

// STest
C_(Lwogtest_RunTests)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments == 0)
   {
      JS_ReportError(pContext, "Usage: Lwogtest.RunTests(test1[,test2[,test3...]])");
      return JS_FALSE;
   }

   RETURN_RC(Lwogtest::RunTests(NumArguments, pArguments));
}

// STest
C_(Lwogtest_GenList)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   string FileName;
   JavaScriptPtr pJavaScript;

   // This is a void method.
   if (NumArguments != 1 ||
       (OK != pJavaScript->FromJsval(pArguments[0], &FileName)))
   {
      JS_ReportError(pContext, "Usage: Lwogtest.GenList(FileName)");
      return JS_FALSE;
   }

   RETURN_RC(Lwogtest::GenList(FileName));
}

// STest
C_(Lwogtest_BindTestDevice)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   UINT32 devInst = ~0U;
   if (NumArguments != 1 ||
       (OK != pJavaScript->FromJsval(pArguments[0], &devInst)))
   {
      JS_ReportError(pContext, "Usage: Lwogtest.BindTestDevice(DevInst)");
      return JS_FALSE;
   }

   TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
   TestDevicePtr pDevice;
   if (OK != pTestDeviceMgr->GetDevice(devInst, &pDevice) ||
       !pDevice)
   {
       JS_ReportError(pContext, "Lwogtest.BindTestDevice invalid Devinst = %d", devInst);
       return JS_FALSE;
   }

   Lwogtest::d_pTestDevice = pDevice;

   RETURN_RC(OK);
}


//------------------------------------------------------------------------------
RC Lwogtest::RunAll()
{
    StickyRC firstRc = InnerRunAll();

    d_pTestDevice->GetDisplay()->TurnScreenOff(false);

    firstRc = d_mglTestContext.Cleanup();

    return firstRc;
}

//------------------------------------------------------------------------------
RC Lwogtest::InnerRunAll()
{
    RC rc;

    CHECK_RC(InitProperties());

    if (d_UseWindow)
    {
        mglSetWindow(d_WindowX, d_WindowY,
                     d_WindowWidth, d_WindowHeight);
    }
    CHECK_RC(d_mglTestContext.Setup());

    CHECK_RC(InitDLL());

    for (int i = 0; i < TestStructEntries; i++)
    {
        // Is the test lame, and are we skipping lame tests?
        if ((TestStruct[i].pathMask & (LWOG_TEST_PATH_MASK_LAME |
                                       LWOG_TEST_PATH_MASK_DEBUG_TEST)) &&
            !d_RunLameTests)
        {
            Printf(Tee::PriLow, "Skipping lame lwogtest '%s'.\n",
                   TestStruct[i].name[0]);
            continue;
        }

        // Run the test.
        CHECK_RC( RunOneTest(i) );
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Lwogtest::RunTests(uintN NumArguments, jsval *pArguments)
{
    StickyRC firstRc = InnerRunTests(NumArguments, pArguments);

    d_pTestDevice->GetDisplay()->TurnScreenOff(false);

    firstRc = d_mglTestContext.Cleanup();

    return firstRc;
}

//------------------------------------------------------------------------------
RC Lwogtest::InnerRunTests(uintN NumArguments, jsval *pArguments)
{
    JavaScriptPtr pJavaScript;
    RC rc;
    bool found;
    UINT32 i;
    int j;

    CHECK_RC(InitProperties());

    if (d_UseWindow)
    {
        mglSetWindow(d_WindowX, d_WindowY,
                     d_WindowWidth, d_WindowHeight);
    }
    CHECK_RC(d_mglTestContext.Setup());

    CHECK_RC(InitDLL());

    // For each test specified by the user
    for (i = 0; i < NumArguments; i++)
    {
        string TestName;

        found = false;

        // Search for the test
        pJavaScript->FromJsval(pArguments[i], &TestName);
        for (j = 0; j < TestStructEntries; j++)
        {
            // Is this the test the user asked for?
            if (!strcmp(TestStruct[j].name[0], TestName.c_str()))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            Printf(Tee::PriHigh, "Could not find an lwogtest named '%s'.\n",
                TestName.c_str());
            continue;
        }

        // Run the test
        CHECK_RC( RunOneTest(j) );
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Lwogtest::InitProperties()
{
   RC rc;
   UINT32 tmpI;
   JavaScriptPtr pJs;

   CHECK_RC(d_TstCfg.InitFromJs(Lwogtest_Object));
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_PathMask, &d_PathMask));
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_DoubleBuffer, &tmpI));
   d_DoubleBuffer = (0 != tmpI);
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_DumpTga, &tmpI));
   d_DumpTga = (0 != tmpI);
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_DumpPng, &tmpI));
   d_DumpPng = (0 != tmpI);
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_RunLameTests, &tmpI));
   d_RunLameTests = (0 != tmpI);
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_Cmodel, &tmpI));
   d_Cmodel = (0 != tmpI);
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_NoComment, &tmpI));
   d_NoIntrusiveComment = (0 != tmpI);
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_SWRead, &tmpI));
   d_SWRead = (0 != tmpI);

   CHECK_RC( d_mglTestContext.SetProperties(
           &d_TstCfg,
           d_DoubleBuffer,
           d_pTestDevice->GetInterface<GpuSubdevice>()->GetParentDevice(),
           DisplayMgr::PreferRealDisplay,
           0,           // don't override color format
           false,       // don't force rendering to offscreen frambuffer object
           false,       // renderToSysmem
           0));         // do not use a layered FBO

   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_DumpECover, &tmpI));
   d_mglTestContext.SetDumpECover(tmpI != 0);

   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_OneCell, &tmpI));
   d_OneCell = (0 != tmpI);
   if (d_OneCell)
   {
       CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_OneCellX,
                                 &d_CellX));
       CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_OneCellY,
                                 &d_CellY));
   }

   {
      int first, last;

      CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_Filter, &tmpI));
      if (tmpI)
      {
          CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_FilterFirst,
                                    &first));
          CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_FilterLast,
                                    &last));
          dll_filterInit(first, last);
      }
   }

   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_NoSwapBuffers, &tmpI));
   d_NoSwapBuffers = (0 != tmpI);
   CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_UseWindow, &tmpI));
   if (tmpI)
   {
       d_UseWindow = GL_TRUE;
       CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_WindowX,
                                 &d_WindowX));
       CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_WindowY,
                                 &d_WindowY));
       CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_WindowW,
                                 &d_WindowWidth));
       CHECK_RC(pJs->GetProperty(Lwogtest_Object, Lwogtest_WindowH,
                                 &d_WindowHeight));
   }
   else
   {
       d_WindowWidth = d_TstCfg.SurfaceWidth();
       d_WindowHeight = d_TstCfg.SurfaceHeight();
   }

   return OK;
}

//------------------------------------------------------------------------------
RC Lwogtest::RunOneTest(int i)
{
   // Is the test supported?
   if (!TestStruct[i].isSupportedFunc(TestStruct[i].name[LWOG_TEST_PATH_BASE]))
   {
      Printf(Tee::PriLow, "Lwogtest '%s' not supported on this platform.\n",
             TestStruct[i].name[0]);
      return OK;
   }

   for (int testPath = 0; testPath < LWOG_TEST_PATH_MAX; testPath++)
   {
      *dll_global_p_testPath = testPath;
      if ((d_PathMask & TestStruct[i].pathMask) & (1 << testPath))
      {
         Printf(Tee::PriNormal, "Running lwogtest '%s'... ", TestStruct[i].name[testPath]);

         glPushAttrib(GL_ALL_ATTRIB_BITS);
         glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
         dll_lwSRand(1);
         TestStruct[i].initFunc(1.0f, TestStruct[i].name[LWOG_TEST_PATH_BASE], testPath);

         switch (testPath) {
         case LWOG_TEST_PATH_BASE:
            break;
         case LWOG_TEST_PATH_DLIST:
            glNewList(99, GL_COMPILE);
            break;
         case LWOG_TEST_PATH_DLIST_CE:
            glNewList(99, GL_COMPILE_AND_EXELWTE);
            break;
         }

         TestStruct[i].doFunc(1.0f, TestStruct[i].name[LWOG_TEST_PATH_BASE], testPath);

         switch (testPath) {
         case LWOG_TEST_PATH_BASE:
             break;
         case LWOG_TEST_PATH_DLIST:
             glEndList();
             glCallList(99);
             glDeleteLists(99, 1);
             break;
         case LWOG_TEST_PATH_DLIST_CE:
             glEndList();
             glDeleteLists(99, 1);
             break;
         }

         TestStruct[i].exitFunc(TestStruct[i].name[LWOG_TEST_PATH_BASE], testPath);
         glPopClientAttrib();
         glPopAttrib();

         if (d_DumpTga)
         {
            unsigned int *buf;
            char filename[256];

            buf = new GLuint[d_WindowWidth*d_WindowHeight];
            if (d_SWRead) {
                glEnable(GL_FORCE_SOFTWARE_LW);
            }
            glReadPixels(0, 0, d_WindowWidth, d_WindowHeight,
                         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buf);
            if (d_SWRead) {
                glDisable(GL_FORCE_SOFTWARE_LW);
            }
            sprintf(filename, "%s.tga", TestStruct[i].name[testPath]);
            ImageFile::WriteTga(filename, ColorUtils::A8R8G8B8,
                                buf, d_WindowWidth, d_WindowHeight,
                                d_WindowWidth*sizeof(GLuint), false, true);
            delete[] buf;
         }
         if (d_DumpPng)
         {
            unsigned int *buf;
            char filename[256];

            buf = new GLuint[d_WindowWidth*d_WindowHeight];
            if (d_SWRead) {
                glEnable(GL_FORCE_SOFTWARE_LW);
            }
            glReadPixels(0, 0, d_WindowWidth, d_WindowHeight,
                         GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buf);
            if (d_SWRead) {
                glDisable(GL_FORCE_SOFTWARE_LW);
            }
            sprintf(filename, "%s.png", TestStruct[i].name[testPath]);
            ImageFile::WritePng(filename, ColorUtils::A8R8G8B8,
                                buf, d_WindowWidth, d_WindowHeight,
                                d_WindowWidth*sizeof(GLuint), false, true);
            delete[] buf;
         }

         if (d_DoubleBuffer)
         {
            if (!d_NoSwapBuffers)
            {
               mglSwapBuffers();
            }
         }
         else
         {
            glFlush();
         }
         Printf(Tee::PriNormal, "done.\n");

         GLenum error = glGetError();
         if (error != GL_NO_ERROR)
         {
            Printf(Tee::PriHigh, "Error running lwogtest '%s': %s\n",
                   TestStruct[i].name[testPath], gluErrorString(error));
         }
      }
   }

   return OK;
}

//------------------------------------------------------------------------------
RC Lwogtest::GenList(string FileName)
{
   int j;
   RC rc;
   int LwrTestNum;
   FILE *fp = 0;

   CHECK_RC(InitProperties());

   CHECK_RC(Utility::OpenFile(FileName, &fp, "wb"));

   CHECK_RC(d_mglTestContext.Setup());

   {
      CHECK_RC(InitDLL());

      for (LwrTestNum = 0, j = 0; j < TestStructEntries; j++)
      {
         //count the LwrTestNum because it could be output in the
         //file in the future to run by number instead of by name.
         //and this way the number if callwlated the way it is
         //expected in runOneTest()

         // Is the test supported?
         if (!TestStruct[j].isSupportedFunc(TestStruct[j].name[LWOG_TEST_PATH_BASE]))
         {
            continue;
         }

         // Is the test lame, and are we skipping lame tests?
         if ((TestStruct[j].pathMask & (LWOG_TEST_PATH_MASK_LAME |
                                        LWOG_TEST_PATH_MASK_DEBUG_TEST)) &&
             !d_RunLameTests)
         {
            continue;
         }

         fprintf(fp, "%s\n", TestStruct[j].name[0]);
         LwrTestNum ++;
      }
   }

   d_pTestDevice->GetDisplay()->TurnScreenOff(false);

   // Check for robust channels errors
   RC mrc = d_mglTestContext.Cleanup();
   if (rc == OK)
      rc = mrc;

   return rc;
}
