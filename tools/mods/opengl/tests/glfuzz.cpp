/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2011,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/gpumtest.h"
#include "glstress.h"
#include "core/include/jsonlog.h"

class GLFuzzyTest : public GLStressTest
{
public:
    GLFuzzyTest();
    virtual ~GLFuzzyTest();
    virtual RC Run();
private:
    UINT32 m_AllowedErrorCount;
public:
    SETGET_PROP(AllowedErrorCount, UINT32);
};

//------------------------------------------------------------------------------

GLFuzzyTest::GLFuzzyTest()
{
    SetName("GLFuzzyTest");
    m_AllowedErrorCount = 0;
}

//------------------------------------------------------------------------------
/* virtual */

GLFuzzyTest::~GLFuzzyTest()
{
}
//------------------------------------------------------------------------------
/* virtual */

RC GLFuzzyTest::Run()
{
    RC rc = GLStressTest::Run();

    const UINT64 errCount = GetMemError(0).GetErrorCount();

    GetJsonExit()->SetField("errcount", errCount);
    Printf(Tee::PriNormal, "GLFuzzyTest errcount = %llu\n", errCount);

    if ((errCount <= m_AllowedErrorCount) &&
        (RC::GPU_STRESS_TEST_FAILED == rc))
    {
        rc.Clear();
    }

    return rc;
}

//------------------------------------------------------------------------------
// INHERIT_FULL gives us the standard GpuMemTest JS interface, including Run.
JS_CLASS_INHERIT_FULL(GLFuzzyTest, GpuMemTest,
        "Fuzzy pass/fail version of glstress",
        0);

// Any other JS property (i.e. all the glstress-specific properties)
// are not inherited at the JS level.
// We must dup them here to get them visible to JS.

CLASS_PROP_READWRITE_FULL(GLFuzzyTest, LoopMs, UINT32,
        "How long to run GLFuzzy, in milliseconds.",
        SPROP_VALID_TEST_ARG, 60000);

CLASS_PROP_READWRITE(GLFuzzyTest, TxSize, UINT32,
        "Texture width & height in texels.");
CLASS_PROP_READWRITE(GLFuzzyTest, NumTextures, UINT32,
        "How many different patterns (textures) to try (0 disables texturing).");
CLASS_PROP_READWRITE(GLFuzzyTest, PpV, double,
        "Pixels per vertex (controls primitive size).");
CLASS_PROP_READWRITE(GLFuzzyTest, TxGenQ, bool,
        "Enable txgen for the Q coordinate.");
CLASS_PROP_READWRITE(GLFuzzyTest, NumLights, UINT32,
        "Number of point-source lights to enable (0 == disable lighting).");
CLASS_PROP_READWRITE(GLFuzzyTest, Normals, bool,
        "Send normals per-vertex.");
CLASS_PROP_READWRITE(GLFuzzyTest, Stencil, bool,
        "Enable stencil.");
CLASS_PROP_READWRITE(GLFuzzyTest, Ztest, bool,
        "Enable depth test.");
CLASS_PROP_READWRITE(GLFuzzyTest, TwoTex, bool,
        "Use dual-texturing (tx1 always tx pattern 0).");
CLASS_PROP_READWRITE(GLFuzzyTest, Fog, bool,
        "Enable fog.");
CLASS_PROP_READWRITE(GLFuzzyTest, BoringXform, bool,
        "Use a boring xform (x,y passthru, z=0).");
CLASS_PROP_READWRITE(GLFuzzyTest, DrawLines, bool,
        "Draw GL_LINEs.");
CLASS_PROP_READWRITE(GLFuzzyTest, DumpPngOnError, bool,
        "Dump a .png file of the failing surface.");
CLASS_PROP_READWRITE(GLFuzzyTest, SendW, bool,
        "Send x,y,z,w positions (not just x,y,z).");
CLASS_PROP_READWRITE(GLFuzzyTest, FragProg, string,
        "Fragment program (if empty, use traditional GL fragment processing).");
CLASS_PROP_READWRITE(GLFuzzyTest, VtxProg, string,
        "Vertex program (if empty, use traditional GL vertex processing).");
CLASS_PROP_READWRITE(GLFuzzyTest, ForceErrorCount, UINT32,
       "Force this many errors, to test reporting.");
CLASS_PROP_READWRITE(GLFuzzyTest, DumpFrames, UINT32,
        "Number of frames to dump images of (default 0).");
CLASS_PROP_READWRITE(GLFuzzyTest, UseTessellation, bool,
        "Use the GL_LW_tessellation_program5 extension (default = false).");
CLASS_PROP_READONLY(GLFuzzyTest, Frames, double,
       "Frames completed in last Run().");
CLASS_PROP_READONLY(GLFuzzyTest, Pixels, double,
       "Pixels drawn in last Run().");
CLASS_PROP_READONLY(GLFuzzyTest, ElapsedGpu, double,
       "Elapsed Gpu time(seconds) during drawing in last Run().");
CLASS_PROP_READONLY(GLFuzzyTest, Fps, double,
       "Frames per second in last Run().");
CLASS_PROP_READONLY(GLFuzzyTest, Pps, double,
       "Pixels per second in last Run().");
JS_STEST_LWSTOM(GLFuzzyTest, SetProgParams, 3,
        "Set TessCtrl/TessEval/Vtx/Frag program constants.")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    GLFuzzyTest *pTest = (GLFuzzyTest *)JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;
    string     pgmType;
    UINT32     idx;
    JsArray    vals;

    if ( (NumArguments != 3) ||
         (OK != pJavaScript->FromJsval(pArguments[0], &pgmType)) ||
         (OK != pJavaScript->FromJsval(pArguments[1], &idx)) ||
         (OK != pJavaScript->FromJsval(pArguments[2], &vals)) )
    {
        JS_ReportError(pContext,
                       "Usage: GLFuzzyTest.SetProgParams(\"Vtx\", idx, [vals])\n");
        return JS_FALSE;
    }

    RETURN_RC(pTest->SetProgParams(pgmType, idx, vals));
}

CLASS_PROP_READWRITE(GLFuzzyTest, AllowedErrorCount, UINT32,
        "Number of pixel errors to ignore (default = 0).");
