/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// The diagrams in the test are derived from
// https://wiki.lwpu.com/engwiki/index.php/Path_rendering examples
// The glyph examples are copied with gratitude from Mark Kilgard's tutorial

//------------------------------------------------------------------------------
#include "opengl/modsgl.h"
#include "lwprhelper.h"
#include "core/include/massert.h"
#include "gpu/tests/gputest.h"
#include "core/tests/testconf.h"
#include "core/include/golden.h"
#include "opengl/glgoldensurfaces.h"
#include "gpu/js/fpk_comm.h"
#include "core/include/utility.h"
#include "lwprhelper.h"
#include "core/include/display.h"
#include "opengl/glgpugoldensurfaces.h"
#include "gpu/include/gpusbdev.h"
#include "lwdiagutils.h"
#include <math.h>
#include <map>

using namespace ColourHelper;
namespace
{
    // Choose a background colour
    void ClearAndSetBackgroundColour(UINT32 colourIdx)
    {
        if (colourIdx > BG_MIX1)
        {
            Printf(Tee::PriHigh, "chosen Colour is out of range. Using colour Black\n");
            colourIdx = BG_BLACK;
        }
        glClearColor(s_ColourTable[colourIdx].r,
                s_ColourTable[colourIdx].g,
                s_ColourTable[colourIdx].b,
                s_ColourTable[colourIdx].a);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    //glyph messages
    string LWPRTexts[] = {"GRAPHICS", "ABCDEFGHIJKL", "SCIHPARG",};
    const GLfloat LWPRTextAspectRatio = 1.77f;

    Coordpair GetNextPointOnSpiral(float t, const float a)
    {
        //With big values of "a" more dense points can be obtained.
        //"t" is the parameter that can be incremented to get next point.
        float x = t * (LwDiagUtils::Cos (a*t));
        float y = t * (LwDiagUtils::Sin (a*t));
        Coordpair cP(x, y);
        return cP;
    }
    void CreateCoords(
            Coordpair cp[], UINT32 maxCoords, const float delta, const float a,
            const float transx = 0.0, const float transy = 0.0)
    {
        //Draw a figure. Start from origin and move outward
        float t = 0.0f;
        for (UINT32 ii=0; ii < maxCoords; ii++, t+=delta)
        {
            float x = t * (LwDiagUtils::Cos (a*t)) + transx;
            float y = t * (LwDiagUtils::Sin (a*t)) + transy;
            cp[ii].Assign(x, y);
        }
        return;
    }

    UINT32 PrepareSqGrid (
            const Coordpair min, const Coordpair max, const int rows, const int cols,
            Coordpair cp[], const UINT32 maxCoords)
    {
        GLfloat xDelta = (max.X() - min.X()) / (cols -1);
        GLfloat yDelta = (max.Y() - min.Y()) / (rows -1);
        UINT32 ii = 0;
        for (GLfloat x = min.X(); x < max.X(); x += xDelta)
        {
            for(GLfloat y =min.Y(); y < max.Y(); y += yDelta)
            {
                cp[ii].Assign(x, y);
                ii++;
                if(ii >= maxCoords)
                    break;
            }
        }
        return ii;
    }
    //Simple 3triangles MISC1
    const GLuint s_NumVertices = 9;
    const UINT32 vPosition = 0;
    enum VAO_ID {TRIANGLES, NUMVAOS};
    enum Buffer_IDs {ARRAYBUFFER, NUMBUFFERIDS };
    GLuint s_Vaos[NUMVAOS];
    GLuint s_Buffers[NUMBUFFERIDS];
    GLfloat s_Vert[s_NumVertices][2] =
    {
        //Clip coords
        {-0.9f, -0.9f}, //Tri0
        {-0.5f, -0.9f},
        {-0.9f, 0.8f},
        {-0.8f, 0.9f}, //Tri1
        {0.8f, 0.9f},
        {0.0f, -0.8f},
        {0.5f, -0.9f}, //Tri2
        {0.9f, -0.9f},
        {0.9f, 0.8f}
    };

    //Divide 1 square in 2 triangle paths with diagonal as shared edge
    void DivideCell(SimplifyPathCommands *p0, SimplifyPathCommands *p1,
            Coordpair mincp, Coordpair maxcp)
    {
        p0->ClearPathCommands();
        p1->ClearPathCommands();
        p0->cmdM(mincp);
        p0->cmdL(maxcp.X(), mincp.Y());
        p0->cmdL(mincp.X(), maxcp.Y());
        p0->cmdZ();
        p1->cmdM(maxcp.X(), mincp.Y());
        p1->cmdL(maxcp);
        p1->cmdL(mincp.X(), maxcp.Y());
        p1->cmdZ();
        return;
    }
    SimplifyPathCommands s_SharedPaths[FPK_FIG_SHARED_MAX_PATH - FPK_FIG_SHARED_MIN_PATH + 1];
}

//------------------------------------------------------------------------------
class PathRenderingTest : public  GpuTest
{
    public:
        SETGET_PROP(CheckTIR,      bool);
        SETGET_PROP(CheckAA,       bool);
        SETGET_PROP(Checkfillrect, bool);
        SETGET_PROP(CheckS8,       bool);
        SETGET_PROP(CoverFill,     bool);
        SETGET_PROP(PathStroke,    bool);
        SETGET_PROP(SharedBottom,  FLOAT32);
        SETGET_PROP(SharedTop,     FLOAT32);
        SETGET_PROP(SharedLeft,    FLOAT32);
        SETGET_PROP(SharedRight,   FLOAT32);
        SETGET_PROP(ForceFbo,      bool);
        PathRenderingTest();
        bool IsSupported();
        bool IsSupportedVf();
        RC Setup();
        RC Cleanup();
        RC Run();
        /*virtual*/ RC SetDefaultsForPicker(UINT32 idx);

    private:
        mglTestContext       m_mglTestContext;
        TestConfiguration    *m_pTstCfg;       // standard test options

        RC GoldenRun(UINT32 figureNum);
        Goldelwalues        *m_pGolden;
        GLGoldenSurfaces    m_GSurfs;
        // Pick repeatability checking.
        //
        // We can log CRC's of our internal random picks and other generated
        // data (i.e. textures, coordinates,etc) to verify that our randomizer
        // is repeatable.
        //
        // This is very handy when the rendered images aren't repeatable and
        // we need to track down the cause (mods vs. GL driver vs. hardware...).
        //
        GoldenSelfCheck     m_GSelfCheck;
        void LogCoordPairs(UINT32 id);

        //Callwlate number of loops and other Run related variables in prerun
        //Restore the gl state post run
        RC PreRunState(UINT32 *ploopsPerSample);
        RC FindSupportedExtensions();
        void PrintJsProperties(Tee::Priority pri);
        RC SetupSimplePathSet();
        RC SetuplwprGlyphs();
        RC SetupFillScreen();
        RC SetContourPath1();
        RC SetContourPath2();
        RC SetContourPath3();
        void GenSharedPathIds();
        RC SetSharedEdges(const UINT32 maxrow, const UINT32 maxcol);
        RC DrawSimplePathSet(int FigId);
        RC DrawlwprGlyphs();
        RC DrawSharedEdges(const UINT32 maxrow, const UINT32 maxcol);
        RC SetupTriangles();
        RC DrawTriangles();
        RC CleanupTriangles();
        RC InnerDraw(UINT32 figureNum, UINT32 loopnum);

        //Shader management
        //Shader objects
        GLuint m_GrProgramId;
        class TestShaders
        {
            public:
                string vsh; //vertex
                string fsh; //fragment
        };
        enum ShaderId{SHADER0, NUMSHADERS};
        //Track all shaders and choose 1 from fancypicker
        TestShaders m_ListShaders[NUMSHADERS];
        GLuint CompileShader(GLenum shaderType, const GLchar * src);
        RC AttachShader(GLenum shaderType, const GLchar * src);
        RC InitShaders(UINT32 id);
        RC DetachShaders();

        //Fbo management
        RC CreateFbo(GLenum colourfmt, GLenum checkS8,
                GLint csamples, GLint dsamples, bool renderBufferTarget,
                GLuint *pfboid, GLuint *prboid, GLuint *pstnclid,
                int wd, int ht);
        RC DestroyFbo(GLuint *pfboid, GLuint *prboid, GLuint *pstnclid,
                bool renderBufferTarget);
        RC BlitFbo(const GLint fromFB, const GLint toFb,
                GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
                GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                GLbitfield msk, GLenum filter);
        void PrintRBParameters(GLint id);
        void PrintTexParameters(GLint id);
        void PrintFbo(); //If Verbose is true
        RC SetupRbo();
        RC CleanupRbo();

        FancyPickerArray       *m_pPickers; //Figure
        FancyPicker::FpContext *m_pFpCtx;   //Loop

        //800, 12000 are arbitrarily large number of control points.
        //More than required coordinates are generated to have clipping effects too.
        Coordpair             m_CoordSet[800];
        Coordpair             m_ScrFillCoordSet[15000];
        Coordpair             m_GridCoords[5000];
        UINT32                m_GridSize;

        INT32                 m_DisplayWidth;      //in pixels
        INT32                 m_DisplayHeight;     //in pixels
        ColorUtils::Format    m_ColorSurfaceFormat;
        GLenum                m_SurfFormat;
        pair<UINT32, UINT32>  m_ColourvsDepth[10];
        bool                  m_UseRbo; //Target Renderbuffer
        enum                  {COLOURBUFF, STENCILBUFF, NUMRDBUFFERTYPES};
        GLuint                m_RBuffs[NUMRDBUFFERTYPES];
        GLuint                m_Fboid; //Framebuffer object

        //Test inputs
        bool               m_CheckTIR; //TIR is target independent rasterization
        bool               m_CheckAA;  //Antialiasing test (not mixed samples)
        bool               m_Checkfillrect; //Fill Rectangle in polygon mode
        bool               m_CheckS8;  //S8 format and includes stencil compression
                                       //Does not require any other extension
        bool               m_CoverFill;
        bool               m_PathStroke;
        bool               m_SharedEdgeIsSupported;
        GLuint             m_Csamples, m_Zsamples;
        GLfloat            m_SharedRight, m_SharedLeft;
        GLfloat            m_SharedTop, m_SharedBottom;
        const UINT32       m_ColourFmt;  //GL_RGBA8 is the format to be tested
        SimplifyPathCommands  m_PathHelpers[FPK_FIG_TOTALPATHS];
        SimplifyGlyphCommands m_GlyphHelpers[FPK_FIG_TOTALGLYPHS];
        UINT32             m_VerbosePrint;
        UINT32             m_LogPath; //Print level for path commands
        bool               m_mglContextIsSetup;
        bool               m_ForceFbo;
        map<UINT32, string>m_FigIdVsName;

        enum eCoordPairIds
        {
            CoordSet = 0,
            SrcFillCoordSet = 1,
            GridCoords = 2,
            NumCoordPairIDs
        };
};

JS_CLASS_INHERIT(PathRenderingTest, GpuTest, "GL Pathrendering Test");
CLASS_PROP_READWRITE(PathRenderingTest, CheckTIR, bool, "Enable TIR test");
CLASS_PROP_READWRITE(PathRenderingTest, CheckAA, bool, "Enable raster sampling test");
CLASS_PROP_READWRITE(PathRenderingTest, Checkfillrect, bool, "Enable Fill Rectangle test");
CLASS_PROP_READWRITE(PathRenderingTest, CheckS8, bool, "Surface Depth format");
CLASS_PROP_READWRITE(PathRenderingTest, CoverFill, bool, "Cover fill enabled.");
CLASS_PROP_READWRITE(PathRenderingTest, PathStroke, bool, "Path stroke enabled.");
CLASS_PROP_READWRITE(PathRenderingTest, ForceFbo, bool, "Force test to use FBOs.");

PathRenderingTest::PathRenderingTest()
    : m_ColourFmt(GL_RGBA8),
      m_VerbosePrint(Tee::PriLow)
{
    SetName("PathRenderingTest");
    CreateFancyPickerArray(LWPR_FIG_NUMPICKERS);
    m_pPickers = GetFancyPickerArray();
    m_pFpCtx = GetFpContext();
    m_pTstCfg = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
    m_GridSize = 0;
    //Use Renderbuffers for generating multisample Colour and Stencil targets
    //This is useful for checking MsAA test, Stencil tests, TIR (Target independent rasterization)
    m_UseRbo         = false;
    //For Testing Target independent rasterization (test 289) on GM20x+ chips
    m_CheckTIR       = false;
    m_CheckAA        = false;
    m_Checkfillrect  = false;
    m_CheckS8        = false;
    m_CoverFill      = true;
    m_PathStroke     = true;
    m_LogPath = Tee::PriNone;
    m_SharedEdgeIsSupported   = false; //extension GL_LW_path_rendering_shared_edge
    /*
       -------------------------------------------
       test num->   | 176 | 286 | 287 | 288 | 289|
       -------------------------------------------
                    |     |     |     |     |    |
       UseRbo       | 0   | 0   | 1   | 1   | 1  |
       -------------------------------------------
       checkTIR     | 0   | 0   | 0   | 0   | 1  |
       -------------------------------------------
       checkfillrect| 0   | 1   | 0   | 0   | 0  |
       -------------------------------------------
       checkS8      | 0   | 0   | 1   | 1   | 0  |
       -------------------------------------------
       checkAA      | 0   | 0   | 1   | 1   | 0  |
       -------------------------------------------
     */

#define CVsDepth(m, n) m_ColourvsDepth[FPK_MSAA_##m##x##n] = make_pair(m, n)
    CVsDepth(2, 2);
    CVsDepth(4, 4);
    CVsDepth(8, 8);
    CVsDepth(16, 16);
    CVsDepth(2, 4);
    CVsDepth(2, 8);
    CVsDepth(2, 16);
    CVsDepth(4, 8);
    CVsDepth(4, 16);
    CVsDepth(8, 16);

    m_Csamples = 0;
    m_Zsamples = 0;
    m_SharedLeft = 0.0f;
    m_SharedRight = 100.0f;
    m_SharedTop = 50.0f;
    m_SharedBottom = 0.0f;
    m_mglContextIsSetup = false;
    m_ForceFbo = false;

    // Initialize the OpenGL stuff
    m_PathHelpers[FPK_FIG_SIMPLEPATH].SetPathid(FPK_FIG_SIMPLEPATH + 1);
    m_PathHelpers[FPK_FIG_CONTOUR1].SetPathid(FPK_FIG_CONTOUR1 + 1);
    m_PathHelpers[FPK_FIG_CONTOUR2].SetPathid(FPK_FIG_CONTOUR2 + 1);
    m_PathHelpers[FPK_FIG_CONTOUR3].SetPathid(FPK_FIG_CONTOUR3 + 1);
    m_PathHelpers[FPK_FIG_FILLPOINTS].SetPathid(FPK_FIG_FILLPOINTS + 1);

    //Shader source
    m_ListShaders[SHADER0].vsh = "#version 400\n"
        "\n"
        "layout (location = 0) in vec4 vPos;\n"
        "void main()\n"
        "{\n"
        "    vec4 v = vec4(vPos);"
        "    gl_Position =  v;\n"
        "}";
    m_ListShaders[SHADER0].fsh = "#version 400\n"
        "out vec4 color;\n"
        "void main()\n"
        "{\n"
        "    color = vec4(.0, .5, .5 , 1.0);\n"
        "}";

    /*Fill map FigureId vs Name for print assistance*/
#define ADDFIGURE(name) m_FigIdVsName[FPK_FIG_##name] = #name
    ADDFIGURE(SIMPLEPATH);
    ADDFIGURE(CONTOUR1);
    ADDFIGURE(CONTOUR2);
    ADDFIGURE(CONTOUR3);
    ADDFIGURE(FILLPOINTS);
    ADDFIGURE(MIXED_CONTOURS);
    ADDFIGURE(SHARED_EDGE);
    ADDFIGURE(GLYPH0); //With gradient
    ADDFIGURE(GLYPH1); //Without gradient
    ADDFIGURE(MISC0);
    ADDFIGURE(MISC1);  //A Simple triangle to see the coverage area
}

void PathRenderingTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    // Print the options we are going with:

    const char * TF[2] = {"false", "true"};
    Printf(pri, "PathRendering controls:\n");
    Printf(pri, "\t%-32s %s\n",  "CheckTIR:",       TF [m_CheckTIR]);
    Printf(pri, "\t%-32s %s\n",  "CheckAA:",        TF [m_CheckAA]);
    Printf(pri, "\t%-32s %s\n",  "Checkfillrect:",  TF [m_Checkfillrect]);
    Printf(pri, "\t%-32s %s\n",  "CheckS8:",        TF [m_CheckS8]);
    Printf(pri, "\t%-32s %s\n",  "PathStroke:",     TF [m_PathStroke]);
    Printf(pri, "\t%-32s %s\n",  "ForceFbo:",       TF [m_ForceFbo]);
    Printf(pri, "\t%-32s %f\n",  "SharedTop:",      m_SharedTop);
    Printf(pri, "\t%-32s %f\n",  "SharedLeft:",     m_SharedLeft);
    Printf(pri, "\t%-32s %f\n",  "SharedBottom:",   m_SharedBottom);
    Printf(pri, "\t%-32s %f\n",  "SharedRight:",    m_SharedRight);
}

//Standard test functions
bool PathRenderingTest::IsSupported()
{
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_LWPR))
    {
        return false;
    }
    string testName = GetName();
    if (testName == "MsAAPR")
    {
        if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_16xMsAA))
            return false;
        if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_S8_FORMAT))
            return false;
    }
    else if (testName == "FillRectangle")
    {
        if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_FILLRECT))
            return false;
    }
    else if (testName == "GLPRTir")
    {
        if (!pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_TIR))
            return false;
    }

    if (!mglHwIsSupported(GetBoundGpuSubdevice()))
       return false;
    if (OK != m_mglTestContext.SetProperties(
            GetTestConfiguration(),
            false, //m_DoubleBuffer,
            GetBoundGpuDevice(),
            GetDispMgrReqs(),
            m_ColourFmt,
            m_ForceFbo, //  use/dont use Fbo provided by mglutils
            false, // Do not render FBOs to sysmem
            0))    // Do not use layered FBOs
    {
        return false;
    }
    if (!m_mglTestContext.IsSupported())
    {
        return false;
    }
    return true;
}

bool PathRenderingTest::IsSupportedVf()
{
    return !(GetBoundGpuSubdevice()->IsSMCEnabled());
}

//------------------------------------------------------------------------------
//! \brief Test.Setup function calls FindSupported Extensions to check if we can test
//         desired extension or not.
RC PathRenderingTest::FindSupportedExtensions()
{
    // Check which extensions are actually supported.
    // Features that must be supported for running tests 176, 286, 287, 288, 289
    // GL_LW_path_rendering, GL_EXT_raster_multisample, GL_LW_fill_rectangle
    // There are other extensions which are expected to be supported on GM20x chips
    // and when supported can render different image.
    // eg GL_LW_framebuffer_mixed_samples, GL_LW_path_rendering_shared_edge
    RC rc;
    if (!mglExtensionSupported("GL_LW_path_rendering"))
    {
        Printf(Tee::PriHigh, "LW PR extension is expected to be supported, but it is not.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (m_CheckTIR && !mglExtensionSupported("GL_EXT_raster_multisample"))
    {//Can't Run Test 289
        Printf(Tee::PriHigh, "EXT_raster_multisample not supported. We can't test Tir feature\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (m_CheckTIR && (!mglExtensionSupported ("GL_LW_framebuffer_mixed_samples")))
    {//Can't Run Test 289
        Printf(Tee::PriHigh, "GL_LW_framebuffer_mixed_samples is not supported. We can't test Tir\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (m_Checkfillrect && !mglExtensionSupported("GL_LW_fill_rectangle"))
    {//Can't Run Test 286
        Printf(Tee::PriHigh, "We can't test GL_LW_fill_rectangle as it is not supported.\n");
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!mglExtensionSupported("GL_LW_path_rendering_shared_edge"))
    {//A part of test 176 and all the dependent tests
        Printf(m_VerbosePrint, "GL_LW_path_rendering_shared_edge not supported. This may cause CRC miscompare.\n");
        m_SharedEdgeIsSupported = false;
    }
    else
    {
        m_SharedEdgeIsSupported = true;
    }
    return rc;
}
//------------------------------------------------------------------------------
RC PathRenderingTest::Setup()
{
    StickyRC rc;
    CHECK_RC(GpuTest::Setup());

    m_VerbosePrint  = GetVerbosePrintPri();
    m_DisplayWidth  = m_pTstCfg->SurfaceWidth();
    m_DisplayHeight = m_pTstCfg->SurfaceHeight();
    m_pFpCtx->Rand.SeedRandom(0);

    CHECK_RC(m_mglTestContext.Setup());
    Display *pDisplay = m_mglTestContext.GetDisplay();
    m_SurfFormat = m_mglTestContext.ColorSurfaceFormat();
    m_ColorSurfaceFormat = mglTexFmt2Color(m_SurfFormat);
    m_mglContextIsSetup = true;
    CHECK_RC(FindSupportedExtensions());

    m_GSelfCheck.Init("LWPR_", GetGoldelwalues());
    m_GSelfCheck.SetLogMode(((*m_pPickers)[LWPR_LOG_MODE].Pick()));
    UINT32 numEntries = (m_Checkfillrect) ? NumCoordPairIDs : NumCoordPairIDs-1;
    m_GSelfCheck.LogBegin(numEntries);

    // Setting up paths here in setup is disabled.
    // It is observed that this should be done before drawing otherwise the path ids are ilwalidated.
    //Generate Set of coordinates to be used as control points for Path Rendering
    // These coordinates are chosen for adjustment, the projection window is also
    // based upon the values generated here
    UINT32 maxCoords = static_cast<UINT32>(NUMELEMS(m_CoordSet));
    float delta = 1.0, a = 2000;
    CreateCoords(m_CoordSet, maxCoords, delta, a);
    LogCoordPairs(CoordSet);

    maxCoords = static_cast<UINT32>(NUMELEMS(m_ScrFillCoordSet));
    delta = 0.5;    //More closer coordinates
    a = 15000.0; //More Dense figure, will generate more intersecting points
    CreateCoords(m_ScrFillCoordSet, maxCoords, delta, a);
    LogCoordPairs(SrcFillCoordSet);

    if (m_CheckAA || m_CheckTIR)
    {
        m_UseRbo = true; //Need Fbo to test rasterizer
        m_CheckS8 = true; //Use depth buffer
    }
    if (m_Checkfillrect)
    {
        maxCoords = static_cast<UINT32>(NUMELEMS(m_GridCoords));
        const Coordpair minCp(-1, -1);
        const Coordpair maxCp(1, 1);
        UINT32 rows = 50;
        m_GridSize = PrepareSqGrid(minCp, maxCp, rows, rows, m_GridCoords, maxCoords);
        LogCoordPairs(GridCoords);
    }
    CHECK_RC(m_GSelfCheck.LogFinish("CoordPairs",GetTestConfiguration()->Seed()));

    m_GSurfs.SetCalcAlgorithmHint(GetGoldelwalues()->GetCalcAlgorithmHint());
    m_GSurfs.SetGoldensBinSize(GetGoldelwalues()->GetSurfChkOnGpuBinSize());

    ColorUtils::Format clr = mglTexFmt2Color(m_ColourFmt);
    m_GSurfs.DescribeSurfaces(
            m_DisplayWidth,
            m_DisplayHeight,
            clr,
            ColorUtils::LWFMT_NONE, //We need CRC of Blit surface on stencil tested colour attachment
            pDisplay->Selected());
    m_pGolden->SetSurfaces(&m_GSurfs);

    // Create compute shader and surface for checksum on GPU
    CHECK_RC(m_GSurfs.CreateSurfCheckShader());
    if (m_ForceFbo && m_pTstCfg->Verbose())
    {
        PrintFbo();
    }
    return (rc);
}

//------------------------------------------------------------------------------
RC PathRenderingTest::GoldenRun(UINT32 figureNum)
{
    RC rc;
    if (m_CheckAA || m_CheckTIR)
    {
        CHECK_RC(BlitFbo(m_Fboid, 0,
                    0, 0, m_DisplayWidth, m_DisplayHeight,
                    0, 0, m_DisplayWidth, m_DisplayHeight,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST));
        m_GSurfs.SetReadPixelsBuffer(GL_FRONT);
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, 0);
        rc = m_pGolden->Run();
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, m_Fboid);
    }
    else
    {
        //We are rendering offscreen.
        if (m_mglTestContext.UsingFbo())
        {
            m_GSurfs.SetReadPixelsBuffer(GL_COLOR_ATTACHMENT0_EXT);
        }
        else
        {
            m_GSurfs.SetReadPixelsBuffer(GL_FRONT);
        }
        rc = m_pGolden->Run();
    }
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Error: Path Rendering failed for Figure [%d]\n",
                figureNum);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PathRenderingTest::PreRunState(UINT32 *ploopsPerSample)
{
    StickyRC rc;
    UINT32 verbose = (*m_pPickers)[LWPR_TEST_VERBOSITY].Pick();
    if (verbose == LWPR_PATH_ONSCREEN)
    {
        m_LogPath = Tee::PriHigh;
    }
    else if (verbose == LWPR_PATH_INLOGFILE)
    {
        m_LogPath = Tee::PriLow;
    }
    else
    {
        m_LogPath = Tee::PriNone;
    }
    if (m_CheckAA)
    {
        //If checkTIR is true, that means we are running test 289
        //If only checkAA is true, that means we are running test 287 or test 288
        *ploopsPerSample = m_CheckTIR ? NUM_TIR_SAMPLES: NUM_MSAA_SETS;
    }
    else if (m_CheckTIR)
    {
        //Each Loop should contain NUM_TIR_SAMPLES iterations
        *ploopsPerSample = NUM_TIR_SAMPLES;
    }
    else
    {
        //No need of use Render buffer. Ensure it is false
        MASSERT(m_UseRbo == false);
        *ploopsPerSample = 1;
    }
    //Setup
    CHECK_RC(SetupSimplePathSet()); //Helloworld of 2d path, lwrves and straight lines
    CHECK_RC(SetContourPath1());    //Straight line joints
    CHECK_RC(SetContourPath2());    //Cubic joins
    CHECK_RC(SetContourPath3());    //Quad joins
    CHECK_RC(SetupFillScreen());    //completely fill the view with lines
    UINT32 rcMax = FPK_FIG_SHARED_ROWCOL_MAX;
    CHECK_RC(SetSharedEdges(rcMax, rcMax));
    m_pPickers->CheckInitialized();
    //Do this once because we are not using skipcount in test
    UINT32 seed = m_pTstCfg->Seed();
    m_pFpCtx->Rand.SeedRandom(seed);
    return rc;
}

//------------------------------------------------------------------------------
RC PathRenderingTest::InnerDraw(UINT32 figureNum, UINT32 loopNum)
{
    SimplifyPathCommands *pFig = nullptr;
    StickyRC rc;
    glDisable(GL_STENCIL_TEST);
    ClearAndSetBackgroundColour(BG_BLACK);

    // Reset matrices
    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glMatrixLoadIdentityEXT(GL_MODELVIEW);

    if (m_FigIdVsName.find(figureNum) == m_FigIdVsName.end())
    {
        Printf(Tee::PriNormal, "Invalid Pick: %u\n", figureNum);
        return RC::SOFTWARE_ERROR;
    }

    Printf(m_VerbosePrint, "Draw %s\n", m_FigIdVsName[figureNum].c_str());
    switch (figureNum)
    {
        case FPK_FIG_SIMPLEPATH:
            pFig = &m_PathHelpers[figureNum];
            CHECK_RC(SetupSimplePathSet()); //Helloworld of 2d path, lwrves and straight lines
            ClearAndSetBackgroundColour(BG_GREEN);
            CHECK_RC(pFig->SubmitPath(m_LogPath));
            //Set style for Stroke width
            pFig->SelectPathStyle(PathStyle::CAP_ROUND, PathStyle::JOIN_ROUND, 3.0);
            CHECK_RC(pFig->SubmitPathStyleSettings());
            rc = DrawSimplePathSet(FPK_FIG_SIMPLEPATH + 1);
            break;

        case FPK_FIG_CONTOUR1:
            pFig = &m_PathHelpers[figureNum];
            CHECK_RC(pFig->SubmitPath(m_LogPath));
            //Set style
            pFig->SelectPathStyle(PathStyle::CAP_TRIANGLE, PathStyle::JOIN_ROUND, 2);
            CHECK_RC(pFig->SubmitPathStyleSettings());
            rc = pFig->cmdDrawPath(m_CoverFill, m_PathStroke);
            break;

        case FPK_FIG_CONTOUR2:
            pFig = &m_PathHelpers[figureNum];
            CHECK_RC(pFig->SubmitPath(m_LogPath));
            pFig->SetPathColor(BG_RED);
            //Set style, ending, capping, stroke width
            pFig->SelectPathStyle(PathStyle::CAP_TRIANGLE, PathStyle::JOIN_ROUND, 3);
            CHECK_RC(pFig->SubmitPathStyleSettings());
            rc = pFig->cmdDrawPath(m_CoverFill, m_PathStroke);
            break;

        case FPK_FIG_CONTOUR3:
            pFig = &m_PathHelpers[figureNum];
            CHECK_RC(pFig->SubmitPath(m_LogPath));
            //Set style
            pFig->SelectPathStyle(PathStyle::CAP_ROUND, PathStyle::JOIN_ROUND, .5);
            CHECK_RC(pFig->SubmitPathStyleSettings());
            rc = pFig->cmdDrawPath(m_CoverFill, m_PathStroke);
            break;

        case FPK_FIG_FILLPOINTS:
            pFig = &m_PathHelpers[figureNum];
            CHECK_RC(pFig->SubmitPath(m_LogPath));
            //Set style
            pFig->SelectPathStyle(PathStyle::CAP_TRIANGLE, PathStyle::JOIN_ROUND, 1);
            CHECK_RC(pFig->SubmitPathStyleSettings());
            rc = pFig->cmdDrawPath(m_CoverFill, m_PathStroke);
            break;

        case FPK_FIG_MIXED_CONTOURS:
            {
                SimplifyPathCommands *p1 = &m_PathHelpers[FPK_FIG_CONTOUR1];
                SimplifyPathCommands *p2 = &m_PathHelpers[FPK_FIG_CONTOUR2];
                SimplifyPathCommands *p3 = &m_PathHelpers[FPK_FIG_CONTOUR3];
                p1->SelectPathStyle(PathStyle::CAP_TRIANGLE, PathStyle::JOIN_ROUND, 2);
                p2->SelectPathStyle(PathStyle::CAP_TRIANGLE, PathStyle::JOIN_ROUND, 3);
                p3->SelectPathStyle(PathStyle::CAP_ROUND, PathStyle::JOIN_ROUND, .5);
                CHECK_RC(p1->SubmitPath(m_LogPath));
                CHECK_RC(p2->SubmitPath(m_LogPath));
                CHECK_RC(p3->SubmitPath(m_LogPath));
                CHECK_RC(p1->SubmitPathStyleSettings());
                CHECK_RC(p2->SubmitPathStyleSettings());
                CHECK_RC(p3->SubmitPathStyleSettings());
                rc = p1->cmdDrawPath(false, true);
                rc = p2->cmdDrawPath(true, false);
                rc = p3->cmdDrawPath(true, true);
            }
            break;

        case FPK_FIG_SHARED_EDGE:
            if (!m_SharedEdgeIsSupported)
            {
                Printf(m_VerbosePrint, "Shared edge Extension is not supported\n");
                Printf(m_VerbosePrint, "This will lead to shared edge colour artifact\n");
            }
            {
                //hard-coding Row, Column maximum value knowingly
                UINT32 rcMax = FPK_FIG_SHARED_ROWCOL_MAX;
                size_t numPaths = MIN((rcMax * rcMax), NUMELEMS(s_SharedPaths));
                for(size_t ii = 0; ii < numPaths; ii++)
                {
                    SimplifyPathCommands *pFig = &s_SharedPaths[ii];
                    CHECK_RC(pFig->SubmitPath(m_LogPath));
                    CHECK_RC(pFig->SubmitPathStyleSettings());
                }
                rc = DrawSharedEdges(rcMax, rcMax);
            }
            break;

        case FPK_FIG_GLYPH0:
            CHECK_RC(SetuplwprGlyphs());    //Setup characters
            for (int degree = 0; degree < 360; degree += 5)
            {
                SimplifyGlyphCommands &glyphset =
                    m_GlyphHelpers[FPK_FIG_GLYPH0 % FPK_FIG_TOTALGLYPHS];
                glyphset.SetZRotation(1.0f * degree);
                glyphset.UseColourGradient();
                glyphset.Drawglyphs();
            }
            break;

        case FPK_FIG_GLYPH1:
            // Glyph paths are overflowing sometimes in OGL driver during cleanup.
            // Do not render this figure in SLT/MFG
            if (GetBoundGpuSubdevice()->HasBug(200052356))
                break;
            //Draw 2 diagrams overlapping partially with 1 diagram being coloured
            //according to gradient.
            ClearAndSetBackgroundColour(BG_WHITE);
            CHECK_RC(SetuplwprGlyphs());    //Setup characters
            for (int degree = 0; degree < 180; degree += 10)
            {
                SimplifyGlyphCommands &glyphset0 =
                    m_GlyphHelpers[FPK_FIG_GLYPH1  % FPK_FIG_TOTALGLYPHS];
                glyphset0.SetAspectRatio(3);
                glyphset0.SetZRotation(1.0f * degree);
                glyphset0.DisableUnderline();
                glyphset0.UseColourGradient();
                glyphset0.Drawglyphs();
                SimplifyGlyphCommands &glyphset1 =
                    m_GlyphHelpers[FPK_FIG_GLYPH0 % FPK_FIG_TOTALGLYPHS];
                glyphset0.SetAspectRatio(.8f);
                glyphset1.SetZRotation(-1.0f * degree);
                glyphset1.Drawglyphs();
            }
            break;

        case FPK_FIG_MISC0:
            //A case to test Fill Rectangle Extension
            ClearAndSetBackgroundColour(BG_OCHRE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL_RECTANGLE_LW);
            glVertexAttrib3fLW(1, 0, 1, 0);
            {
                UINT32 rmax = 50;
                UINT32 ri = 0;

                for(UINT32 ii = 0; ii < m_GridSize; ii++)
                {
                    UINT32 skip = rmax - ri + 1;
                    ri = (ri + 1) % rmax;
                    Coordpair p0, p1, p2, p3;
                    if ((ii + skip + 1) >= m_GridSize)
                    {
                        break;
                    }
                    p0 = m_GridCoords[ii];
                    p1 = m_GridCoords[ii + 1];
                    p2 = m_GridCoords[ii + skip];
                    p3 = m_GridCoords[ii + skip + 1];
                    {
                        glBegin(GL_TRIANGLES);
                        glColor4f(0.0f, 0.3f, 0.3f, 1.0f);
                        glVertex2f(p0.X(), p0.Y());
                        glVertex2f(p1.X(), p1.Y());
                        glVertex2f(p2.X(), p2.Y());
                        glEnd();
                    }
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    glVertexAttrib3fLW(1, 0, 0, 1);
                    {
                        glBegin(GL_TRIANGLES);
                        glVertex2f(p1.X(), p1.Y());
                        glVertex2f(p2.X(), p2.Y());
                        glVertex2f(p3.X(), p3.Y());
                        glEnd();
                    }
                }
            }
            break;

        case FPK_FIG_MISC1:
            CHECK_RC(InitShaders(SHADER0)); //Triangle
            CHECK_RC(SetupTriangles());
            rc = DrawTriangles();
            CHECK_RC(DetachShaders());
            rc = CleanupTriangles();
            glUseProgram(0);
            glDeleteProgram(m_GrProgramId);
            break;

        default:
            Printf(Tee::PriNormal, "Invalid Pick: %u\n", figureNum);
            return RC::SOFTWARE_ERROR;
    }
    glFinish();
    rc = mglGetRC();
    if (rc != OK)
    {
        Printf(Tee::PriHigh, "Error: Path Rendering failed for Figure [%d]\n",
                figureNum);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PathRenderingTest :: SetupRbo()
{
    StickyRC rc;
    //Create an offscreen buffer
    GLenum stencilfmt = (m_CheckS8)? GL_STENCIL_INDEX8: GL_NONE;
    UINT32 msaaIndex = (*m_pPickers)[LWPR_TEST_SAMPLES].Pick();
    m_Csamples = m_ColourvsDepth[msaaIndex].first;
    m_Zsamples = m_ColourvsDepth[msaaIndex].second;

    Printf(Tee::PriLow, "Create Fbo for Csamples %d Zsamples %d\n",
            m_Csamples, m_Zsamples);
    //On gm20x m_Zsamples > m_Csamples scenario is possible because
    // LW_framebuffer_mixed_samples is supported
    CHECK_RC(CreateFbo(GL_RGBA8, //8 fixed
                stencilfmt,
                m_Csamples,
                m_Zsamples,
                true, //useRbo
                &m_Fboid, &m_RBuffs[COLOURBUFF], &m_RBuffs[STENCILBUFF],
                m_DisplayWidth, m_DisplayHeight));
    if (m_CheckTIR)
    {
        glCoverageModulationLW(GL_ALPHA);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER, m_Fboid);
    glViewport(0, 0, m_DisplayWidth, m_DisplayHeight);
    rc = mglGetRC();
    return rc;
}

//------------------------------------------------------------------------------
RC PathRenderingTest :: CleanupRbo()
{
    RC rc;
    if (m_CheckTIR)
    {
        glCoverageModulationLW(GL_NONE);
        glDisable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ZERO);
        CHECK_RC(mglGetRC());
    }
    DestroyFbo(&m_Fboid, &m_RBuffs[COLOURBUFF], &m_RBuffs[STENCILBUFF], 1);
    CHECK_RC(mglGetRC());
    return rc;
}

//------------------------------------------------------------------------------
RC PathRenderingTest::Run()
{
    StickyRC rc;
    UINT32 loopsPerSample;
    CHECK_RC(PreRunState(&loopsPerSample));
    const UINT32 startDiaLoop  = m_pTstCfg->StartLoop();
    const UINT32 endDiaLoop    = startDiaLoop + m_pTstCfg->Loops();
    const UINT32 restartSkipCount = m_pTstCfg->RestartSkipCount();
    FancyPicker::FpContext * pFpCtx = GetFpContext();

    // Start test loops
    if ((startDiaLoop % restartSkipCount) != 0)
    {
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");
    }
    m_pGolden->SetSkipCount(0);
    m_pPickers->RestartInitialized();
    for (UINT32 ii = 0; ii < loopsPerSample; ii++)
    {
        if (m_UseRbo)
        {
            pFpCtx->LoopNum = ii; //Picker context for Samples array
            CHECK_RC(SetupRbo());
        }
        for (UINT32 jj = startDiaLoop; jj < endDiaLoop; ++jj)
        {
            pFpCtx->LoopNum = jj; //Picker context for Diagram array
            if ((pFpCtx->LoopNum == startDiaLoop)
                    || ((pFpCtx->LoopNum % restartSkipCount) == 0))
            {
                //Restart point
                if (m_pTstCfg->Verbose())
                {
                    Printf(Tee::PriLow, "\n\tRestart: loop %d,\n",
                            pFpCtx->LoopNum);
                }
                m_pPickers->RestartInitialized();//Restart each Fpicker
                ClearAndSetBackgroundColour(BG_BLACK);
            }
            UINT32 figureNum = (*m_pPickers)[LWPR_TEST_FIGURE].Pick();
            //Csamples: (0-99), Zsamples: (0-99), FigureNum (0-99)
            MASSERT(figureNum < 99);
            MASSERT(m_Csamples <= 32);
            MASSERT(m_Zsamples <= 32);
            UINT32 dbindex = 10000 * figureNum + 100 * m_Csamples + m_Zsamples;
            Printf(m_VerbosePrint, "Sample %u : DiaLoop %u : GoldenRecord %u\n",
                    ii, jj, dbindex);
            m_pGolden->SetLoop(dbindex);
            CHECK_RC(InnerDraw(figureNum, pFpCtx->LoopNum));
            // Goldens
            if (m_mglTestContext.UsingFbo())
            {
                m_mglTestContext.CopyFboToFront();
            }
            glFinish();
            rc = GoldenRun(figureNum);
            glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            rc = mglGetRC();
            if (OK != rc)
            {
                return rc;
            }
        } //Loop per figure or as specified as test argument
        if (m_UseRbo)
        {
            CHECK_RC(CleanupRbo());
        }
    }//Loops per sample
    CHECK_RC(m_pGolden->ErrorRateTest(GetJSObject()));
    return rc;
}

//------------------------------------------------------------------------------
RC PathRenderingTest::Cleanup()
{
    StickyRC rc;
    if (m_mglTestContext.IsGLInitialized())
    {
        if (glIsEnabled(GL_STENCIL_TEST))
        {
            glDisable(GL_STENCIL_TEST);
        }
        for (UINT32 ii = 0; ii < FPK_FIG_TOTALGLYPHS; ii++)
        {
            SimplifyGlyphCommands &glyphset = m_GlyphHelpers[ii];
            glyphset.CleanCommands();
        }
        rc = mglGetRC();
        m_GSurfs.CleanupSurfCheckShader();
        m_pGolden->ClearSurfaces();
    }
    if (m_mglTestContext.GetDisplay())
        rc = m_mglTestContext.GetDisplay()->TurnScreenOff(false);
    rc = m_mglTestContext.Cleanup();
    rc = GpuTest::Cleanup();
    return rc;
}

//END Standard test functions

//------------------------------------------------------------------------------
//! \brief Setup fancy picker for target figures to draw
RC PathRenderingTest::SetDefaultsForPicker(UINT32 idx)
{
    FancyPickerArray *pPickers = GetFancyPickerArray();
    FancyPicker &Fp = (*pPickers)[idx];
    switch (idx)
    {
        case LWPR_TEST_FIGURE:
            Fp.ConfigList(FancyPicker::WRAP);
            Fp.AddListItem(FPK_FIG_SIMPLEPATH);
            Fp.AddListItem(FPK_FIG_CONTOUR1);
            Fp.AddListItem(FPK_FIG_CONTOUR2);
            Fp.AddListItem(FPK_FIG_CONTOUR3);
            Fp.AddListItem(FPK_FIG_FILLPOINTS);
            Fp.AddListItem(FPK_FIG_MIXED_CONTOURS);
            Fp.AddListItem(FPK_FIG_SHARED_EDGE);
            Fp.AddListItem(FPK_FIG_GLYPH0);
            Fp.AddListItem(FPK_FIG_GLYPH1);
            break;

        case LWPR_SHARED_PATHID:
            Fp.ConfigRandom();
            Fp.AddRandRange(1, FPK_FIG_SHARED_MIN_PATH, FPK_FIG_SHARED_MAX_PATH);
            Fp.CompileRandom();
            break;

        case LWPR_TEST_SAMPLES:
            Fp.ConfigList(FancyPicker::WRAP);
            Fp.AddListItem(FPK_MSAA_2x2);
            Fp.AddListItem(FPK_MSAA_4x4);
            Fp.AddListItem(FPK_MSAA_8x8);
            Fp.AddListItem(FPK_MSAA_16x16);
            Fp.AddListItem(FPK_MSAA_2x4);
            Fp.AddListItem(FPK_MSAA_2x8);
            Fp.AddListItem(FPK_MSAA_2x16);
            Fp.AddListItem(FPK_MSAA_4x8);
            Fp.AddListItem(FPK_MSAA_4x16);
            Fp.AddListItem(FPK_MSAA_8x16);
            break;

        case LWPR_TEST_VERBOSITY:
            Fp.ConfigConst(LWPR_PATH_DONOTLOG);
            break;

        // Picked during Setup() so we can selfcheck coordinate data.
        case LWPR_LOG_MODE:
            Fp.ConfigConst(FPK_LOG_MODE_Skip);
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}
//-----------------------------------------------------------------------------
//Helloworld of 2d path, lwrves and straight lines
RC PathRenderingTest::SetupSimplePathSet()
{
    RC rc;
    SimplifyPathCommands *PathObject = &m_PathHelpers[FPK_FIG_SIMPLEPATH];
    PathObject->cmdM(100, 180);
    PathObject->cmdL(40, 10);
    PathObject->cmdL(190, 120);
    PathObject->cmdL(10, 120);
    PathObject->cmdL(160, 10);
    PathObject->cmdZ();
    PathObject->cmdM(300, 300);
    PathObject->cmdC(100, 400, 100, 200, 300, 100);
    PathObject->cmdZ();

    PathObject->cmdM(500, 200);
    PathObject->cmdQ(500, 400, 300, 300);
    PathObject->cmdL(500, 200);
    PathObject->cmdSetProj(30, 520.0, 10, 400.0);
    CHECK_RC(PathObject->SubmitPath(m_LogPath));
    return rc;
}

void PathRenderingTest::PrintFbo()
{
    int colorBufferCount = 0;
    int objectType;
    int objectId;

    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &colorBufferCount);
    // print info of the colorbuffer attachable image
    for(int i = 0; i < colorBufferCount; ++i)
    {
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0 + i,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                &objectType);
        if(objectType != GL_NONE)
        {
            glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER,
                    GL_COLOR_ATTACHMENT0 + i,
                    GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                    &objectId);

            Printf(Tee::PriHigh, "Color Attachment %d ", i);
            if(objectType == GL_TEXTURE)
            {
                Printf(Tee::PriHigh, "GL_TEXTURE ");
                PrintTexParameters(objectId);
            }
            else if(objectType == GL_RENDERBUFFER)
            {
                Printf(Tee::PriHigh, "GL_RENDERBUFFER ");
                PrintRBParameters(objectId);
            }
        }
    }
    // print info of the depthbuffer attachable image
    glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
            &objectType);
    if(objectType != GL_NONE)
    {
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                &objectId);

        Printf(Tee::PriHigh, "Depth Attachment: %d", objectId);
        switch(objectType)
        {
            case GL_TEXTURE:
                Printf(Tee::PriHigh, "GL_TEXTURE\n");
                PrintTexParameters(objectId);
                break;

            case GL_RENDERBUFFER:
                Printf(Tee::PriHigh, "GL_RENDERBUFFER\n");
                PrintRBParameters(objectId);
                break;

            default:
                MASSERT(!"Unsupported target type");
        }
    }

    // print info of the stencilbuffer attachable image
    glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER,
            GL_STENCIL_ATTACHMENT,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
            &objectType);
    if(objectType != GL_NONE)
    {
        glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER,
                GL_STENCIL_ATTACHMENT,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,
                &objectId);

        Printf(Tee::PriHigh, "Stencil Attachment: ");
        switch(objectType)
        {
            case GL_TEXTURE:
                Printf(Tee::PriHigh, "GL_TEXTURE ");
                PrintTexParameters(objectId);
                break;

            case GL_RENDERBUFFER:
                Printf(Tee::PriHigh, "GL_RENDERBUFFER ");
                PrintRBParameters(objectId);
                break;

            default:
                MASSERT(!"Unsupported target type");
        }
    }
    GLint num_samples;
    glGetIntegerv(GL_SAMPLES, &num_samples);
    Printf(Tee::PriHigh, "Fbo raster samples are %u\n", num_samples);
}

void PathRenderingTest::PrintTexParameters(GLint id)
{
    if(glIsTexture(id) == GL_FALSE)
    {
        Printf(Tee::PriHigh, "%d is not a texture object\n", id);
        return;
    }

    int width, height, format;
    glBindTexture(GL_TEXTURE_2D, id);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);            // get texture width
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);          // get texture height
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &format); // get texture internal format
    glBindTexture(GL_TEXTURE_2D, 0);
    Printf(Tee::PriHigh, "Wd:Ht:format = %d:%d:0x%x\n", width, height, format);
}

void PathRenderingTest::PrintRBParameters(GLint id)
{
    if(glIsRenderbufferEXT(id) == GL_FALSE)
    {
        Printf(Tee::PriHigh, "%d is not a renderbuffer object\n", id);
        return;
    }

    int width, height, format;
    glBindRenderbufferEXT(GL_RENDERBUFFER, id);
    glGetRenderbufferParameterivEXT(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);    // get renderbuffer width
    glGetRenderbufferParameterivEXT(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);  // get renderbuffer height
    glGetRenderbufferParameterivEXT(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format); // get renderbuffer internal format
    glBindRenderbufferEXT(GL_RENDERBUFFER, 0);
    Printf(Tee::PriHigh, "Wd:Ht:format = %d:%d:0x%x\n", width, height, format);
}
//------------------------------------------------------------------------------
//! \brief Setup Fbo
RC PathRenderingTest::CreateFbo(GLenum colourfmt, GLenum stencilfmt,
        GLint csamples, GLint dsamples, bool renderBufferTarget,
        GLuint *pfboid, GLuint *prboid, GLuint *pstnclid, //Modified
        int wd, int ht)
{
    GLboolean depthAttach = GL_FALSE;
    GLboolean stencilAttach = GL_FALSE;
    StickyRC rc;

    //Generate Fbo
    glGenFramebuffers(1, pfboid);
    if (renderBufferTarget)
    {
        glGenRenderbuffersEXT(1, prboid);
        glGenRenderbuffersEXT(1, pstnclid);
        rc = mglGetRC();
    }
    else
    {
        MASSERT(!"Render to Texture targets isn't supported!");
        return RC::UNSUPPORTED_FUNCTION;
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER, *pfboid);
    //Generate RBuffer or TextureBuffer based on colour format selection
    if (colourfmt != GL_NONE)
    {
        if (renderBufferTarget)
        {
            glBindRenderbufferEXT(GL_RENDERBUFFER, *prboid);
            if (csamples > 1)
            {
                glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, csamples,
                        colourfmt, wd, ht);
            }
            else
            {
                glRenderbufferStorageEXT(GL_RENDERBUFFER, colourfmt, wd, ht);
            }
            // Bind the renderbuffer object to the framebuffer's color0 logical buffer.
            // now all framebuffer color0 access will use the renderbuffer storage.
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                    GL_RENDERBUFFER, *prboid);
        }
        else
        {
            //NOT needed
            MASSERT(!"Using texture objects is not supported in GLPathRendering set of tests\n");
        }
    }
    rc = mglGetRC();

    //Allocate stencil attachment. For GLPathRendering Depth buffer
    //attachment isn't required
    switch (stencilfmt)
    {
        //Lwrrently only Stencil index8 is tested but rest of the cases
        //are also handled for completeness.
        case GL_STENCIL_INDEX8:
            stencilAttach = GL_TRUE;
            break;

        case GL_DEPTH24_STENCIL8:
             stencilAttach = depthAttach = GL_TRUE;
             break;

        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
             depthAttach = GL_TRUE;
             break;

        case GL_NONE:
        default:
             Printf(Tee::PriHigh, " No depth buffer attachment sought\n");
             break;
    }
    if (renderBufferTarget && (stencilfmt != GL_NONE))
    {
        // bind the stencil renderbuffer object so all future renderbuffer APIs will
        // be directed at this object.
        glBindRenderbufferEXT(GL_RENDERBUFFER, *pstnclid);
        if (dsamples > 1)
        {
            // create some storage for the stencil object.
            glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER, dsamples,
                    stencilfmt, wd, ht);
        }
        else
        {
            // create some storage for the stencil object.
            glRenderbufferStorageEXT(GL_RENDERBUFFER, stencilfmt, wd, ht);
        }
        if(stencilAttach)
        {
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                    GL_RENDERBUFFER, *pstnclid);
        }
        if(depthAttach)
        {
            glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                    GL_RENDERBUFFER,* pstnclid);
        }
        rc = mglGetRC();
    }
    else
    {//Do not handle texture cases
        MASSERT(!"Using texture objects is not supported in GLPathRendering set of tests\n");
    }
    if(m_pTstCfg->Verbose())
    {
        PrintFbo();
    }
    CHECK_RC(mglCheckFbo());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief destroy framebuffer object
RC PathRenderingTest::BlitFbo(const GLint readFbo, const GLint writeFbo,
        GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
        GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
        GLbitfield msk, GLenum filter)
{
    StickyRC rc;
    glBindFramebufferEXT(GL_READ_FRAMEBUFFER, readFbo);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, writeFbo);
    rc = mglGetRC();

    //handle multisampled source before bliting it on to single sampled destination
    //windowsize
    GLint srcWidth  = (srcX0 < srcX1) ? (srcX1 - srcX0) : (srcX0 - srcX1);
    GLint srcHeight = (srcY0 < srcY1) ? (srcY1 - srcY0) : (srcY0 - srcY1);
    GLint dstWidth  = (dstX0 < dstX1) ? (dstX1 - dstX0) : (dstX0 - dstX1);
    GLint dstHeight = (dstY0 < dstY1) ? (dstY1 - dstY0) : (dstY0 - dstY1);
    GLint isSrcMsaa;
    GLint isDstMsaa;
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, readFbo);
    glGetIntegerv(GL_SAMPLE_BUFFERS, &isSrcMsaa);
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, writeFbo);
    glGetIntegerv(GL_SAMPLE_BUFFERS, &isDstMsaa);
    rc = mglGetRC();
    /*
       We can't copy Fbo contents to a different Fbo without confirming size and
       number of samples. Copying will result in Invalid operation error in OGL.
       Here is the example:

       SrcFbo A, DestFbo B,
       IntermediateFbo C with 1x sample and size same as Fbo B
       if ((WindowSize(A) != WindowSize(B)
                &&
            (IsSingleSampled(A) && IsMultiSampled(B)))
       {
           Blit(A->B) is invalid operation then because stretching or contraction is
           not allowed.

       }
       If ((IsMultisampled(A) && IsMultisampled(B)) &&
           (NumGLSamples(A) != NumGLSamples(B)))
       {
           Blit(A->B) is invalid operation
       }
    */
    bool needsTempFbo = true;
    if (!isDstMsaa)
    {
        needsTempFbo = false;
    }
    if (needsTempFbo)
    {
        if ((srcWidth == dstWidth) && (srcHeight == dstHeight) && !(isSrcMsaa))
        {
            needsTempFbo = false;
        }
    }
    if(!needsTempFbo)
    {
        glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0,
                dstX1, dstY1, msk, filter);
    }
    else
    {
        GLuint tempFbo;        // framebuffer object used for emulation
        GLuint tempRbo[3];     // renderbuffers used for emulation
        CHECK_RC(CreateFbo(GL_RGBA8,         //colour format
                GL_STENCIL_INDEX8,  //Stencil format
                1, 1,                //Colour samples vs depth samples
                true,                //UseRbo
                &tempFbo, &tempRbo[0], &tempRbo[1], //Buffer name ids
                dstWidth, dstHeight //Window size
                ));
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, readFbo);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, tempFbo);
        // Blit from the source to our 1x temp framebuffer.
        glBlitFramebufferEXT(srcX0, srcY0, srcX1, srcY1, 0, 0, dstWidth, dstHeight, msk, filter);
        rc = mglGetRC();
        // Blit from our 1x temp framebuffer to the destination of the default (GL_FRONT).
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, tempFbo);
        glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, writeFbo);
        glBlitFramebufferEXT(0, 0, dstWidth, dstHeight, dstX0, dstY0, dstX1, dstY1, msk, filter);
        rc = mglGetRC();
        // Restore the read framebuffer binding.
        glBindFramebufferEXT(GL_READ_FRAMEBUFFER, readFbo);
        // Delete our temp resources.
        CHECK_RC(DestroyFbo(&tempFbo, &tempRbo[0], &tempRbo[1], true));
    }
    //Restore the order
    glBindFramebufferEXT(GL_FRAMEBUFFER, readFbo);
    rc = mglGetRC();
    return rc;
}

//------------------------------------------------------------------------------
//! \brief destroy framebuffer object
RC PathRenderingTest::DestroyFbo(
        GLuint *pfboid, GLuint *prboid, GLuint *pstnclid, bool renderBufferTarget)
{
    StickyRC rc;
    if (renderBufferTarget)
    {
        glDeleteRenderbuffersEXT(1, pstnclid);
        glDeleteRenderbuffersEXT(1, prboid);
        rc = mglGetRC();
        if (rc == OK)
        {
            *prboid = *pstnclid = 0;
        }
    }
    else
    {
        MASSERT(!"We are not handeling Texture target\n");
        rc = RC::UNSUPPORTED_FUNCTION;
    }
    glDeleteFramebuffersEXT(1, pfboid); //This is equivalent to bind FBO id 0 after deletion
    rc = mglGetRC();
    if (rc == OK)
    {
        *pfboid = 0;
    }
    rc = mglGetRC();
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Setup fancy picker for target figures to draw
RC PathRenderingTest::DrawSimplePathSet(int FigId)
{
    RC rc;
    //Set projection
    glMatrixLoadIdentityEXT(GL_PROJECTION);
    glMatrixOrthoEXT(GL_PROJECTION, 0, 500, 0, 500, -1, 1);
    glMatrixLoadIdentityEXT(GL_MODELVIEW);
    MASSERT(glIsPathLW(FPK_FIG_SIMPLEPATH + 1));

    //Draw
    glStencilFillPathLW(FigId, GL_COUNT_UP_LW, 0x1F);//0x1f);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 0, 0x1F); //0x1f
    glStencilOp(GL_KEEP, GL_ZERO, GL_ZERO);
    glColor3f(0, 0,.1f); //Stencil Fill
    glCoverFillPathLW(FigId, GL_BOUNDING_BOX_LW);
    glStencilStrokePathLW(FigId, 0x1, ~0);
    glColor3f(0.2f, 0.0, 0.1f); //Stroke colour
    glCoverStrokePathLW(FigId, GL_COLWEX_HULL_LW);  //Pick this up from fpk

    CHECK_RC(mglGetRC());
    return rc;
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::SetContourPath1() //for Line join
{
    RC rc;

    SimplifyPathCommands *PathObject = &m_PathHelpers[FPK_FIG_CONTOUR1];
    //prepare line commands
    PathObject->ClearPathCommands();
    PathObject->cmdM(m_CoordSet[0]);
    unsigned int ii =1;
    for (; ii < NUMELEMS(m_CoordSet); ii++)
    {
        PathObject->cmdL(m_CoordSet[ii]);
    }
    PathObject->cmdZ();
    PathObject->SetPathColor(BG_BLUE);
    PathObject->cmdSetProj(-300, 300.0, -200, 200.0, .5, .5);
    return rc;
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::SetContourPath2() //for lwrve
{
    RC rc;
    SimplifyPathCommands *PathObject = &m_PathHelpers[FPK_FIG_CONTOUR2];
    PathObject->ClearPathCommands();
    PathObject->cmdM(m_CoordSet[0]); //Origin
    //broken interesecting lines
    for (UINT32 ii =0; ii < NUMELEMS(m_CoordSet); ii+=3)
    {
        PathObject->cmdC(&m_CoordSet[ii]);
    }
    PathObject->cmdSetProj(-300, 300, -200, 200, .5, .5);
    PathObject->SetPathColor(BG_RED);
    return rc;
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::SetContourPath3() //for Quads
{
    RC rc;
    SimplifyPathCommands *PathObject = &m_PathHelpers[FPK_FIG_CONTOUR3];
    //prepare line commands

    //Move to origin
    const float delta = .5;
    const float a = 1200.0;
    const UINT32 maxPathLen = 10000;
    const Coordpair trans(0, 0);
    Coordpair origin = GetNextPointOnSpiral(0, a) + trans;
    PathObject->ClearPathCommands();
    PathObject->cmdM(origin);

    //Construct rest of the path
    float t = 0;
    for (UINT32 ii = 0; ii < maxPathLen; ii+=2)
    {
        Coordpair pt0, pt1;
        pt0 = GetNextPointOnSpiral(t, a) + trans;
        t += delta;
        pt1 = GetNextPointOnSpiral(t, a) + trans;
        t += delta;
        PathObject->cmdQ(pt0, pt1);
    }
    PathObject->SetPathColor(BG_MIX0);
    PathObject->cmdSetProj(-800, 800, -800, 800, .5, .5);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Register Path id in glContext
void PathRenderingTest::GenSharedPathIds()
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &fp = (*pPickers)[LWPR_SHARED_PATHID];

    for (UINT32 ii = 0; ii <  NUMELEMS(s_SharedPaths); ii++)
    {
        if (s_SharedPaths[ii].GetPathid() == ILWALIDPATHID)
        {
            UINT32 upid = fp.Pick();
            s_SharedPaths[ii].SetPathid(upid);
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Generate test paths with multiple shared edge. In this case a adjacent
//!        rectangles
RC PathRenderingTest::SetSharedEdges(const UINT32 maxrow, const UINT32 maxcol) //Multiple paths
{
    RC rc;
    const UINT32 maxCoords = static_cast<UINT32>(NUMELEMS(s_SharedPaths));

    MASSERT((maxrow * maxcol) <= FPK_FIG_SHARED_MAX_PATH - FPK_FIG_SHARED_MIN_PATH);
    GenSharedPathIds();

    const Coordpair  unit(5.0f, 5.0f);
    const Coordpair xunit(5.0f, 0.0f);
    const Coordpair yunit(0.0f, 5.0f);
    Coordpair pmin(10, 10);
    Coordpair pmax = pmin + unit;
    const Coordpair origin = pmin;
    m_SharedLeft = origin.X();
    m_SharedRight = origin.X() + unit.X() * (maxcol - 1);
    m_SharedBottom = origin.Y();
    m_SharedTop = origin.Y() + unit.Y() * (maxrow/2);
    UINT32 lwrcol = 0;
    UINT32 maxPathObjs = MIN(maxCoords, (maxrow * maxcol));
    for(UINT32 ii = 0; ii < maxPathObjs;ii += 2)
    {
        pmax = pmin + unit;
        SimplifyPathCommands *sp0 = &s_SharedPaths[ii];
        SimplifyPathCommands *sp1 = &s_SharedPaths[ii+1];
        DivideCell(sp0, sp1, pmin, pmax);
        sp0->SetPathColor(BG_OCHRE);
        sp1->SetPathColor(BG_RED);
        sp0->SetStrokeWidth(0.2f);
        sp1->SetStrokeWidth(0.2f);
        sp0->cmdSetProj(m_SharedLeft,
                m_SharedRight,
                m_SharedBottom,
                m_SharedTop, 1, 1); //Scaling to make pattern visible
        sp1->cmdSetProj(m_SharedLeft,
                m_SharedRight,
                m_SharedBottom,
                m_SharedTop, 1, 1);
        if (ii % maxrow)
        {
            pmin = pmin + yunit;
        }
        else if (ii)
        {
            pmin = origin + (xunit * lwrcol);
            lwrcol++;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::SetupTriangles()
{
    RC rc;
    CHECK_RC(mglGetRC());
    //Array of Vertices
    glGelwertexArrays(NUMVAOS, s_Vaos);
    glBindVertexArray(s_Vaos[TRIANGLES]);
    glGenBuffers(NUMBUFFERIDS, s_Buffers);
    glBindBuffer(GL_ARRAY_BUFFER, s_Buffers[ARRAYBUFFER]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_Vert), s_Vert, GL_STATIC_DRAW);
    return mglGetRC();
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::DrawTriangles()
{
    ClearAndSetBackgroundColour(BG_WHITE);
    glUseProgram(m_GrProgramId);
    glVertexAttribPointer(vPosition, 2, GL_FLOAT,
            GL_FALSE, 0, (const GLvoid*) 0);
    glEnableVertexAttribArray(vPosition);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(-10.0, 10.0, -10.0, 10.0, -1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(s_Vaos[TRIANGLES]);
    glDrawArrays(GL_TRIANGLES, 0, s_NumVertices);
    glPopMatrix();
    return mglGetRC();
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::CleanupTriangles()
{
    glDisableVertexAttribArray(vPosition);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(NUMBUFFERIDS, s_Buffers);
    glBindVertexArray(0);
    glDeleteVertexArrays(NUMVAOS, s_Vaos);
    return mglGetRC();
}
//-----------------------------------------------------------------------------
RC PathRenderingTest::DrawSharedEdges(const UINT32 maxrow, const UINT32 maxcol) //Multiple paths
{
    RC rc;
    UINT32 maxCoords = static_cast<UINT32>(NUMELEMS(s_SharedPaths));
    MASSERT((maxrow * maxcol) <= FPK_FIG_SHARED_MAX_PATH - FPK_FIG_SHARED_MIN_PATH);
    UINT32 maxPathObjs = MIN(maxCoords, (maxrow * maxcol));
    for (UINT32  ii = 0; ii < maxPathObjs; ii++)
    {
        if (m_SharedEdgeIsSupported)
        {
            s_SharedPaths[ii].TestSharedEdge(true);
        }

        CHECK_RC(s_SharedPaths[ii].cmdDrawPath(m_CoverFill, m_PathStroke));
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::SetuplwprGlyphs()
{
    for (UINT32 ii = 0; ii < FPK_FIG_TOTALGLYPHS; ii++)
    {
        SimplifyGlyphCommands &glyphset = m_GlyphHelpers[ii];
        glyphset.Setupglyphs(LWPRTexts[ii]);
        glyphset.SetAspectRatio(LWPRTextAspectRatio);
        glyphset.EnableStroking();
        glyphset.EnableFilling();
    }

    return mglGetRC();
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::SetupFillScreen()
{
    RC rc;
    SimplifyPathCommands &PathObject = m_PathHelpers[FPK_FIG_FILLPOINTS];

    //prepare line commands
    PathObject.cmdM(m_ScrFillCoordSet[0]);
    unsigned int ii =1;
    for (; ii < NUMELEMS(m_ScrFillCoordSet); ii++)
    {
        PathObject.cmdL(m_ScrFillCoordSet[ii]);
    }
    PathObject.SetPathColor(BG_BLUE);
    PathObject.cmdZ();
    PathObject.cmdSetProj(-300.0, 300.0, -200.0, 200.0, 0.8f, 0.8f);
    CHECK_RC(PathObject.SubmitPath(m_LogPath));
    //Set style
    PathObject.SelectPathStyle(PathStyle::CAP_TRIANGLE, PathStyle::JOIN_ROUND, 1);
    CHECK_RC(PathObject.SubmitPathStyleSettings());
    return rc;
}

//-----------------------------------------------------------------------------
GLuint PathRenderingTest::CompileShader(GLenum shaderType, const GLchar * src)
{
    MASSERT(src);
    GLuint shader = glCreateShader(shaderType);
    GLint  len = static_cast<GLint>(strlen(src)), result;
    glShaderSource(shader, 1, &src, &len);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
    if(result == GL_FALSE)
    {
        /* get the shader info log */
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        char *log = new char [len+1]; // "+1" To make Coverity happy
        glGetShaderInfoLog(shader, len, &result, &log[0]);
        Printf(Tee::PriHigh, "Unable to compile: [%s]\n", &log[0]);
        glDeleteShader(shader);
        delete [] log;
        return 0;
    }

    return shader;
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::DetachShaders()
{
    GLsizei count = 0;
    GLuint shaders[2] = {0};
    glGetAttachedShaders(m_GrProgramId, 2, &count, shaders);
    glDetachShader(m_GrProgramId, shaders[0]);
    glDetachShader(m_GrProgramId, shaders[1]);
    return mglGetRC();
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::AttachShader(GLenum shaderType, const GLchar * src)
{
    RC rc;
    GLuint shader = CompileShader(shaderType, src);
    if (shader)
    {
        glAttachShader(m_GrProgramId, shader);
        CHECK_RC(mglGetRC());
        glDeleteShader(shader); //Mark shader for deletion after processing
    }
    else
    {
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PathRenderingTest::InitShaders(UINT32 shaderId)
{
    RC rc;
    m_GrProgramId = glCreateProgram();
    CHECK_RC(AttachShader(GL_VERTEX_SHADER, m_ListShaders[shaderId].vsh.c_str()));
    CHECK_RC(AttachShader(GL_FRAGMENT_SHADER, m_ListShaders[shaderId].fsh.c_str()));
    glLinkProgram(m_GrProgramId);
    GLint ret = GL_FALSE;
    glGetProgramiv(m_GrProgramId, GL_LINK_STATUS, &ret);
    if (ret == GL_FALSE) //Temp
    {
        GLint len;
        glGetProgramiv(m_GrProgramId, GL_INFO_LOG_LENGTH, &len);
        char *log = new char[len];
        glGetProgramInfoLog(m_GrProgramId, len, &ret, &log[0]);
        Printf(Tee::PriHigh, "GrProgram Link failed [%s]\n", &log[0]);
        glDeleteProgram(m_GrProgramId);
        delete [] log;
        m_GrProgramId = 0;
    }
    return mglGetRC();
}

void PathRenderingTest::LogCoordPairs(UINT32 id)
{
    Coordpair *pCoord = nullptr;
    size_t maxCoords = 0;
    switch (id)
    {
        case CoordSet:
            pCoord = &m_CoordSet[0];
            maxCoords = NUMELEMS(m_CoordSet);
            break;

        case SrcFillCoordSet:
            pCoord = &m_ScrFillCoordSet[0];
            maxCoords = NUMELEMS(m_ScrFillCoordSet);
            break;

        case GridCoords:
            pCoord = &m_GridCoords[0];
            maxCoords = NUMELEMS(m_GridCoords);
            break;

        default:
            MASSERT(!"Invalid coord ID");
            break;
    }
    for (size_t ii = 0; ii < maxCoords; ii++)
    {
        GLfloat xval = pCoord[ii].X();
        GLfloat yval = pCoord[ii].Y();
        m_GSelfCheck.LogData(id, &xval, sizeof(GLfloat));
        m_GSelfCheck.LogData(id, &yval, sizeof(GLfloat));
    }
}

