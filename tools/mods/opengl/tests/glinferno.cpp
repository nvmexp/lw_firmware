/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <math.h>
#include <sstream>
#include "glinferno.h"
#include "gpu/include/gpudev.h"
#include "core/include/imagefil.h"
#include "core/include/version.h"
#include "core/include/utility.h"
#include "random.h"
//#include "core/include/imagefil.h"
//#include "opengl/glgpugoldensurfaces.h"
#include "core/include/display.h"

//#include "core/include/utility.h"

using namespace std;

struct format_struct
{
    GLuint internal_format;
    GLuint external_format;
    GLuint external_data;
    GLuint data_size;
};

static struct format_struct formats[] =
{
    {GL_RGBA8,              GL_RGBA,               GL_UNSIGNED_BYTE,         4},
    {GL_DEPTH_STENCIL,      GL_DEPTH_STENCIL,      GL_UNSIGNED_INT_24_8_EXT, 4},
    {GL_RGBA16F,            GL_RGBA,               GL_HALF_FLOAT,            8},
    {GL_SRGB8_ALPHA8,       GL_RGBA,               GL_UNSIGNED_BYTE,         4},
    {GL_RGBA8,              GL_RGBA,               GL_UNSIGNED_BYTE,         4},
};

GLInfernoTest::GLInfernoTest()
    : m_Rand(GetFpContext()->Rand)
    , m_HighestGPCIdx(0)
#define GLINFERNO_OPTS(A,B,C,D)    ,m_##A(C)
#define GLINFERNO_GLOBALS(A,B,C,D) ,m_##A(C)
#include "glinferno_vars.h"
#undef GLINFERNO_OPTS
#undef GLINFERNO_GLOBALS
    , NUM_TRI(0)
    , VERTS_PER_TRI(0)
    , VERTICES(0)
    , FLOAT_PER_VERTEX(0)
    , zlwll_strip_ptr(0)
    , fullscreen_ptr(0)
    , lwll_ptr(0)
    , lwll_count(0)
{
    SetName("GLInfernoTest");

}

GLInfernoTest::~GLInfernoTest()
{
}

bool GLInfernoTest::IsSupported()
{
    return mglHwIsSupported(GetBoundGpuSubdevice());
}

RC GLInfernoTest::InitFromJs()
{
    RC rc;
    CHECK_RC( GpuMemTest::InitFromJs() );
    CHECK_RC( m_mglTestContext.SetProperties(
                  GetTestConfiguration(),
                  false,      // double buffer
                  GetBoundGpuDevice(),
                  GetDispMgrReqs(),
                  0,          // don't override color format
                  false,      //we explicitly enforce FBOs depending on the testmode.
                  false,      //don't render to sysmem
                  0));        //do not use layered FBO

    if (m_NumActiveGPCs)
    {
        if (m_PulseMode == PULSE_DISABLED)
        {
            // For an easier UI, non-zero NumActiveGPCs should activate the mode:
            m_PulseMode = PULSE_TILE_MASK_GPC_SHUFFLE;
        }
        else if (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE)
        {
            Printf(Tee::PriError, "PulseMode can't be set to %d when NumActiveGPCs=%d\n",
                m_PulseMode, m_NumActiveGPCs);
            return RC::ILWALID_ARGUMENT;
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void GLInfernoTest::PrintJsProperties(Tee::Priority pri)
{
    GpuMemTest::PrintJsProperties(pri);
    //:? RTTI?
}

RC GLInfernoTest::Setup()
{
    m_Rand.SeedRandom(m_RandomSeed);

    VERTS_PER_TRI   = 3;
    zlwll_strip_ptr = (sizeof(GLuint)*(VERTS_PER_TRI*(1)));
    fullscreen_ptr  =
        (sizeof(GLuint)*(VERTS_PER_TRI*(1 + ZLwllStripPrimCount())));
    lwll_ptr        =
        (sizeof(GLuint)*(VERTS_PER_TRI*(1 + m_FullscreenPrimCount + ZLwllStripPrimCount())));
    lwll_count      =
        (m_ZlwllPrimCount + m_BackfaceLwllPrimCount+ m_FrustumLwllPrimCount);
    NUM_TRI                     = 1 +
        m_FullscreenPrimCount + ZLwllStripPrimCount() + m_ZlwllPrimCount +
        m_BackfaceLwllPrimCount + m_FrustumLwllPrimCount;
    VERTS_PER_TRI               = 3;
    VERTICES                    = NUM_TRI*VERTS_PER_TRI;
    FLOAT_PER_VERTEX            = 3+3+2;  // xyz rgb st -- but we don't actually use rgb

    RC rc;
    CHECK_RC(GpuMemTest::Setup());
    CHECK_RC(m_mglTestContext.Setup());

    // Depth & Color
    glEnable(GL_DEPTH_TEST); CHECK_OPENGL_ERROR;
    glDepthMask(GL_TRUE); CHECK_OPENGL_ERROR;

    // Color Logic Operation
    if(m_ErrorCheck)
    {
        glLogicOp(GL_XOR);
        glEnable(GL_COLOR_LOGIC_OP);
    }

    //lwlling
    if(m_BackfaceLwllPrimCount > 0)
    {
        glEnable(GL_LWLL_FACE); CHECK_OPENGL_ERROR;
        glLwllFace(GL_FRONT); CHECK_OPENGL_ERROR; // our fullscreen prims a bit backwards.  :)
    }

    // blending
    switch(m_BlendLevel)
    {
    case 1:
        glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_DST_COLOR); CHECK_OPENGL_ERROR; // >= 4ppc
        break;
    case 2:
        glBlendFunc(GL_ONE_MINUS_SRC_COLOR, GL_ONE_MINUS_DST_COLOR); CHECK_OPENGL_ERROR; // 2ppc
        break;
    }
    glBlendColor(static_cast<GLclampf>(m_BlendR),
                 static_cast<GLclampf>(m_BlendG),
                 static_cast<GLclampf>(m_BlendB),
                 static_cast<GLclampf>(m_BlendA)); CHECK_OPENGL_ERROR; // any color will do

    SetupVertices(); CHECK_OPENGL_ERROR;
    SetupTextures(); CHECK_OPENGL_ERROR;
    CHECK_RC(SetupShaders()); CHECK_OPENGL_ERROR;

    glGenQueriesARB(1, &m_ECQuery);

    //Sanity check on ErrorCheckInterval - must be an even number
    if(m_ErrorCheckInterval%2!=0)
        m_ErrorCheckInterval--;

    return mglGetRC();
}

RC GLInfernoTest::Run()
{
    RC rc;
    double         elapsed  = 0;
    UINT32         timerIdx = 0;
    UINT32         loops    = 0;
    mglTimerQuery  timers[2];
    bool           errorChecked = false;

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.Reset();
    }

    //Clear framebuffers
    glBindFramebuffer(GL_FRAMEBUFFER, m_ECFboid);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, m_Fboid);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //Setup render environment
    glBindFramebuffer(GL_FRAMEBUFFER, m_Fboid);      CHECK_OPENGL_ERROR;
    glViewport(0, 0, m_ScreenWidth, m_ScreenHeight); CHECK_OPENGL_ERROR;
    glActiveTexture(GL_TEXTURE0);                    CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D, m_Texid);           CHECK_OPENGL_ERROR;
    glEnable(GL_LWLL_FACE);                          CHECK_OPENGL_ERROR;
    glBindVertexArray(m_Vaoid);                      CHECK_OPENGL_ERROR;
    glUseProgram(m_Prg);                             CHECK_OPENGL_ERROR;

    UINT32 nextEndLoopMs = m_EndLoopPeriodMs;
    while (m_KeepRunning || (elapsed * 1000.0 < m_LoopMs) || (m_ErrorCheck && !errorChecked))
    {
        //get time from two loops ago
        timerIdx = timerIdx ^ 1;
        elapsed += timers[timerIdx].GetSeconds();

        //start fresh loop on this timer
        timers[timerIdx].Begin();
        CHECK_RC(DrawOnce());

        //if ErrorCheck is enabled and (an ErrorCheckInterval of frames has passed
        //                              or we're on an odd frame and we're past where we should have ended because we haven't errorchecked yet)
        //This last part ensures that ErrorCheckFramebuffer is called at least once per Run().
        //If ErrorCheckInterval=0, there will only be one ErrorCheck - once Run() is finished.
        if(m_ErrorCheck && ((m_ErrorCheckInterval && loops%m_ErrorCheckInterval == (m_ErrorCheckInterval-1))
                            || ((loops & 1) && !m_KeepRunning && (elapsed * 1000.0 >= m_LoopMs))))
        {
            CHECK_RC(ErrorCheckFramebuffer());
            errorChecked = true;
        }

        if (loops < m_DumpFrames)
        {
            // The dumps are all black unless BlendLevel 1 or 0 is used
            char filename[64];
            snprintf(filename, sizeof(filename), "gli%04d_c.png", loops);
            DumpImage(filename);
        }

        UpdateGpcShuffle();

        CHECK_RC(DoEndLoop(&nextEndLoopMs, elapsed));

        timers[timerIdx].End();
        loops++;

        if(m_RecompileFlag) {
            m_RecompileFlag = 0;
            CHECK_RC(SetupShaders());
            glUseProgram(m_Prg);
        }
    }

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.PrintReport(GetVerbosePrintPri(), "GPC shuffle");
    }

    return mglGetRC();
}

RC GLInfernoTest::ErrorCheckFramebuffer(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_ECFboid);        CHECK_OPENGL_ERROR;
    glUseProgram(m_ECPrg);                               CHECK_OPENGL_ERROR;
    glActiveTexture(GL_TEXTURE1);                        CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_Colorid); CHECK_OPENGL_ERROR;

    glDisable(GL_COLOR_LOGIC_OP); CHECK_OPENGL_ERROR;
    glDisable(GL_BLEND);          CHECK_OPENGL_ERROR;
    glDepthFunc(GL_ALWAYS);       CHECK_OPENGL_ERROR;

    glBeginQueryARB(GL_SAMPLES_PASSED, m_ECQuery);                   CHECK_OPENGL_ERROR;
    glDrawElements(GL_TRIANGLES, VERTS_PER_TRI, GL_UNSIGNED_INT, 0); CHECK_OPENGL_ERROR;
    glEndQueryARB(GL_SAMPLES_PASSED);                                CHECK_OPENGL_ERROR;

    glEnable(GL_COLOR_LOGIC_OP); CHECK_OPENGL_ERROR;

    GLuint passedFragments = 1;
    glGetQueryObjectuivARB(m_ECQuery, GL_QUERY_RESULT, &passedFragments); CHECK_OPENGL_ERROR;
    if(passedFragments != 0)
    {
        //No fragments should pass if the past two images were the same
        Printf(Tee::PriError, "Error Check Failed: Failed Pixels %d\n", passedFragments);
        return RC(RC::GOLDEN_VALUE_MISCOMPARE);
    }

    glActiveTexture(GL_TEXTURE1);                CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0); CHECK_OPENGL_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, m_Fboid);  CHECK_OPENGL_ERROR;
    glUseProgram(m_Prg);                         CHECK_OPENGL_ERROR;

    return mglGetRC();
}

void GLInfernoTest::UpdateGpcShuffle()
{
    if (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE)
        return;

    UINT32 tileMask = 0;
    while (static_cast<UINT32>(Utility::CountBits(tileMask)) < m_NumActiveGPCs)
    {
        const UINT32 gpcIdx = GetFpContext()->Rand.GetRandom(0, m_HighestGPCIdx);
        tileMask |= 1 << gpcIdx;
    }

    Printf(Tee::PriDebug, "GPC shuffle tileMask = 0x%08x\n", tileMask);
    m_GPCShuffleStats.AddPoint(tileMask);

    glUniform1ui(glGetUniformLocation(m_Prg, "tileMask"), tileMask); CHECK_OPENGL_ERROR;
}

void GLInfernoTest::DumpImage(const char *filename)
{
    const mglTexFmtInfo *pFmtInfo = mglGetTexFmtInfo(mglTexFmt2Color(GL_RGBA8));
    const UINT32 bpp = ColorUtils::PixelBytes(pFmtInfo->ExtColorFormat);
    const UINT32 size = m_ScreenWidth * m_ScreenHeight * bpp;

    if (size > m_ScreenBuf.size())
    {
        m_ScreenBuf.resize(size);
    }

    glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_Fboid); CHECK_OPENGL_ERROR;
    glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_DumpFboid); CHECK_OPENGL_ERROR;
    glBlitFramebufferEXT(0, 0, m_ScreenWidth, m_ScreenHeight, 0, 0,
        m_ScreenWidth, m_ScreenHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST); CHECK_OPENGL_ERROR;
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_DumpFboid); CHECK_OPENGL_ERROR;
    glReadPixels(
        0,
        0,
        m_ScreenWidth,
        m_ScreenHeight,
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        &m_ScreenBuf[0]); CHECK_OPENGL_ERROR;

    ImageFile::WritePng(
        filename,
        ColorUtils::ColorDepthToFormat(32),
        &m_ScreenBuf[0],
        m_ScreenWidth,
        m_ScreenHeight,
        m_ScreenWidth * 4,
        false,
        false);

    Printf(Tee::PriNormal, "Wrote image file %s.\n", filename);

    glBindFramebuffer(GL_FRAMEBUFFER, m_Fboid); CHECK_OPENGL_ERROR;
}

RC GLInfernoTest::DoEndLoop(UINT32 * pNextEndLoopMs, double l_Elapsed)
{
    RC rc;
    if ((l_Elapsed  * 1000.0) >= *pNextEndLoopMs)
    {
        rc = EndLoop(*pNextEndLoopMs);
        //if (rc == OK)
        //    rc = CheckBufferGPU();
        *pNextEndLoopMs += m_EndLoopPeriodMs;
    }
    return rc;
}

RC GLInfernoTest::DrawOnce()
{
    // start with blending & ztest disabled on first triangle -- clear
    glDisable        (GL_BLEND);                CHECK_OPENGL_ERROR;
    glDepthFunc      (GL_ALWAYS);               CHECK_OPENGL_ERROR;

    //Effectively a glClear
    glDrawElements   (GL_TRIANGLES, VERTS_PER_TRI, GL_UNSIGNED_INT, 0); CHECK_OPENGL_ERROR;
    if(m_WFILevel > 0)
    {
        glFinish(); CHECK_OPENGL_ERROR;
    }

    // enable blending & depth compare on subsequent triangles
    glEnable(GL_BLEND);   CHECK_OPENGL_ERROR;
    glDepthFunc(GL_LESS); CHECK_OPENGL_ERROR;

    //zlwll strip section
    if(ZLwllStripPrimCount() > 0)
    {
        glDrawElements(GL_TRIANGLES,
                       VERTS_PER_TRI*ZLwllStripPrimCount(),
                       GL_UNSIGNED_INT,
                       reinterpret_cast<const GLvoid*>((size_t)zlwll_strip_ptr));  CHECK_OPENGL_ERROR;
    }

    //main mode section
    for(UINT32 bigTriIdx = 0; bigTriIdx < m_FullscreenPrimCount; ++bigTriIdx)
    {
        //sample mask manip
        if(m_SampleMaskEnable > 0)
        {
            switch(bigTriIdx)
            {
            case 0:
                glEnable(GL_SAMPLE_COVERAGE_ARB); CHECK_OPENGL_ERROR;
                glSampleMaski(0, m_SampleMask0);  CHECK_OPENGL_ERROR;
                glEnable(GL_SCISSOR_TEST);        CHECK_OPENGL_ERROR;
                glScissor(0,0,m_SampleMaskScissorWidth, m_SampleMaskScissorHeight); CHECK_OPENGL_ERROR;
                break;
            case 1:
                glSampleMaski(0, m_SampleMask1);  CHECK_OPENGL_ERROR;
                break;
            case 2:
                glDisable(GL_SAMPLE_COVERAGE_ARB); CHECK_OPENGL_ERROR;
                glSampleMaski(0,0xFFFF);          CHECK_OPENGL_ERROR;
                glDisable(GL_SCISSOR_TEST);       CHECK_OPENGL_ERROR;
                glScissor(0,0,m_ScreenWidth, m_ScreenHeight); CHECK_OPENGL_ERROR;
                break;
            default:
                break;
            }
        }

        // fullscreen prim
        glDrawElements(GL_TRIANGLES,
                       VERTS_PER_TRI,
                       GL_UNSIGNED_INT,
                       (const GLvoid*)(fullscreen_ptr+sizeof(GLuint)*bigTriIdx));
        CHECK_OPENGL_ERROR;

        // zlwll + backface + frustum prims (same over-n-over)
        glDrawElements(GL_TRIANGLES,
                       VERTS_PER_TRI*lwll_count,
                       GL_UNSIGNED_INT,
                       reinterpret_cast<const GLvoid*>((size_t)lwll_ptr));
        CHECK_OPENGL_ERROR;

        if(m_WFILevel > 1)
        {
            glFinish(); CHECK_OPENGL_ERROR;
        }
    }

    return mglGetRC();
}

RC GLInfernoTest::Cleanup()
{
    StickyRC firstRc;
    if (m_mglTestContext.IsGLInitialized())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_FRONT);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0 );
        //glDisableVertexAttribArray(0);
        //glDisableVertexAttribArray(1);
        //glDisableVertexAttribArray(2);
        glBindVertexArray(0);
        glUseProgram(0);
        firstRc = mglGetRC();

//WHY, OPENGL, WHY?!?!?
#define RECLAIM1(X,Y) if(X) {Y(1, (const GLuint*)&X); X =0; }
#define RECLAIM0(X,Y) if(X) {Y(X); X =0; }
        RECLAIM1(m_Fboid,   glDeleteFramebuffersEXT);
        RECLAIM1(m_Colorid, glDeleteTextures);
        RECLAIM1(m_Depthid, glDeleteTextures);
        RECLAIM1(m_ECFboid,glDeleteFramebuffersEXT);
        RECLAIM1(m_ECColorid, glDeleteTextures);
        RECLAIM1(m_ECDepthid, glDeleteTextures);
        if (m_DumpFrames)
        {
            RECLAIM1(m_DumpColorid, glDeleteRenderbuffersEXT);
            RECLAIM1(m_DumpFboid, glDeleteFramebuffersEXT);
        }
        RECLAIM1(m_ECQuery,   glDeleteQueriesARB);
        RECLAIM1(m_Vboid,   glDeleteBuffers);
        RECLAIM1(m_Idxid,   glDeleteBuffers);
        RECLAIM1(m_Vaoid,   glDeleteVertexArrays);
        RECLAIM0(m_Ps,      glDeleteShader)
        RECLAIM0(m_Vs,      glDeleteShader)
        RECLAIM0(m_ECPs,    glDeleteShader)
        RECLAIM0(m_Prg,     glDeleteProgram)
        RECLAIM0(m_ECPrg,  glDeleteProgram)
        firstRc = mglGetRC();
#undef RECLAIM1
#undef RECLAIM0
    }

    if (m_mglTestContext.GetDisplay() )
    {
        firstRc = m_mglTestContext.GetDisplay()->TurnScreenOff(false);
    }
    firstRc = m_mglTestContext.Cleanup();
    firstRc = GpuMemTest::Cleanup();

    return firstRc;
}

void GLInfernoTest::ReportErrors(UINT32 errs)
{
}

float GLInfernoTest::LwFloatRand(GLfloat min, GLfloat max)
{
    return m_Rand.GetRandomFloat(min,max);
}

void GLInfernoTest::GetTextureRatios(float *sxRatio,float *tyRatio,float *maxS,float *maxT)
{
    // start with 1:2-epsilon pixel:texel ratio to get a good tex bank ratio.  then scale by aniso to get some
    *sxRatio = static_cast<float>((1.9f)*m_TexAniso);
    *tyRatio = (1.9f);
    *maxS = (((float)m_ScreenWidth)/m_TexWidth)*(*sxRatio);
    *maxT = (((float)m_ScreenHeight)/m_TexHeight)*(*tyRatio);
}

int GLInfernoTest::SetupFullscreenPrim(int i, GLfloat* ptr)
{
    int pi = 0;
    float sxRatio, tyRatio, maxS, maxT;
    GetTextureRatios(&sxRatio,&tyRatio,&maxS,&maxT);

    // Color:
    // ??? using alpha for selecting the temp storage container
    //
    // Texture coords:
    // want 0,maxT over -1,1=2, so specify 0,2maxT over -1,3
    //
    // Depth:
    // eye looking down -Z axis
    // origin point at -1 + dZ looks to eye as Z=~1.  others at 0.5 puts eyeZ as ~.25+dZ
    // also dz at dx=2 means 2*dz at dx=4
    ptr[pi++] = -1.0f;                   // X
    ptr[pi++] = -1.0f;                   // Y
    ptr[pi++] = static_cast<GLfloat>(-1.0f + m_DeltaZ*i);      // Z
    ptr[pi++] =  1.0f;                   // R
    ptr[pi++] =  0.5f;                   // G
    ptr[pi++] =  1.0f;                   // B
    ptr[pi++] =  static_cast<GLfloat>(i*m_TexIterOffsetS + 0.0f); // S
    ptr[pi++] =  static_cast<GLfloat>(i*m_TexIterOffsetT + 0.0f); // T

    ptr[pi++] = -1.0f;                   // X
    ptr[pi++] =  3.0f;                   // Y
    ptr[pi++] =  static_cast<GLfloat>(0.5f + 2.0f*m_DeltaZ*i); // Z
    ptr[pi++] =  1.0f;                   // R
    ptr[pi++] =  1.0f;                   // G
    ptr[pi++] =  0.5f;                   // B
    ptr[pi++] =  static_cast<GLfloat>(i*m_TexIterOffsetS + 0.0f);      // S
    ptr[pi++] =  static_cast<GLfloat>(i*m_TexIterOffsetT + 2.0f*maxT); // T

    ptr[pi++] =  3.0f;                   // X
    ptr[pi++] = -1.0f;                   // Y
    ptr[pi++] =  static_cast<GLfloat>(0.5f + 2.0f*m_DeltaZ*i); // Z
    ptr[pi++] =  0.5f;                   // R
    ptr[pi++] =  1.0f;                   // G
    ptr[pi++] =  1.0f;                   // B
    ptr[pi++] =  static_cast<GLfloat>(i*m_TexIterOffsetS + 2.0f*maxS); // S
    ptr[pi++] =  static_cast<GLfloat>(i*m_TexIterOffsetT + 0.0f);      // T

    return(pi);
}

// ======================================================================
// fill in m_FrustumLwllPrimCount prims from ptr[0] on.
// return offset count from ptr for next vertices.
int GLInfernoTest::SetupFrustumLwllPrims(GLfloat* ptr, unsigned int FLOAT_PER_TRI)
{
    int pi = 0;

    for(unsigned int j=0; j < m_FrustumLwllPrimCount; ++j)
    {
        ptr[pi++] = LwFloatRand(-100.0,-2.0); // X
        ptr[pi++] = LwFloatRand(-100.0,-2.0); // Y
        ptr[pi++] = LwFloatRand(-100.0,100.0); // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T

        ptr[pi++] = LwFloatRand(-100.0,-2.0); // X
        ptr[pi++] = LwFloatRand(-100.0,-2.0); // Y
        ptr[pi++] = LwFloatRand(-100.0,100.0); // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T

        ptr[pi++] = LwFloatRand(-100.0,-2.0); // X
        ptr[pi++] = LwFloatRand(-100.0,-2.0); // Y
        ptr[pi++] = LwFloatRand(-100.0,100.0); // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T
    }
    return(pi);
}

// ======================================================================
// fill in m_backface_lwll_prim_count prims from ptr[0] on.
// return offset count from ptr for next vertices.
int GLInfernoTest::SetupBackfaceLwllPrims(GLfloat* ptr, unsigned int FLOAT_PER_TRI)
{
    int pi = 0;
    float w = 0.1f;

    for(unsigned int j=0; j < m_BackfaceLwllPrimCount; ++j)
    {
        float x = LwFloatRand(-1.0f,1.0f-w);
        float y = LwFloatRand(-1.0f,1.0f-w);
        float z = LwFloatRand(-1.0f,1.0f);
        ptr[pi++] = x; // X
        ptr[pi++] = y; // Y
        ptr[pi++] = z; // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T

        ptr[pi++] = x+w; // X
        ptr[pi++] = y;   // Y
        ptr[pi++] = z;   // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T

        ptr[pi++] = x;   // X
        ptr[pi++] = y+w; // Y
        ptr[pi++] = z;   // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T
    }
    return(pi);
}

// ======================================================================
// fill in m_zlwll_prim_count prims from ptr[0] on.
// return offset count from ptr for next vertices.
int GLInfernoTest::SetupZLwllPrims(GLfloat* ptr, int FLOAT_PER_TRI)
{
    int pi = 0;

    for(UINT32 j=0; j < m_ZlwllPrimCount; ++j)
    {
        ptr[pi++] = 0.0;     // X
        ptr[pi++] = 0.0;     // Y
        ptr[pi++] = LwFloatRand(-1.0f,-0.9f);   // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T

        ptr[pi++] = 0.0;     // X
        ptr[pi++] = 3.0;     // Y
        ptr[pi++] = LwFloatRand(-1.0f,-0.9f);   // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T

        ptr[pi++] = 3.0;     // X
        ptr[pi++] = 0.0;     // Y
        ptr[pi++] = LwFloatRand(-1.0f,-0.9f);   // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = LwFloatRand(0.0,1.0);     // S
        ptr[pi++] = LwFloatRand(0.0,1.0);     // T
    }
    return(pi);
}

// ======================================================================
// return count of prims in zlwll strips
int GLInfernoTest::ZLwllStripPrimCount()
{
    if(m_ZlwllStripWidth == 0.0)
    {
        return(0);
    }

    // 2 * (screen width / strip period)
    int v = int(2 * ceil(1.0f*m_ScreenWidth/m_ZlwllStripPeriod));
    return(v);
}

// ======================================================================
// VertexBuffers
// Attrs:
//   1+m_fullscreen_prim_count fullscreen prims
//   m_zlwll_strip prims
//   m_ZlwllPrimCount
//   m_BackfaceLwllPrimCount
//   m_FrustumLwllPrimCount
// Indices:
//   repeat 1+m_fullscreen_prim_count:
//     fullscreen prim
//     if first_pass:
//       m_zlwll_strip prims
//   zlwll_prims
//   backface_lwll_prims
//   frustum_lwll_prims
//
int GLInfernoTest::SetupZLwllStripPrims(GLfloat* ptr, int FLOAT_PER_TRI)
{
    int pi = 0;
    // early exit
    if(m_ZlwllStripWidth == 0.0)
    {
        return(pi);
    }

    float sxRatio, tyRatio, maxS, maxT;
    GetTextureRatios(&sxRatio,&tyRatio,&maxS,&maxT);
    // scale by prim width on the screen
    maxS *= m_ZlwllStripPeriod/m_ScreenWidth;
    // +1 +--+.....+--+..  |3\3  2|
    //    |\.|.....|\-|..  |  \   |
    //    |.\|.....|.\|..  |   \  |
    // -1 +--+.....+--+..  |1  2\1|
    //   -1  W     P  P+W
    // W = Width, P = Period
    // since it has to go from x=-1,1 you 2x it.
    float scaled_zlwll_strip_width  = 2.0f*m_ZlwllStripWidth/m_ScreenWidth;
    float scaled_zlwll_strip_period = 2.0f*m_ZlwllStripPeriod/m_ScreenWidth;
    float x0 = -1.0;
    float x1 = -1 + scaled_zlwll_strip_width;
    while(x0 < 1.0)
    {
        // Bottom Tri
        ptr[pi++] = x0;     // X
        ptr[pi++] = -1.0;   // Y
        ptr[pi++] = 1.0;    // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = 0.0;    // S
        ptr[pi++] = 0.0;    // T

        ptr[pi++] = x0;     // X
        ptr[pi++] = 1.0;    // Y
        ptr[pi++] = 1.0;    // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = 0.0;    // S
        ptr[pi++] = maxT;   // T

        ptr[pi++] = x1;     // X
        ptr[pi++] = -1.0;   // Y
        ptr[pi++] = 1.0;    // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = maxS;   // S
        ptr[pi++] = 0.0;    // T

        // Top Tri
        ptr[pi++] = x1;     // X
        ptr[pi++] = -1.0;   // Y
        ptr[pi++] = 1.0;    // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = maxS;   // S
        ptr[pi++] = 0.0;    // T

        ptr[pi++] = x1;     // X
        ptr[pi++] = 1.0;    // Y
        ptr[pi++] = 1.0;    // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = maxS;   // S
        ptr[pi++] = maxT;   // T

        ptr[pi++] = x0;     // X
        ptr[pi++] = 1.0;    // Y
        ptr[pi++] = 1.0;    // Z
        ptr[pi++] = LwFloatRand(0.0,1.0);     // R
        ptr[pi++] = LwFloatRand(0.0,1.0);     // G
        ptr[pi++] = LwFloatRand(0.0,1.0);     // B
        ptr[pi++] = 0.0;    // S
        ptr[pi++] = maxT;   // T

        x0 += scaled_zlwll_strip_period;
        x1 += scaled_zlwll_strip_period;
    }
    return(pi);
}

void GLInfernoTest::SetupVertices()
{
    unsigned int zlwll_strip_offset,
        zlwll_prim_offset,
        backface_lwll_prim_offset,
        frustum_lwll_prim_offset;
    vector<char> vbuff; // vertices
    vector<char> ibuff; // indices
    GLfloat* ptr;
    GLuint*  ptr2;

    vbuff.resize( VERTICES*FLOAT_PER_VERTEX*sizeof(GLfloat) );
    ptr = (GLfloat*)&vbuff[0];
    for(unsigned int i=0; i < 1+m_FullscreenPrimCount; ++i)
    {
        ptr += SetupFullscreenPrim(i,ptr);
    }
    zlwll_strip_offset = static_cast<unsigned int>((ptr - (GLfloat*)&vbuff[0])/FLOAT_PER_VERTEX);
    ptr += SetupZLwllStripPrims(ptr,VERTS_PER_TRI*FLOAT_PER_VERTEX);

    zlwll_prim_offset = static_cast<unsigned int>((ptr - (GLfloat*)&vbuff[0])/FLOAT_PER_VERTEX);
    ptr += SetupZLwllPrims(ptr,VERTS_PER_TRI*FLOAT_PER_VERTEX);

    backface_lwll_prim_offset = static_cast<unsigned int>((ptr - (GLfloat*)&vbuff[0])/FLOAT_PER_VERTEX);
    ptr += SetupBackfaceLwllPrims(ptr,VERTS_PER_TRI*FLOAT_PER_VERTEX);

    frustum_lwll_prim_offset = static_cast<unsigned int>((ptr - (GLfloat*)&vbuff[0])/FLOAT_PER_VERTEX);
    ptr += SetupFrustumLwllPrims(ptr,VERTS_PER_TRI*FLOAT_PER_VERTEX);

    glGelwertexArrays(1, &m_Vaoid);
    glBindVertexArray(m_Vaoid);

    glGenBuffers(1, &m_Vboid);
    glBindBuffer(GL_ARRAY_BUFFER, m_Vboid);
    glBufferData(GL_ARRAY_BUFFER, VERTICES*FLOAT_PER_VERTEX*sizeof(GLfloat), (void*)&vbuff[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*FLOAT_PER_VERTEX, (GLvoid*)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*FLOAT_PER_VERTEX, (GLvoid*)12);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*FLOAT_PER_VERTEX, (GLvoid*)24);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    const unsigned int NUM_INDEXED_TRIS = 1 + m_FullscreenPrimCount + ZLwllStripPrimCount() + m_ZlwllPrimCount +
        m_BackfaceLwllPrimCount + m_FrustumLwllPrimCount;
    ibuff.resize(VERTS_PER_TRI*NUM_INDEXED_TRIS*sizeof(GLuint));
    ptr2 = (GLuint*)&ibuff[0];
    for (unsigned int i = 0; i < (unsigned int)(1+m_FullscreenPrimCount); i++)
    {
        *ptr2++ = (GLuint)VERTS_PER_TRI*i+0;
        *ptr2++ = (GLuint)VERTS_PER_TRI*i+1;
        *ptr2++ = (GLuint)VERTS_PER_TRI*i+2;
        if(i == 0)
        {
            for(unsigned int j = 0; j < (unsigned int)ZLwllStripPrimCount(); j++)
            {
                *ptr2++ = (GLuint)zlwll_strip_offset+VERTS_PER_TRI*j+0;
                *ptr2++ = (GLuint)zlwll_strip_offset+VERTS_PER_TRI*j+1;
                *ptr2++ = (GLuint)zlwll_strip_offset+VERTS_PER_TRI*j+2;
            }
        }
    }
    for(unsigned int j = 0; j < (unsigned int)m_ZlwllPrimCount; j++)
    {
        *ptr2++ = (GLuint)zlwll_prim_offset+VERTS_PER_TRI*j+0;
        *ptr2++ = (GLuint)zlwll_prim_offset+VERTS_PER_TRI*j+1;
        *ptr2++ = (GLuint)zlwll_prim_offset+VERTS_PER_TRI*j+2;
    }
    for(unsigned int j = 0; j < (unsigned int)m_BackfaceLwllPrimCount; j++)
    {
        *ptr2++ = (GLuint)backface_lwll_prim_offset+VERTS_PER_TRI*j+0;
        *ptr2++ = (GLuint)backface_lwll_prim_offset+VERTS_PER_TRI*j+1;
        *ptr2++ = (GLuint)backface_lwll_prim_offset+VERTS_PER_TRI*j+2;
    }
    for(unsigned int j = 0; j < (unsigned int)m_FrustumLwllPrimCount; j++)
    {
        *ptr2++ = (GLuint)frustum_lwll_prim_offset+VERTS_PER_TRI*j+0;
        *ptr2++ = (GLuint)frustum_lwll_prim_offset+VERTS_PER_TRI*j+1;
        *ptr2++ = (GLuint)frustum_lwll_prim_offset+VERTS_PER_TRI*j+2;
    }
    glGenBuffers(1, &m_Idxid);
    //Setup index element state
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Idxid );
    // FIXME -- create a short vs. int buffer depending on VERTS_PER_TRI*NUM_INDEXED_TRIS
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 VERTS_PER_TRI*NUM_INDEXED_TRIS*sizeof(GLuint),
                 (void*)&ibuff[0],
                 GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GLInfernoTest::SetupTextures()
{
    SetupFramebuffer(m_Fboid, m_Colorid, m_Depthid);
    SetupFramebuffer(m_ECFboid, m_ECColorid, m_ECDepthid);
    if (m_DumpFrames)
    {
        SetupDumpFramebuffer(m_DumpFboid, m_DumpColorid);
    }

    //----------------------------------------------
    // Init texture
    //----------------------------------------------
    glGenTextures(1, &m_Texid); CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D, m_Texid); CHECK_OPENGL_ERROR;

    // set texture params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); CHECK_OPENGL_ERROR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); CHECK_OPENGL_ERROR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); CHECK_OPENGL_ERROR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); CHECK_OPENGL_ERROR;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, static_cast<GLfloat>(m_TexAniso)); CHECK_OPENGL_ERROR;

    // set texture image (4 GL_FLOAT format)
    unsigned int texSize = m_TexWidth * m_TexHeight  ;
    vector<GLubyte> data(texSize*formats[m_TexBuffer].data_size);

    if(formats[m_TexBuffer].data_size == 8)
    {
        MyFillTexture(m_TexWidth, m_TexHeight, &data[0]);
    }
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 formats[m_TexBuffer].internal_format,
                 m_TexWidth,
                 m_TexHeight,
                 0,
                 formats[m_TexBuffer].external_format,
                 formats[m_TexBuffer].external_data,
                 &data[0]); CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D, 0); CHECK_OPENGL_ERROR;
}

void GLInfernoTest::SetupFramebuffer(GLuint& fboid, GLuint& colorid, GLuint& depthid)
{
    //----------------------------------------------
    // FBO
    //----------------------------------------------
    glGenFramebuffers(1, &fboid); CHECK_OPENGL_ERROR;
    glBindFramebuffer(GL_FRAMEBUFFER, fboid); CHECK_OPENGL_ERROR;

    //----------------------------------------------
    // RTT COLOR
    //----------------------------------------------
    glGenTextures(1, &colorid); CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorid); CHECK_OPENGL_ERROR;

    glTexElwf(GL_TEXTURE_ELW, GL_TEXTURE_ELW_MODE, GL_REPLACE); CHECK_OPENGL_ERROR;

    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                            m_Multisample,
                            formats[m_ColorBuffer].internal_format,
                            m_ScreenWidth,
                            m_ScreenHeight,
                            GL_TRUE); CHECK_OPENGL_ERROR;

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D_MULTISAMPLE,
                           colorid,
                           0); CHECK_OPENGL_ERROR;

    //----------------------------------------------
    // RTT Depth
    //----------------------------------------------
    glGenTextures(1, &depthid); CHECK_OPENGL_ERROR;
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, depthid); CHECK_OPENGL_ERROR;

    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                            m_Multisample,
                            formats[m_DepthBuffer].internal_format,
                            m_ScreenWidth,
                            m_ScreenHeight,
                            GL_TRUE); CHECK_OPENGL_ERROR;

    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D_MULTISAMPLE,
                           depthid,
                           0); CHECK_OPENGL_ERROR;

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE,0); CHECK_OPENGL_ERROR;

    glCheckFramebufferStatus( GL_FRAMEBUFFER ); CHECK_OPENGL_ERROR;

    glBindFramebuffer(GL_FRAMEBUFFER, 0); CHECK_OPENGL_ERROR;
}

void GLInfernoTest::SetupDumpFramebuffer(GLuint& fboid, GLuint& colorid)
{
    //----------------------------------------------
    // FBO
    //----------------------------------------------
    glGenFramebuffers(1, &fboid); CHECK_OPENGL_ERROR;
    glBindFramebufferEXT(GL_FRAMEBUFFER, fboid); CHECK_OPENGL_ERROR;

    //----------------------------------------------
    // RTT COLOR
    //----------------------------------------------
    glGenRenderbuffersEXT(1, &colorid); CHECK_OPENGL_ERROR;
    glBindRenderbufferEXT(GL_RENDERBUFFER, colorid); CHECK_OPENGL_ERROR;
    glRenderbufferStorageEXT(GL_RENDERBUFFER, formats[m_ColorBuffer].internal_format,
        m_ScreenWidth, m_ScreenHeight); CHECK_OPENGL_ERROR;
    glFramebufferRenderbufferEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_RENDERBUFFER, colorid); CHECK_OPENGL_ERROR;

    glCheckFramebufferStatus( GL_FRAMEBUFFER ); CHECK_OPENGL_ERROR;

    glBindFramebufferEXT(GL_FRAMEBUFFER, 0); CHECK_OPENGL_ERROR;
}

void GLInfernoTest::LwFloat32ToFloat32Raw(const GLfloat &from, GLuint &to)
{
    union
    {
        GLuint  u;
        GLfloat f;
    };
    f  = from;
    to = u;
}

void GLInfernoTest::LwFloat32ToFloat16Raw(const GLfloat &from, GLhalfLW &to)
{
    to = Utility::Float32ToFloat16(from);
}

void GLInfernoTest::MyFillTexture(unsigned int texw, unsigned int texh, GLubyte* image)
{
    GLhalfLW* tp = (GLhalfLW*)image;
    for (unsigned int y = 0; y < texh; y++)
    {
        for (unsigned int x = 0; x < texw; x++)
        {
            for (int i = 0; i < 4; ++i)
            {
                GLhalfLW tmpH;
                GLfloat tmpF = static_cast<GLfloat>(
                    m_TexCoeffA * sin(2*3.1415926535*(float)x/texw) +
                    m_TexCoeffB +
                    m_TexCoeffC * m_Rand.GetRandomFloat(-999.0f, 999.0f));
                LwFloat32ToFloat16Raw(tmpF,tmpH);
                *tp++ = tmpH;
            }
        }
    }
}

void GLInfernoTest::PrintShaderInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
#define SHADERIV_FUNC(X) {                               \
    int temp;                                            \
    glGetShaderiv(obj, X, &temp);                        \
    Printf(Tee::PriDebug, "%30s = 0x%08x;\n", #X, temp);\
    }
    SHADERIV_FUNC(GL_SHADER_TYPE);
    SHADERIV_FUNC(GL_DELETE_STATUS);
    SHADERIV_FUNC(GL_COMPILE_STATUS);
    SHADERIV_FUNC(GL_INFO_LOG_LENGTH);
    SHADERIV_FUNC(GL_SHADER_SOURCE_LENGTH);
#undef SHADERIV_FUNC

    glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);
    if (infologLength > 1)
    {
        vector<char> buff_info(infologLength+1);
        char* infoLog = &buff_info[0];
        glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
        Printf(Tee::PriNormal, "SHADER INFO LOG : \n%s\n", infoLog);
    }
}

void GLInfernoTest::PrintProgramInfoLog(GLuint obj)
{
    int infologLength = 0;
    int charsWritten  = 0;
#define PROGRAMIV_FUNC(X) {                                  \
    int temp_##X;                                            \
    glGetProgramiv(obj, X, &temp_##X);                       \
    Printf(Tee::PriDebug, "%40s = 0x%08x;\n", #X, temp_##X);\
    }
    PROGRAMIV_FUNC(GL_DELETE_STATUS);
    PROGRAMIV_FUNC(GL_LINK_STATUS);
    PROGRAMIV_FUNC(GL_VALIDATE_STATUS);
    PROGRAMIV_FUNC(GL_INFO_LOG_LENGTH);
    PROGRAMIV_FUNC(GL_ATTACHED_SHADERS);
    PROGRAMIV_FUNC(GL_ACTIVE_ATTRIBUTES);
    PROGRAMIV_FUNC(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH) ;
    PROGRAMIV_FUNC(GL_ACTIVE_UNIFORMS);
    PROGRAMIV_FUNC(GL_ACTIVE_UNIFORM_BLOCKS);
    PROGRAMIV_FUNC(GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH) ;
    PROGRAMIV_FUNC(GL_ACTIVE_UNIFORM_MAX_LENGTH);
#undef PROGRAMIV_FUNC

    GLint pgm_aa, pgm_au;
    GLint pgm_aa_len, pgm_au_len;
    GLint length;
    GLenum type;
    GLsizei size;
    glGetProgramiv(obj, GL_ACTIVE_ATTRIBUTES,&pgm_aa);
    glGetProgramiv(obj, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH,&pgm_aa_len);
    glGetProgramiv(obj, GL_ACTIVE_UNIFORMS, &pgm_au);
    glGetProgramiv(obj, GL_ACTIVE_UNIFORM_MAX_LENGTH, &pgm_au_len);
    glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

    if (infologLength > 1)
    {
        vector<char> buff_info(infologLength+1);
        char* infoLog = &buff_info[0];
        glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
        Printf(Tee::PriNormal, "PROGRAM INFO LOG: \n%s\n", infoLog);
    }

    vector<char> buff_aa_v(pgm_aa_len+1);
    char* buff_aa = &buff_aa_v[0];
    for( GLint i = 0 ; i < pgm_aa; i++)
    {
        glGetActiveAttrib(obj, i, pgm_aa_len+1, &length, &size, &type, &buff_aa[0]);
        Printf(Tee::PriDebug, "ATTR %d: %d %d %d %s\n", i, length, size, type, buff_aa);
        Printf(Tee::PriDebug, " @LOC %d\n", glGetAttribLocation(obj, buff_aa));
    }

    vector<char> buff_au_v(pgm_au_len+1);
    char* buff_au = &buff_au_v[0];
    for (GLint i = 0; i < pgm_au; i++)
    {
        glGetActiveUniform(obj, i, pgm_au_len+1, &length, &size, &type, &buff_au[0]);
        Printf(Tee::PriDebug, "UNIFORM %d: %d %d %d %s\n", i, length, size, type, buff_au);
        Printf(Tee::PriDebug, " @LOC %d\n", glGetUniformLocation(obj, buff_au));

    }
}

RC GLInfernoTest::SetupShaders()
{
    string str;

    const bool gpcPulsing =
        (m_PulseMode == PULSE_TILE_MASK_GPC) ||
        (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE);
    const bool tileMasking =
        (m_PulseMode == PULSE_TILE_MASK_USER) ||
        gpcPulsing;

    // header
    str +=
        "#version 150\n";

    if (tileMasking)
    {
        str +=
            "#extension GL_LW_shader_thread_group : enable\n";
    }
    str +=
        "#extension GL_ARB_shader_bit_encoding : enable\n"
        "#extension GL_ARB_gpu_shader_fp64     : enable\n"
        "in  vec3 ifragColor;\n"
        "in  vec2 ifragTex;\n"
        "out vec4 ofragColor;\n"
        "uniform sampler2D tex;\n";
    if (tileMasking)
    {
        str +=
            "uniform unsigned int tileMask;\n";
    }
    if (gpcPulsing)
    {
        str +=
            "uniform unsigned int gpcId[512];\n";
    }
    str +=
        "\n"
        "void main() \n"
        "{\n";

    switch (m_PulseMode)
    {
        case PULSE_TILE_MASK_USER:
            str +=
                " int tileIndexMod32 = int(gl_FragCoord.x) % 32;\n"
                " if (((1 << tileIndexMod32) & tileMask) == 0) discard;\n";
            break;
        case PULSE_TILE_MASK_GPC:
        case PULSE_TILE_MASK_GPC_SHUFFLE:
            str +=
                " if ( (1 << (gpcId[gl_SMIDLW]) & tileMask) == 0) discard;\n";
            break;
        default:
            break;
    }

    str +=
        "  float  x1, x2, x3, x4, x5, x6;\n"
        "  double d1, d2, d3, d4;\n"
        "  uint i = uint(0x55555555), n, slot;\n"
        "  int foo[2];\n"
        "  slot   = floatBitsToUint(ifragColor.x)&uint(0x1);\n"
        "#define  m 1.0737305f\n"
        "#define dm 1.0737305\n"
        "  float   c = 12345.0f;\n"
        "  double dc = 12345.0;\n"
        "  x1 =  ifragTex.x + ifragTex.y;\n"
        "  x2 =  x1 * -3e3;\n"
        "  x3 =  x1 * 5e5;\n"
        "  x4 =  x1 * 7e7;\n"
        "  x5 =  x1 * .001 + .6789;\n"
        "  x6 =  x1 * -9e9;\n"
        "  d1 = double(x1);\n"
        "  d2 = double(x2);\n"
        "  d3 = double(x3);\n"
        "  d4 = double(x4);\n"
        "  float tmp = .1;\n"
        "  n  =  uint(floatBitsToUint(x1));\n";
    // texture
    if(m_PsKernelTex >= 4)
    {
        str +=
            "  vec4 T0, T1, T2, T3;\n"
            "  float tx;\n"
            "  vec4 STRQ0, STRQ1;\n"
            "  STRQ0 = ifragTex.xyxy;\n"
            "  STRQ0.z = STRQ0.x + 0.0001;\n"
            "  STRQ1.x = STRQ0.x + 0.0002;\n"
            "  STRQ1.y = STRQ0.y;\n"
            "  STRQ1.z = STRQ0.x + 0.0003;\n"
            "  STRQ1.w = STRQ0.y;\n"
            "  T0 = texture(tex, STRQ0.xy);\n"
            "  T1 = texture(tex, STRQ0.zw);\n"
            "  T2 = texture(tex, STRQ1.xy);\n"
            "  T3 = texture(tex, STRQ1.zw);\n"
            "  tx = T0.x*T1.y + T2.z*T3.w;\n";
    }
    if(m_PsKernelTex == 8)
    {
        str +=
            "    STRQ0.y = STRQ0.y + 0.0001;\n"
            "    STRQ0.w = STRQ0.y;\n"
            "    STRQ1.y = STRQ1.y + 0.0003;\n"
            "    STRQ1.w = STRQ1.y;\n"
            "    T0 = texture(tex, STRQ0.xy);\n"  // t.x,       t.y + 0.1
            "    T1 = texture(tex, STRQ0.zw);\n"  // t.x + 0.1, t.y + 0.1
            "    T2 = texture(tex, STRQ1.xy);\n"  // t.x + 0.2, t.y + 0.3
            "    T3 = texture(tex, STRQ1.zw);\n"  // t.x + 0.3, t.y + 0.3
            "    tx = tx + T0.x*T1.y + T2.z*T3.w;\n";
    }
    {
        int i = m_PsKernelRep;
        switch ( m_PsKernelType )
        {
        case 0:
        {
            while(i > 0)
            {
                str +=
                    " x1 = m * x1 + c;\n"
                    " x5 = log2(abs(x5));\n"
                    " x2 = m * x2 + c;\n"
                    " i = i << uint(1);\n"
                    " x3 = m * x3 + c;\n"
                    //" x6 = m * x6 + c;\n"
                    " foo[slot] = int(floatBitsToUint(x4));\n"
                    " x4 = m * x4 + c;\n";
                --i;
                if(i > 0)
                {
                    str +=
                        " x1 = m * x1 + c;\n"
                        " x5 = 1/abs(x5);\n"
                        " x2 = m * x2 + c;\n"
                        " i = n >> i;\n"
                        " x3 = m * x3 + c;\n"
                        //"    x6 = m * x6 + c;\n"
                        " n = uint(foo[slot]);\n"
                        " x4 = m * x4 + c;\n";
                    --i;
                }
            }
        }
        break;
        case 1:
        {
            //to match the other kernels; make a rep 7 insts
            // let the compiler handle ordering
            // FMAD
            //   Reg = m*Reg + c
            // ALU op loop (2 insts) :
            //  reg = log2(abs(reg))
            //  reg = 1/abs(reg)
            // MEM op loop (4 insts) :
            //  reg0 = reg0 << uint(1)
            //  mem0 = uint(floatBitsToUint(reg1));
            //  reg0 = N >> reg0
            //  N = uint(mem0)

            //form choice list
            std::vector<unsigned int> weights;

            for (GLuint j = 0; j < GLuint(m_PsKernelFmadRatio*1000); j++) weights.push_back(0);
            for (GLuint j = 0; j < GLuint(m_PsKernelDmadRatio*1000); j++) weights.push_back(1);
            for (GLuint j = 0; j < GLuint(m_PsKernelMemRatio*1000);  j++) weights.push_back(2);
            for (GLuint j = 0; j < GLuint(m_PsKernelAluRatio*1000);  j++) weights.push_back(3);
            vector<string> FMAD_STRINGS;
            FMAD_STRINGS.push_back(" x1 = m * x1 + c;\n") ;
            FMAD_STRINGS.push_back(" x2 = m * x2 + c;\n") ;
            FMAD_STRINGS.push_back(" x3 = m * x3 + c;\n") ;
            FMAD_STRINGS.push_back(" x4 = m * x4 + c;\n") ;
            vector<string> DMAD_STRINGS;
            DMAD_STRINGS.push_back(" d1 =dm * d1 +dc;\n") ;
            DMAD_STRINGS.push_back(" d2 =dm * d2 +dc;\n") ;
            DMAD_STRINGS.push_back(" d3 =dm * d3 +dc;\n") ;
            DMAD_STRINGS.push_back(" d4 =dm * d4 +dc;\n") ;
            vector<string> MEM_STRINGS;
            MEM_STRINGS.push_back(" i = i << uint(1);\n");
            MEM_STRINGS.push_back(" foo[slot] = int(floatBitsToUint(x1));\n");
            MEM_STRINGS.push_back(" i = n >> i;\n");
            MEM_STRINGS.push_back(" n = n + uint(foo[slot]);\n");
            vector<string> ALU_STRINGS;
            ALU_STRINGS.push_back(" x5 = log2(abs(x5));\n");
            ALU_STRINGS.push_back(" x5 = 1/abs(x5);\n");

            GLuint FMAD_MOD = static_cast<GLuint>(FMAD_STRINGS.size()), FMAD_IDX = 0;
            GLuint DMAD_MOD = static_cast<GLuint>(DMAD_STRINGS.size()),  DMAD_IDX = 0;
            GLuint ALU_MOD  = static_cast<GLuint>(ALU_STRINGS.size()),   ALU_IDX  = 0;
            GLuint MEM_MOD  = static_cast<GLuint>(MEM_STRINGS.size()),   MEM_IDX  = 0;
            GLuint /*TYPE_MOD=3,*/ WEIGHT_MOD = static_cast<GLuint>(weights.size());

#define INST_TYPE(A,B) {           \
    str  += B##_STRINGS[B##_IDX];      \
    B##_IDX = (B##_IDX+1) % B##_MOD; }

            while(i > 0 && WEIGHT_MOD)
            {
                for(GLuint inst=0; inst < 7; inst++)
                {
                    GLuint rand_type = weights[m_Rand.GetRandom() % WEIGHT_MOD];
                    switch(rand_type)
                    {
                    case 0:
                        INST_TYPE(0,FMAD);
                        break;
                    case 1:
                        INST_TYPE(1,DMAD);
                        break;
                    case 2:
                        INST_TYPE(2,MEM);
                        break;
                    case 3:
                        INST_TYPE(3,ALU);
                        break;
                    }
                }
                i--;
            }
            //Close out each op type
            while (FMAD_IDX != 0 && (m_PsKernelFmadRatio > 0) ) INST_TYPE(0,FMAD);
            while (DMAD_IDX != 0 && (m_PsKernelDmadRatio > 0) ) INST_TYPE(1,DMAD);
            while (MEM_IDX  != 0 && (m_PsKernelAluRatio  > 0) ) INST_TYPE(2,MEM);
            while (ALU_IDX  != 0 && (m_PsKernelMemRatio  > 0) ) INST_TYPE(3,ALU);
#undef INST_TYPE

        }
        break;
        }
    }

    // combine the FFMA etc values into RGBA
    str +=
        "    x1 += x2; x4 += x5; \n"
        "    x1 += x3; x1 *= x4; \n"
        "    i = n >> i;\n";
    // combine DFMA values.
    str +=
        "    x1 += float(d2); x1 += float(d1); \n"
        "    x1 += float(d3); x1 *= float(d4); \n";
    // finally add in the texture result
    if(m_PsKernelTex > 0)
    {
        str+=
            "    i += uint(floatBitsToUint(x1));\n"
            "    i = i >> uint(31);\n"          // get rid of math, but in a way the compiler cannot fixup
            "float oc = i/256.0f + tx;\n" // concentrate mainly on TEX results
            "    ofragColor.r =  oc;\n"
            "    ofragColor.g =  oc;\n"
            "    ofragColor.b =  oc;\n"
            "    ofragColor.a =  oc;\n";
    }
    else
    {
        str +=
            "    i += uint(floatBitsToUint(x1));\n"
            "    ofragColor.r =  ((i>> 0)&uint(0xFF)) / 256.0f;\n"
            "    ofragColor.g =  ((i>> 8)&uint(0xFF)) / 256.0f;\n"
            "    ofragColor.b =  ((i>>16)&uint(0xFF)) / 256.0f;\n"
            "    ofragColor.a =  ((i>>24)&uint(0xFF)) / 256.0f;\n";
    }

    str+= "}\n";

    //glsl type binding
    if(m_Prg)
        glDeleteProgram(m_Prg);
    m_Prg = glCreateProgram();

    //
    const GLchar* c = str.c_str();
    if(m_Ps)
        glDeleteShader(m_Ps);
    m_Ps = glCreateShader(GL_FRAGMENT_SHADER);  CHECK_OPENGL_ERROR;
    glShaderSource(m_Ps, 1, &c,NULL);           CHECK_OPENGL_ERROR;
    glCompileShader(m_Ps);                      CHECK_OPENGL_ERROR;
    PrintShaderInfoLog(m_Ps);                   CHECK_OPENGL_ERROR;

    glAttachShader(m_Prg,m_Ps);                 CHECK_OPENGL_ERROR;

    str = "";
    str +=
        "#version 150\n"
        "in vec3 i_position;\n"
        "in vec3 i_color;\n"
        "in vec2 i_texcoord;\n"
        "out  vec3 ifragColor;\n"
        "out  vec2 ifragTex;\n"
        "uniform mat4 mvmatrix;\n"
        "uniform mat4 pmatrix;\n"
        "void main() {\n"
        "   gl_Position = pmatrix*mvmatrix * vec4(i_position, 1.0);\n"
        "   ifragColor     = i_color;\n"
        "   ifragTex       = i_texcoord;\n"
        "}\n";

    c = str.c_str();
    if(m_Vs)
        glDeleteShader(m_Vs);
    m_Vs = glCreateShader(GL_VERTEX_SHADER); CHECK_OPENGL_ERROR;
    glShaderSource(m_Vs, 1, &c, NULL);       CHECK_OPENGL_ERROR;
    glCompileShader(m_Vs);                   CHECK_OPENGL_ERROR;
    PrintShaderInfoLog(m_Vs);                CHECK_OPENGL_ERROR;
    glAttachShader(m_Prg,m_Vs);              CHECK_OPENGL_ERROR;

    glLinkProgram(m_Prg);       CHECK_OPENGL_ERROR;
    PrintProgramInfoLog(m_Prg); CHECK_OPENGL_ERROR;
    glUseProgram(m_Prg);        CHECK_OPENGL_ERROR;

    glBindAttribLocation(m_Prg,0,"i_position"); CHECK_OPENGL_ERROR;
    glBindAttribLocation(m_Prg,1,"i_color");    CHECK_OPENGL_ERROR;
    glBindAttribLocation(m_Prg,2,"i_texcoord"); CHECK_OPENGL_ERROR;

    float data[16] = {1, 0, 0, 0, 0, 1, 0, 0, -0, -0, -1, -0, 0, 0, 0, 1};
    glUniformMatrix4fv(glGetUniformLocation(m_Prg, "pmatrix"),1, GL_FALSE, data);
    float data2[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    glUniformMatrix4fv(glGetUniformLocation(m_Prg, "mvmatrix"),1, GL_FALSE, data2);

    GLuint tex_loc = glGetUniformLocation(m_Prg, "tex"); CHECK_OPENGL_ERROR;
    glUniform1i(tex_loc, 0);    CHECK_OPENGL_ERROR;

    glValidateProgram(m_Prg);   CHECK_OPENGL_ERROR;
    PrintProgramInfoLog(m_Prg); CHECK_OPENGL_ERROR;

    glUseProgram(0); CHECK_OPENGL_ERROR;

    if(m_ECPrg)
        glDeleteProgram(m_ECPrg);
    m_ECPrg = glCreateProgram(); CHECK_OPENGL_ERROR;

    string crc_fragment_string = "";
    crc_fragment_string +=  "#version 150\n"
                            "uniform sampler2DMS errorTex;\n"
                            "in vec3 ifragColor;\n"
                            "in vec2 ifragTex;\n"
                            "out vec4 outColor;\n"
                            "void main (void)\n"
                            "{\n"
                            "    vec4 errorColor = texelFetch(errorTex, ivec2(ifragTex), 0);\n"
                            "    if (errorColor.rgb == vec3(0,0,0))\n"
                            "        discard;\n"
                            "    else\n"
                            "        outColor = vec4(1,0,0,1);\n"
                            "}\n";

    c = crc_fragment_string.c_str();
    if(m_ECPs)
        glDeleteShader(m_ECPs);
    m_ECPs = glCreateShader(GL_FRAGMENT_SHADER);  CHECK_OPENGL_ERROR;
    glShaderSource(m_ECPs, 1, &c,NULL);           CHECK_OPENGL_ERROR;
    glCompileShader(m_ECPs);                      CHECK_OPENGL_ERROR;
    PrintShaderInfoLog(m_ECPs);                   CHECK_OPENGL_ERROR;

    glAttachShader(m_ECPrg,m_ECPs);               CHECK_OPENGL_ERROR;
    glAttachShader(m_ECPrg,m_Vs);                 CHECK_OPENGL_ERROR;

    glLinkProgram(m_ECPrg);                       CHECK_OPENGL_ERROR;
    glUseProgram(m_ECPrg);                        CHECK_OPENGL_ERROR;

    glBindAttribLocation(m_ECPrg,0,"i_position"); CHECK_OPENGL_ERROR;
    glBindAttribLocation(m_ECPrg,1,"i_color");    CHECK_OPENGL_ERROR;
    glBindAttribLocation(m_ECPrg,2,"i_texcoord"); CHECK_OPENGL_ERROR;

    glUniformMatrix4fv(glGetUniformLocation(m_ECPrg, "pmatrix"),1, GL_FALSE, data);
    glUniformMatrix4fv(glGetUniformLocation(m_ECPrg, "mvmatrix"),1, GL_FALSE, data2);

    GLuint errorTex_loc = glGetUniformLocation(m_ECPrg, "errorTex"); CHECK_OPENGL_ERROR;
    glUniform1i(errorTex_loc, 1);                                    CHECK_OPENGL_ERROR;

    glValidateProgram(m_ECPrg);   CHECK_OPENGL_ERROR;
    PrintProgramInfoLog(m_ECPrg); CHECK_OPENGL_ERROR;

    glUseProgram(0); CHECK_OPENGL_ERROR;

    return SetupPulseModeConstants();
}

RC GLInfernoTest::SetupPulseModeConstants()
{
    RC rc;

    if ( (m_PulseMode != PULSE_TILE_MASK_USER) &&
         (m_PulseMode != PULSE_TILE_MASK_GPC) &&
         (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE) )
    {
        return rc;
    }

    GpuSubdevice *subdev = GetBoundGpuSubdevice();

    glUseProgram(m_Prg); CHECK_OPENGL_ERROR;

    if ( (m_PulseMode == PULSE_TILE_MASK_GPC) ||
         (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE) )
    {
        LW2080_CTRL_GR_GET_GLOBAL_SM_ORDER_PARAMS smOrder = {};
        CHECK_RC(LwRmPtr()->ControlBySubdevice(subdev,
            LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER,
            &smOrder, sizeof(smOrder)));

        unsigned int smIdMap[LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER_MAX_SM_COUNT] = { };
        m_HighestGPCIdx = 0;
        for (UINT32 idx = 0; idx < smOrder.numSm; idx++)
        {
            if (smOrder.globalSmId[idx].gpcId > m_HighestGPCIdx)
            {
                m_HighestGPCIdx = smOrder.globalSmId[idx].gpcId;
            }
            smIdMap[idx] = smOrder.globalSmId[idx].gpcId;
        }

        glUniform1uiv(glGetUniformLocation(m_Prg, "gpcId"),
            LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER_MAX_SM_COUNT, smIdMap); CHECK_OPENGL_ERROR;
    }

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        if (m_NumActiveGPCs > (m_HighestGPCIdx + 1))
        {
            m_NumActiveGPCs = m_HighestGPCIdx + 1;
        }
        UpdateGpcShuffle();
    }
    else
    {
        UINT32 tileMask;
        switch(m_PulseMode)
        {
            case PULSE_TILE_MASK_USER:
                tileMask = m_TileMask;
                break;
            case PULSE_TILE_MASK_GPC:
                CHECK_RC(subdev->HwGpcToVirtualGpcMask(m_TileMask, &tileMask));
                break;
            default:
                tileMask = 1;
        }
        glUniform1ui(glGetUniformLocation(m_Prg, "tileMask"), tileMask); CHECK_OPENGL_ERROR;
    }

    glValidateProgram(m_Prg); CHECK_OPENGL_ERROR;
    glUseProgram(0); CHECK_OPENGL_ERROR;

    return rc;
}

JS_CLASS_INHERIT_FULL(GLInfernoTest, GpuMemTest, "GLInferno OpenGL test", 0);

#define GLINFERNO_OPTS(A,B,C,D) \
    CLASS_PROP_READWRITE(GLInfernoTest, A, B, D);
#define GLINFERNO_GLOBALS(A,B,C,D)
#include "glinferno_vars.h"
#undef GLINFERNO_OPTS
#undef GLINFERNO_GLOBALS
