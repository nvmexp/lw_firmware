/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2013-2017 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "glhist.h"
#include "core/include/imagefil.h"
#include "core/include/display.h"
#include <math.h>
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include <sstream>

GLHistogram::GLHistogram()
    : m_NumBins(128)
      ,m_Labels(128)
      ,m_Values(128)
      ,m_Dividers(128)
      ,m_Thresholds(128)
      ,m_ConstScratch(128)
      ,m_LwrrBin(0)
      ,m_UseDiv(false)
      ,m_UseLog(false)
      ,m_GraphOffset(0)
      ,m_GraphScale(0)
      ,m_Inited(false)
      ,m_ScreenWidth(128)
      ,m_ScreenHeight(512)
{
    SetName("GLHistogram");
}

GLHistogram::~GLHistogram()
{
}

bool GLHistogram::IsSupported()
{
    InitFromJs();
    if (!m_mglTestContext.IsSupported())
        return false;
    return true;
}

RC GLHistogram::InitFromJs()
{
    RC rc;
    CHECK_RC( GpuMemTest::InitFromJs() );

    CHECK_RC( m_mglTestContext.SetProperties(
            GetTestConfiguration(),
            false,      // double buffer
            GetBoundGpuDevice(),
            GetDispMgrReqs(),
            0,          // don't override color format
            false,      // draw offscreen?
            false,      // render to sysmem
            0));        //do not use layered FBO

    return rc;
}

//------------------------------------------------------------------------------
void GLHistogram::PrintJsProperties(Tee::Priority pri)
{
}

RC GLHistogram::Setup()
{
    RC rc;
    InitFromJs();
    CHECK_RC(GpuMemTest::Setup());
    CHECK_RC(m_mglTestContext.Setup());

    CHECK_RC(SetupGL());

    m_Inited = true;
    return mglGetRC();
}

RC GLHistogram::SetupFramebuffers(UINT32 bins)
{
    RC rc;

    //DRAW FB HIST
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_HIST]);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_DRAW_COLOR],
        0);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_DRAW_DEPTH],
        0);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_STENCIL_ATTACHMENT_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_DRAW_DEPTH],
        0);
    CHECK_RC(mglCheckFbo());

    //COMPARE FB
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_CHECK]);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_CHECK_COLOR],
        0);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_DEPTH_ATTACHMENT_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_DRAW_DEPTH],
        0);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_STENCIL_ATTACHMENT_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_DRAW_DEPTH],
        0);
    CHECK_RC(mglCheckFbo());

    //HISTOGRAM FB -- default FB
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    CHECK_RC(mglCheckFbo());
    return OK;
}

RC GLHistogram::SetupPrograms(UINT32 bins)
{
    RC rc;
#define PROGRAM_SETUP(A,B,C)                                                      \
{                                                                                 \
    string prog_##A = B;                                                          \
    CHECK_RC(mglLoadProgram(GL_##C##_PROGRAM_LW, m_ProgIds[PROG_##A],             \
                            static_cast<GLsizei>(prog_##A.size()),                \
                            reinterpret_cast<const GLubyte *>(prog_##A.c_str())));\
}
    PROGRAM_SETUP(FRAG_HIST,        GetFragmentProgramHist(bins), FRAGMENT);
    PROGRAM_SETUP(FRAG_CHECK,       GetFragmentProgramCopyRed(),  FRAGMENT);
    PROGRAM_SETUP(FRAG_COPY,        GetFragmentProgramCopy(),     FRAGMENT);
#undef PROGRAM_SETUP
    return mglGetRC();
}

RC GLHistogram::SetupConstants(UINT32 bins)
{
    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_FRAG_HIST]);
    glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, (2*bins+1)*sizeof(float), 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);

    return mglGetRC();
}

RC GLHistogram::SetupTextures(UINT32 bins)
{
    glActiveTexture(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);

    // TEX_DRAW_COLOR - this texture contains the draw surface for the main test
    glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR]);
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_RGBA8,
                  m_ScreenWidth,
                  m_ScreenHeight,
                  0,
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  NULL);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT );

    // TEX_DRAW_DEPTH -
    glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_DRAW_DEPTH]);
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_DEPTH_STENCIL_EXT,
                  m_ScreenWidth,
                  m_ScreenHeight,
                  0,
                  GL_DEPTH_STENCIL_EXT,
                  GL_UNSIGNED_INT_24_8_EXT,
                  NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // TEX_CHECK_COLOR - this texture contains pixels for oclwlsion query
    glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_CHECK_COLOR]);
    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  GL_RGBA8,
                  m_ScreenWidth,
                  m_ScreenHeight,
                  0,
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return mglGetRC();
}

RC GLHistogram::ResizeBins()
{
    RC rc;

    glDeleteProgramsLW        (PROG_NUM,  (const GLuint*)m_ProgIds);
    glDeleteBuffers           (CONST_NUM, (const GLuint*)m_ConstIds);
    glDeleteFramebuffersEXT   (FBO_NUM,   (const GLuint*)m_FboIds);
    glDeleteTextures          (TEX_NUM,   (const GLuint*)m_TexIds);
    glDeleteOcclusionQueriesLW(OCC_NUM,   (const GLuint*)m_OccIds);

    memset(m_ProgIds,  0, sizeof(m_ProgIds));
    memset(m_ConstIds, 0, sizeof(m_ConstIds));
    memset(m_FboIds,   0, sizeof(m_FboIds));
    memset(m_TexIds,   0, sizeof(m_TexIds));
    memset(m_OccIds,   0, sizeof(m_OccIds));

    CHECK_RC(SetupGL());
    return mglGetRC();
}

RC GLHistogram::SetupGL()
{
    RC rc;

    //viewport calcs; no state set here
    m_ScreenWidth  = m_NumBins;
    m_ScreenHeight = 512;

    //TEX
    glGenTextures(TEX_NUM, (GLuint*)m_TexIds);
    CHECK_RC(SetupTextures(m_NumBins));

    //FBO
    glGenFramebuffersEXT(FBO_NUM, (GLuint*)m_FboIds);
    CHECK_RC(SetupFramebuffers(m_NumBins));

    //PRG
    glGenProgramsLW(PROG_NUM, (GLuint*)m_ProgIds);
    CHECK_RC(SetupPrograms(m_NumBins));

    //OCCd
    glGenOcclusionQueriesLW(OCC_NUM, (GLuint*)m_OccIds);

    //Const
    glGenBuffers(CONST_NUM, (GLuint*)m_ConstIds);
    CHECK_RC(SetupConstants(m_NumBins));

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);       // Enables Depth Testing
    glDisable(GL_BLEND);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_LWLL_FACE);
    glDisable(GL_VERTEX_PROGRAM_LW);
    glDisable(GL_FRAGMENT_PROGRAM_LW);

    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);

    for(int i = 0; i < 8; i++)
    {
        glActiveTexture(GL_TEXTURE0_ARB+i);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    return mglGetRC();
}
void GLHistogram::UpdateConstBuffer()
{
    //constbuff
    for(UINT32 i = 0; i < m_NumBins; i++)
    {
        FLOAT64 newvalue, newthresh;
        newvalue  = m_Values[i];
        newthresh = m_Thresholds[i];
        if(m_UseDiv && m_Dividers[i])
            newvalue /= m_Dividers[i];
        if(m_UseLog && newvalue)
            newvalue = log10(newvalue);
        if(m_UseLog && newthresh)
            newthresh = log10(newthresh);
        if(m_GraphOffset && newvalue)
            newvalue  += m_GraphOffset;
        if(m_GraphOffset && newthresh)
            newthresh += m_GraphOffset;
        if(m_GraphScale)
        {
            newvalue  *= m_GraphScale;
            newthresh *= m_GraphScale;
        }
        m_ConstScratch [i + 0        ] = static_cast<FLOAT32>(newvalue);
        m_ConstScratch [i + m_NumBins] = static_cast<FLOAT32>(newthresh);
    }
    m_ConstScratch [2 * m_NumBins] = static_cast<FLOAT32>(m_NumBins);
}

RC GLHistogram::ReconfigureGPU(gpu_config_e mode)
{
    glDisable         (GL_FRAGMENT_PROGRAM_LW);
    glBindProgramLW   (GL_FRAGMENT_PROGRAM_LW, 0);
    glBindBufferBaseLW(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0, 0);

    glActiveTexture(GL_TEXTURE0_ARB);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    //viewport
    glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);

    //MVP
    glMatrixMode(GL_PROJECTION);    // add perspective to scene
    glLoadIdentity();
    glOrtho (0, m_ScreenWidth, 0, m_ScreenHeight, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    int sz;
    //FLOAT32 tempbins;
    //VBOs (nothing to be done -- there is no toggling of this state)
    switch(mode)
    {
    case CONFIG_HIST:
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_HIST]);

        //shader
        glEnable        (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW (GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_HIST]);
        glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_FRAG_HIST]);

        //constbuffer
        UpdateConstBuffer();
        sz = static_cast<int>(m_ConstScratch.size()*sizeof(FLOAT32));
        glBufferSubData (GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW,  0, sz, &m_ConstScratch[0] );
        glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);
        glBindBufferBaseLW(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0, m_ConstIds[CONST_FRAG_HIST]);

        glClear(GL_COLOR_BUFFER_BIT);
        break;
    case CONFIG_CHECK:
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_CHECK]);

        //shader
        glEnable        (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW (GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_CHECK]);

        //texture
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR]);

        glClear(GL_COLOR_BUFFER_BIT);
        break;
    case CONFIG_COPY:
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        //shader
        glEnable        (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW (GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_COPY]);

        //texture
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR]);

        glClear(GL_COLOR_BUFFER_BIT);
        break;
    default:
    case CONFIG_OFF:
        //unbind
        break;
    }

    return mglGetRC();
}

RC GLHistogram::Run()
{
    RC rc;

    ReconfigureGPU(CONFIG_HIST);
    CHECK_RC(DrawFunction());  //update FBO

    ReconfigureGPU(CONFIG_COPY);
    CHECK_RC(DrawFunction());  //update FBO

    //FIXME: keep this around for OEMs but point it at GL_FRONT
    //ReconfigureGPU(CONFIG_CHECK);
    //rc = CheckBufferGPU();  //Checks for presence of red Pixels

    rc = CheckBuffer();

    return rc;
}

RC GLHistogram::CheckBuffer()
{
    for (UINT32 i = 0; i < m_NumBins; i++)
    {
        FLOAT64 val = m_Values[i];
        if (m_UseDiv) val /= m_Dividers[i];
        if ( val >= m_Thresholds[i]) return RC::GPU_STRESS_TEST_FAILED;
    }
    return OK;
}

RC GLHistogram::CheckBufferGPU()
{
    RC rc;
    glBeginOcclusionQueryLW(m_OccIds[OCC_QUERY0]);
    CHECK_RC(DrawFunction());
    glEndOcclusionQueryLW();

    GLuint bad_pixels_occ = 0;
    glGetOcclusionQueryuivLW(m_OccIds[OCC_QUERY0], GL_PIXEL_COUNT_LW, &bad_pixels_occ);

    if (bad_pixels_occ) return RC::GPU_STRESS_TEST_FAILED;
    else                return OK;

    return rc;
}

RC GLHistogram::DrawFunction ()
{
    glBegin(GL_TRIANGLES);

      glTexCoord2f(              0.0f,                0.0f);
      glVertex2f  (              0.0f,                0.0f);
      glTexCoord2f(              2.0f,                0.0f);
      glVertex2f  (m_ScreenWidth*2.0f,                0.0f);
      glTexCoord2f(              0.0f,                2.0f);
      glVertex2f  (              0.0f, m_ScreenHeight*2.0f);

    glEnd();
    return mglGetRC();
}

RC GLHistogram::PrintStats()
{
    VerbosePrintf("\nGLHistogram::Stats\n");
    VerbosePrintf("Num Bins : %d\n", m_NumBins);
    for (UINT32 i = 0; i < m_NumBins; i++)
    {
        if(m_Values[i])
        {
            VerbosePrintf("    Bin %d (%s Hz) : %g %g %g %g %g\n",
                   i,
                   m_Labels[i].c_str(),
                   m_Values[i],
                   m_Dividers[i],
                   m_Thresholds[i],
                   m_ConstScratch[i],
                   m_ConstScratch[i+m_NumBins]
                );
        }
    }
    return OK;
}

RC GLHistogram::Cleanup()
{
    StickyRC firstRc;
    if (m_mglTestContext.IsGLInitialized())
    {
        ReconfigureGPU(CONFIG_OFF);
    }

    PrintStats();

    if (m_mglTestContext.IsGLInitialized())
    {
        if(m_ProgIds[0])
            glDeleteProgramsLW        (PROG_NUM,  (const GLuint*)m_ProgIds);
        if(m_ConstIds[0])
            glDeleteBuffers           (CONST_NUM, (const GLuint*)m_ConstIds);
        if(m_FboIds[0])
            glDeleteFramebuffersEXT   (FBO_NUM,   (const GLuint*)m_FboIds);
        if(m_TexIds[0])
            glDeleteTextures          (TEX_NUM,   (const GLuint*)m_TexIds);
        if(m_OccIds[0])
            glDeleteOcclusionQueriesLW(OCC_NUM,   (const GLuint*)m_OccIds);
    }

    memset(m_ProgIds,  0, sizeof(m_ProgIds));
    memset(m_ConstIds, 0, sizeof(m_ConstIds));
    memset(m_FboIds,   0, sizeof(m_FboIds));
    memset(m_TexIds,   0, sizeof(m_TexIds));
    memset(m_OccIds,   0, sizeof(m_OccIds));

    if (m_mglTestContext.GetDisplay())
    {
        firstRc = m_mglTestContext.GetDisplay()->TurnScreenOff(false);
    }
    firstRc = m_mglTestContext.Cleanup();
    firstRc = GpuMemTest::Cleanup();
    return firstRc;
}

JS_CLASS_INHERIT_FULL(GLHistogram, GpuMemTest, "GLHistogram Test", MODS_INTERNAL_ONLY);

CLASS_PROP_READWRITE(GLHistogram, NumBins,     UINT32,  "Number of Bins in Histogram");
CLASS_PROP_READWRITE(GLHistogram, LwrrBin,     UINT32,  "Set/Get Current Bin");
CLASS_PROP_READWRITE(GLHistogram, LwrrLabel,   string, "Set/Get Current Label");
CLASS_PROP_READWRITE(GLHistogram, LwrrVal,     FLOAT64, "Set/Get Current Value");
CLASS_PROP_READWRITE(GLHistogram, LwrrDiv,     FLOAT64, "Set/Get current divider");
CLASS_PROP_READWRITE(GLHistogram, LwrrThresh,  FLOAT64, "Set/Get current Thresh");
CLASS_PROP_READWRITE(GLHistogram, UseDiv,      bool,  "divider used");
CLASS_PROP_READWRITE(GLHistogram, UseLog,      bool,  "log10 scale used");
CLASS_PROP_READWRITE(GLHistogram, GraphOffset, UINT32," (V-offset)*scale");
CLASS_PROP_READWRITE(GLHistogram, GraphScale,  UINT32," (V-offset)*scale");

string GLHistogram::GetFragmentProgramCopyRed()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF NE.x;\n"
        "ELSE;\n"
            "KIL {-1};\n"
        "ENDIF;\n"
        "MOV result.color, R1;\n"
        "END\n";
    return str;
}

string GLHistogram::GetFragmentProgramCopy()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV result.color, R1;\n"
        "END\n";
    return str;
}

string GLHistogram::GetFragmentProgramHist(UINT32 bins)
{
    // input constant vectors
    // vector<UINT32> pixel_fails  = how large to make ith column
    // vector<UINT32> pixel_thresh = threshold of ith column
    // UINT32         num_bin      = num bins in histogram
    UINT32 a = bins*2+1; //cbuff size;
    ostringstream oss;
    oss << ""
        "!!LWfp4.0\n"
        "# cgc version 3.0.0016, build date Feb 11 2011\n"
        "# command line args: -profile gp4fp -oglsl\n"
        "# source file: temp.txt\n"
        "#vendor LWPU Corporation\n"
        "#version 3.0.0.16\n"
        "#profile gp4fp\n"
        "#program main\n"
        "#semantic pixel_fails\n"
        "#semantic pixel_thresh\n"
        "#var float4 gl_FragCoord : $vin.WPOS : WPOS : -1 : 1\n"
        "#var float4 gl_FragColor : $vout.COLOR : COL0[0] : -1 : 1\n";
    oss << "BUFFER c[" << a <<" ] = { program.buffer[0][0.."<< (a-1) << "] };\n";  //PaBO
    oss <<
        "TEMP R0;\n"
        "TEMP RC, HC;\n"
        "OUTPUT result_color0 = result.color;\n"
        "TRUNC.S R0.x, fragment.position;\n";
    oss << "SGT.S R0.y, R0.x, c["<<(a-1)<<"].x;\n";
    oss <<
        "MOV.U.CC RC.x, -R0.y;\n"
        "MOV.U R0.x, {1};\n"
        "IF    NE.x;\n"
        "MOV.F result_color0, {0.0,0.0,0.0,1}.xyzw;\n" //Bin out of range
        "MOV.U R0.x, {0};\n"
        "ENDIF;\n"
        "MOV.U.CC RC.x, R0;\n"
        "IF    NE.x;\n"
        "TRUNC.S R0.x, fragment.position;\n"
        "MOV.U R0.y, R0.x;\n";
    oss << "MOV.F R0.x, c[R0.y + " << bins << "];\n";
    oss <<
        "TRUNC.S R0.w, R0.x;\n"
        "TRUNC.S R0.z, fragment.position.y;\n"
        "SEQ.S R0.z, R0, R0.w;\n"
        "MOV.U.CC RC.x, -R0.z;\n"
        "MOV.F R0.y, c[R0.y].x;\n"
        "IF    NE.x;\n"
        "SGE.F R0.x, R0.y, R0;\n"
        "TRUNC.U.CC HC.x, R0;\n"
        "IF    NE.x;\n"
        "MOV.F result_color0, {1,0,0,1}.xyzw;\n" //threshold reached
        "ELSE;\n"
        "MOV.F result_color0, {0,1,1,1}.xyzw;\n" //threshold not reached
        "ENDIF;\n"
        "ELSE;\n"
        "SGT.F R0.z, fragment.position.y, R0.x;\n"
        "TRUNC.U.CC HC.x, R0.z;\n"
        "IF    NE.x;\n"
        "SLE.F R0.x, fragment.position.y, R0.y;\n"
        "TRUNC.U.CC HC.x, R0;\n"
        "IF    NE.x;\n"
        "MOV.F result_color0, {1,0,0,1}.xyzw;\n" // failed - hit red channel
        "ELSE;\n"
        "MOV.F result_color0, {0, 0, 0, 1}.xyzw;\n"//not drawn but above threshold
        "ENDIF;\n"
        "ELSE;\n"
        "SLT.F R0.x, fragment.position.y, R0;\n"
        "TRUNC.U.CC HC.x, R0;\n"
        "IF    NE.x;\n"
        "SLE.F R0.x, fragment.position.y, R0.y;\n"
        "TRUNC.U.CC HC.x, R0;\n"
        "IF    NE.x;\n"
        "MOV.F result_color0, {0,1,0,1}.xyzw;\n" //passed under threshold
        "ELSE;\n"
        "MOV.F result_color0, {0,0.0,0.0, 1}.xyzw;\n"//not drawn but under threshold
        "ENDIF;\n"
        "ENDIF;\n"
        "ENDIF;\n"
        "ENDIF;\n"
        "ENDIF;\n"
        "END\n"
        "# 52 instructions, 1 R-regs\n";

    return oss.str();
}

RC GLHistogram::DumpImage()
{
    GLubyte* l_ScreenBuf = new GLubyte [m_ScreenWidth * m_ScreenHeight * 4];
    const mglTexFmtInfo * pFmtInfo =
        mglGetTexFmtInfo(mglTexFmt2Color(m_mglTestContext.ColorSurfaceFormat()));

    glReadPixels(
        0,
        0,
        m_ScreenWidth,
        m_ScreenHeight,
        pFmtInfo->ExtFormat,
        pFmtInfo->Type,
        l_ScreenBuf
        );

    ImageFile::WritePng(
        "glhist.png",
        pFmtInfo->ExtColorFormat,
        l_ScreenBuf,
        m_ScreenWidth,
        m_ScreenHeight,
        m_ScreenWidth * 4,
        false,
        true);

    delete[] l_ScreenBuf;
    return OK;
}
