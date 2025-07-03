/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "glcombust.h"
#include "core/include/imagefil.h"
#include "opengl/glgpugoldensurfaces.h"
#include "core/include/display.h"
#include <math.h>
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "core/include/version.h"

GLCombustTest::GLCombustTest()
:  m_pGSurfs(NULL)
    //implied params
   ,m_NumVertices(0)
   ,m_NumIndices(0)
    // EC check
   ,m_PixelErrorTol(0)
    //Viewport stuff
   ,m_WorldX(0)
   ,m_WorldY(0)
    // Sweep stuff
   ,m_FreqSweep(false)
    //Runtime length args
   ,m_LoopMs(20000)
   ,m_KeepRunning(false)
   ,m_DrawRepeats(500)
   ,m_Timers(0)
   ,m_HighestGPCIdx(0)
    //params
   ,m_LwllToggle(GL_FRONT)
   ,m_ExtraInstr(0)
   ,m_ExtraTex(0)
   ,m_ScreenWidth(512)
   ,m_ScreenHeight(512)
   ,m_NumXSeg(59)
   ,m_NumTSeg(59)
   ,m_TorusTubeDia(2.0)
   ,m_TorusHoleDia(4.0)
   ,m_EnableLwll(true)
   ,m_BlendMode(CM_XOR)
   ,m_ShaderHashMode(SHM_NONE)
   ,m_DepthTest(false)
   ,m_UseMM(true)
   ,m_SimpleVS(false)
   ,m_EndLoopPeriodMs(10000)
   ,m_DrawPct(100.0)
   ,m_PulseMode(PULSE_DISABLED)
   ,m_TileMask(1)
   ,m_NumActiveGPCs(0)
   ,m_SkipDrawChecks(false)
   ,m_FailingPixels(0)
   ,m_FramesDrawn(0)
   ,m_FsMode(0)
   ,m_Watts(0)
   ,m_DumpFrames(0)
{
    SetName("GLCombustTest");

    m_WindowRect[0] = m_WindowRect[1] =
        m_WindowRect[2] = m_WindowRect[3] = 0;
    m_ScissorRect[0] = m_ScissorRect[1] =
        m_ScissorRect[2] = m_ScissorRect[3] = 0;

    memset(m_FboIds,   0, sizeof(m_FboIds));
    memset(m_ProgIds,  0, sizeof(m_ProgIds));
    memset(m_VboIds,   0, sizeof(m_VboIds));
    memset(m_OccIds,   0, sizeof(m_OccIds));
    memset(m_ConstIds, 0, sizeof(m_ConstIds));
}

GLCombustTest::~GLCombustTest()
{
}

bool GLCombustTest::IsSupported()
{
    return mglHwIsSupported(GetBoundGpuSubdevice());
}

RC GLCombustTest::InitFromJs()
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
void GLCombustTest::PrintJsProperties(Tee::Priority pri)
{
    GpuMemTest::PrintJsProperties(pri);

    const char * ftStrings[2] = { "false", "true" };

    Printf(pri, "%s controls:\n",              GetName().c_str() );
    Printf(pri, "\tLoopMs:\t\t\t\t%d\n",       m_LoopMs);
    Printf(pri, "\tKeepRunning:\t\t\t%s\n",    ftStrings[m_KeepRunning]);
    Printf(pri, "\textraInstr:\t\t\t%d\n",     m_ExtraInstr);
    Printf(pri, "\textraTex:\t\t\t%d\n",       m_ExtraTex);
    Printf(pri, "\tscreenWidth:\t\t\t%d\n",    m_ScreenWidth);
    Printf(pri, "\tscreenHeight:\t\t\t%d\n",   m_ScreenHeight);
    Printf(pri, "\tnumXSeg:\t\t\t%d\n",        m_NumXSeg);
    Printf(pri, "\tnumTSeg:\t\t\t%d\n",        m_NumTSeg);
    Printf(pri, "\ttorusTubeDia:\t\t\t%.1f\n", m_TorusTubeDia);
    Printf(pri, "\ttorusHoleDia:\t\t\t%.1f\n", m_TorusHoleDia);
    Printf(pri, "\tenableLwll:\t\t\t%s\n",     ftStrings[m_EnableLwll]);
    Printf(pri, "\tblendMode:\t\t\t%d\n",      m_BlendMode);
    Printf(pri, "\tshaderHashMode:\t\t\t%d\n", m_ShaderHashMode);
    Printf(pri, "\tdepthTest:\t\t\t%s\n",      ftStrings[m_DepthTest]);
    Printf(pri, "\tuseMM:\t\t\t\t%s\n",        ftStrings[m_UseMM]);
    Printf(pri, "\tPixelErrorTol:\t\t\t%d\n",  m_PixelErrorTol);
    Printf(pri, "\tDrawPct:\t\t%.1f\n",        m_DrawPct);
    Printf(pri, "\tPulseMode:\t\t\t%d\n",      m_PulseMode);
    Printf(pri, "\tTileMask:\t\t\t0x%08x\n",   m_TileMask);
    Printf(pri, "\tNumActiveGPCs:\t\t\t%d\n",  m_NumActiveGPCs);
}

RC GLCombustTest::Setup()
{
    RC rc;
    CHECK_RC(GpuMemTest::Setup());
    CHECK_RC(m_mglTestContext.Setup());

    m_pGSurfs = new GLGpuGoldenSurfaces(GetBoundGpuDevice());

    const UINT32 w = GetTestConfiguration()->SurfaceWidth();
    const UINT32 h = GetTestConfiguration()->SurfaceHeight();

    m_pGSurfs->DescribeSurfaces(
            w,
            h,
            mglTexFmt2Color(m_mglTestContext.ColorSurfaceFormat(0)),
            ColorUtils::ZDepthToFormat(GetTestConfiguration()->ZDepth()),
            m_mglTestContext.GetPrimaryDisplay());

    GetGoldelwalues()->SetSurfaces(m_pGSurfs);
    GetGoldelwalues()->SetYIlwerted(true);
    m_Timers = new mglTimerQuery[2];

    CHECK_RC(SetupGL());
    Printf(Tee::PriNormal, "GLCsettings:{%5d %5d %5d %1d %5d %1d %5d %1d %2d %2d %4d %4d %2d %2d %2.2f %2.2f %1d %1d %1d %1d %1d %1d}\n"
           ,m_NumVertices
           ,m_NumIndices
           ,m_PixelErrorTol
           ,m_FreqSweep
           ,m_LoopMs
           ,m_KeepRunning
           ,m_DrawRepeats
           ,m_LwllToggle
           ,m_ExtraInstr
           ,m_ExtraTex
           ,m_ScreenWidth
           ,m_ScreenHeight
           ,m_NumXSeg
           ,m_NumTSeg
           ,m_TorusTubeDia
           ,m_TorusHoleDia
           ,m_EnableLwll
           ,m_BlendMode
           ,m_ShaderHashMode
           ,m_DepthTest
           ,m_UseMM
           ,m_SimpleVS
        );

    if (m_SkipDrawChecks)
    {
        Printf(Tee::PriWarn, "%s: SkipDrawChecks is active!\n", GetName().c_str());
    }

    return mglGetRC();
}

RC GLCombustTest::SetupFramebuffers()
{
    RC rc;

    //DRAW FB Config
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_DRAW]);
    for (int i = 0; i<GetMrtCountForFsMode(); i++)
    {
        glFramebufferTexture2DEXT(
            GL_FRAMEBUFFER_EXT,
            GL_COLOR_ATTACHMENT0_EXT+i,
            GL_TEXTURE_2D,
            m_TexIds[TEX_DRAW_COLOR0+i],
            0);
    }

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
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_COMP]);
    glFramebufferTexture2DEXT(
        GL_FRAMEBUFFER_EXT,
        GL_COLOR_ATTACHMENT0_EXT,
        GL_TEXTURE_2D,
        m_TexIds[TEX_COMP_COLOR],
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

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    CHECK_RC(mglCheckFbo());
    return OK;
}

void GLCombustTest::SetupScissor(FLOAT64 drawPct)
{
    // Choose new scissor boundaries for drawPct as many pixels
    // as the full screen holds (min .5% pixels, max full screen).

    const GLint fullW = m_WindowRect[2];
    const GLint fullH = m_WindowRect[3];
    const GLint fullPixels = fullW * fullH;

    if (fullPixels == 0)
        return;

    // Colwert DrawPct to a draw ratio, limit to safe range for log().
    const double dr = max(0.005, min(1.0, drawPct / 100.0));

    // Callwlate new height to retain original aspect ratio and yield the
    // right approximate target pixel count.
    const GLint newH = static_cast<GLint>
        (exp(log((double)fullH) + log(dr)/2.0) + 0.5);

    // Find the integer width that gets us closest to the target pixel count.
    const GLint tgtPixels  = static_cast<GLint>(fullPixels * dr + 0.5);
    const GLint newW = (tgtPixels + newH/2) / newH;

    GLint newRect[4];
    newRect[0] = m_WindowRect[0] + (fullW - newW) / 2;
    newRect[1] = m_WindowRect[1] + (fullH - newH) / 2;
    newRect[2] = newW;
    newRect[3] = newH;

    if (memcmp(newRect, m_ScissorRect, sizeof(m_ScissorRect)))
    {
        Printf(Tee::PriDebug, "New Scissor = %dx%d\n", newW, newH);
        memcpy(m_ScissorRect, newRect, sizeof(m_ScissorRect));

        if (drawPct < 100.0)
        {
            glScissor(m_ScissorRect[0], m_ScissorRect[1],
                      m_ScissorRect[2], m_ScissorRect[3]);
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }
}

void GLCombustTest::UpdateTileMask()
{
    if (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE)
        return;

    UINT32 tileMask = 0;
    float enabledGpcs[8*sizeof(tileMask)] = { };
    while (static_cast<UINT32>(Utility::CountBits(tileMask)) < m_NumActiveGPCs)
    {
        const UINT32 gpcIdx = GetFpContext()->Rand.GetRandom(0, m_HighestGPCIdx);
        tileMask |= 1 << gpcIdx;
        enabledGpcs[gpcIdx] = 1.0f;
    }

    Printf(Tee::PriDebug, "GPC shuffle tileMask = 0x%08x\n", tileMask);
    m_GPCShuffleStats.AddPoint(tileMask);

    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_FRAG_ENABLEDGPCS]);
    glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, sizeof(enabledGpcs),
        enabledGpcs, GL_STATIC_DRAW);
    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);
}

RC GLCombustTest::SetupPrograms()
{
    RC rc;
#define PROGRAM_SETUP(A, B, C)                                                    \
{                                                                                 \
    string prog_##A = B;                                                          \
    CHECK_RC(mglLoadProgram(GL_##C##_PROGRAM_LW, m_ProgIds[PROG_##A],             \
                            static_cast<GLsizei>(prog_##A.size()),                \
                            reinterpret_cast<const GLubyte *>(prog_##A.c_str())));\
}
    PROGRAM_SETUP(FRAG_MAIN,            GetFragmentProgram(),         FRAGMENT);

    PROGRAM_SETUP(VERT_MAIN,            GetVertexProgram(),         VERTEX);
    PROGRAM_SETUP(VERT_MAIN_SIMPLE,     GetVertexProgramSimple(),   VERTEX);

    PROGRAM_SETUP(FRAG_COPY,            GetFragmentProgramCopy(),   FRAGMENT);
    PROGRAM_SETUP(FRAG_COPY_16_SMID_RG, GetFragmentProgramCopyRG(), FRAGMENT);
    PROGRAM_SETUP(FRAG_COPY_16_SMID_BA, GetFragmentProgramCopyBA(), FRAGMENT);
    PROGRAM_SETUP(FRAG_COPY_32_SMID_R,  GetFragmentProgramCopyR(),  FRAGMENT);
    PROGRAM_SETUP(FRAG_COPY_32_SMID_G,  GetFragmentProgramCopyG(),  FRAGMENT);
    PROGRAM_SETUP(FRAG_COPY_32_SMID_B,  GetFragmentProgramCopyB(),  FRAGMENT);
    PROGRAM_SETUP(FRAG_COPY_32_SMID_A,  GetFragmentProgramCopyA(),  FRAGMENT);

#undef PROGRAM_SETUP
    return mglGetRC();
}

RC GLCombustTest::SetupConstants()
{
    RC rc;

    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_FRAG_MAIN]);
    glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 512*sizeof(float), 0, GL_STATIC_DRAW);
    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);

    CHECK_RC(mglGetRC());

    if (m_PulseMode != PULSE_DISABLED)
    {
        CHECK_RC(SetupPulseModeConstants());
    }

    return rc;
}

RC GLCombustTest::SetupPulseModeConstants()
{
    RC rc;

    if ( (m_PulseMode != PULSE_TILE_MASK_USER) &&
         (m_PulseMode != PULSE_TILE_MASK_GPC) &&
         (m_PulseMode != PULSE_TILE_MASK_GPC_SHUFFLE) )
    {
        return rc;
    }

    GpuSubdevice *subdev = GetBoundGpuSubdevice();
    LW2080_CTRL_GR_GET_GLOBAL_SM_ORDER_PARAMS smOrder = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(subdev,
        LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER,
        &smOrder, sizeof(smOrder)));

    float smIdMap[LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER_MAX_SM_COUNT] = { };
    m_HighestGPCIdx = 0;
    for (UINT32 idx = 0; idx < smOrder.numSm; idx++)
    {
        if (smOrder.globalSmId[idx].gpcId > m_HighestGPCIdx)
        {
            m_HighestGPCIdx = smOrder.globalSmId[idx].gpcId;
        }
        smIdMap[idx] = 1.0f * smOrder.globalSmId[idx].gpcId;
    }

    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_FRAG_SMIDMAP]);
    glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, sizeof(smIdMap),
        smIdMap, GL_STATIC_DRAW);
    glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        if (m_NumActiveGPCs > (m_HighestGPCIdx + 1))
        {
            m_NumActiveGPCs = m_HighestGPCIdx + 1;
        }
        UpdateTileMask();
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
        float enabledGpcs[8*sizeof(tileMask)] = { };
        for (INT32 bitIdx = Utility::BitScanForward(tileMask);
             bitIdx != -1;
             bitIdx = Utility::BitScanForward(tileMask, bitIdx + 1))
        {
            enabledGpcs[bitIdx] = 1.0f;
        }

        glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, m_ConstIds[CONST_FRAG_ENABLEDGPCS]);
        glBufferData(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, sizeof(enabledGpcs),
            enabledGpcs, GL_STATIC_DRAW);
        glBindBuffer(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0);
    }

    return mglGetRC();
}

RC GLCombustTest::SetupGL()
{
    static const GLfloat    N_RANGE = 4.0F;
    static const GLfloat    D_RANGE = 4.0F;
    RC rc;
    //TEX
    m_TexIds.resize(TEX_NUM+m_ExtraTex+1); //due to variable # of textures
    glGenTextures(TEX_NUM+m_ExtraTex+1, (GLuint*)&m_TexIds[0]);
    CHECK_RC(SetupTextures());

    //FBO
    glGenFramebuffersEXT(FBO_NUM, (GLuint*)m_FboIds);
    CHECK_RC(SetupFramebuffers());

    //VTX
    glGenBuffers(VBO_NUM, (GLuint*)m_VboIds);
    CHECK_RC(SetupVertices());

    //PRG
    glGenProgramsLW(PROG_NUM, (GLuint*)m_ProgIds);
    CHECK_RC(SetupPrograms());

    //OCCd
    glGenOcclusionQueriesLW(OCC_NUM, (GLuint*)m_OccIds);

    //Const
    glGenBuffers(CONST_NUM, (GLuint*)m_ConstIds);
    CHECK_RC(SetupConstants());

    //viewport calcs; no state set here
    if (m_ScreenWidth <= m_ScreenHeight)
    {
        m_WorldX = N_RANGE;
        m_WorldY = N_RANGE*m_ScreenHeight/m_ScreenWidth;
    }
    else
    {
        m_WorldX = N_RANGE*m_ScreenWidth/m_ScreenHeight;
        m_WorldY = N_RANGE;
    }

    //DEPTH METHODS -- no enables here
    glDepthFunc(GL_LEQUAL);        // The Type Of Depth Testing To DC
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(D_RANGE);         // Depth Buffer Setup
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    //BLEND MODE -- no enables
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //LOGIC_OP FUNC -- no enables
    glLogicOp(GL_XOR);

    //LWLL Setting -- no enables
    glLwllFace(m_LwllToggle);

    //don't show junk
    glClear(GL_COLOR_BUFFER_BIT);

    glGetIntegerv(GL_SCISSOR_BOX, m_WindowRect);
    memcpy(m_ScissorRect, m_WindowRect, sizeof(m_ScissorRect));

    return mglGetRC();
}

RC GLCombustTest::ReconfigureGPU(gpu_config_e mode)
{
    static const GLfloat    D_RANGE = 4.0F;
    static GLenum buffer_mrt[] =
    {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
        GL_COLOR_ATTACHMENT4,
        GL_COLOR_ATTACHMENT5,
        GL_COLOR_ATTACHMENT6,
        GL_COLOR_ATTACHMENT7
    };

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,  0 );
    glDrawBuffer(GL_FRONT);
    glDisable(GL_DEPTH_TEST);       // Enables Depth Testing
    glDisable(GL_BLEND);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_LWLL_FACE);
    glDisable(GL_VERTEX_PROGRAM_LW);
    glDisable(GL_FRAGMENT_PROGRAM_LW);

    for (int i = 0; i < 8; i++)
    {
        glActiveTexture(GL_TEXTURE0_ARB+i);
        glDisable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    //VBOs (nothing to be done -- there is no toggling of this state)

    //viewport
    glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);
    glMatrixMode(GL_PROJECTION);    // add perspective to scene
    glLoadIdentity();
    glOrtho (-m_WorldX, m_WorldX, -m_WorldY, m_WorldY, -D_RANGE, D_RANGE);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    SetupScissor(100.0);
    int offset =0, div=0, mod=0;
    switch (mode)
    {
    default:
    case CONFIG_DRAW:
        //FBO
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,  m_FboIds[FBO_DRAW] );
        glDrawBuffersARB(GetMrtCountForFsMode(), buffer_mrt);

        //PARAMS
        if (m_DepthTest)             glEnable(GL_DEPTH_TEST);
        if (m_BlendMode == CM_BLEND) glEnable(GL_BLEND);
        if (m_BlendMode == CM_XOR)   glEnable(GL_COLOR_LOGIC_OP);
        if (m_EnableLwll)            glEnable(GL_LWLL_FACE);

        //VERT
        glEnable       (GL_VERTEX_PROGRAM_LW);
        glBindProgramLW(GL_VERTEX_PROGRAM_LW,
                        m_SimpleVS ?
                        m_ProgIds[PROG_VERT_MAIN_SIMPLE] : m_ProgIds[PROG_VERT_MAIN]);

        //FRAG
        glEnable       (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_MAIN]);

        //CONST
        glBindBufferBaseLW(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 0, m_ConstIds[CONST_FRAG_MAIN]);
        if ( (m_PulseMode == PULSE_TILE_MASK_USER) ||
             (m_PulseMode == PULSE_TILE_MASK_GPC) ||
             (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE) )
        {
            glBindBufferBaseLW(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 1, m_ConstIds[CONST_FRAG_SMIDMAP]);
            glBindBufferBaseLW(GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_LW, 2, m_ConstIds[CONST_FRAG_ENABLEDGPCS]);
        }

        //TEX
        for (UINT32 i = 0; i < m_ExtraTex+1; i++)
        {
            glActiveTexture(GL_TEXTURE0_ARB+i);
            glEnable       (GL_TEXTURE_2D);
            glBindTexture  (GL_TEXTURE_2D, m_TexIds[TEX_NUM+i]);
        }

        break;

    case CONFIG_COPY:
        //FBO
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_COMP]);

        //FRAG PRG
        glEnable       (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_COPY]);

        //TEX
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable       (GL_TEXTURE_2D);
        glBindTexture  (GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR0] );

        glClear(GL_COLOR_BUFFER_BIT);
        break;

    case CONFIG_COPY_MODE_8_SMID_0:
    case CONFIG_COPY_MODE_8_SMID_1:
    case CONFIG_COPY_MODE_8_SMID_2:
    case CONFIG_COPY_MODE_8_SMID_3:
    case CONFIG_COPY_MODE_8_SMID_4:
    case CONFIG_COPY_MODE_8_SMID_5:
    case CONFIG_COPY_MODE_8_SMID_6:
    case CONFIG_COPY_MODE_8_SMID_7:
        offset = mode - CONFIG_COPY_MODE_8_SMID_0;

        //FBO
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_COMP]);

        //FRAG PRG
        glEnable       (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_COPY]);

        //TEX
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable       (GL_TEXTURE_2D);
        glBindTexture  (GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR0+offset]);

        glClear(GL_COLOR_BUFFER_BIT);
        break;

    case CONFIG_COPY_MODE_16_SMID_0:
    case CONFIG_COPY_MODE_16_SMID_1:
    case CONFIG_COPY_MODE_16_SMID_2:
    case CONFIG_COPY_MODE_16_SMID_3:
    case CONFIG_COPY_MODE_16_SMID_4:
    case CONFIG_COPY_MODE_16_SMID_5:
    case CONFIG_COPY_MODE_16_SMID_6:
    case CONFIG_COPY_MODE_16_SMID_7:
    case CONFIG_COPY_MODE_16_SMID_8:
    case CONFIG_COPY_MODE_16_SMID_9:
    case CONFIG_COPY_MODE_16_SMID_10:
    case CONFIG_COPY_MODE_16_SMID_11:
    case CONFIG_COPY_MODE_16_SMID_12:
    case CONFIG_COPY_MODE_16_SMID_13:
    case CONFIG_COPY_MODE_16_SMID_14:
    case CONFIG_COPY_MODE_16_SMID_15:

        offset = mode - CONFIG_COPY_MODE_16_SMID_0;
        //MRT0 contains SMID0(RG) and SMID1(BA)
        //MRT1 contains SMID2(RG) and SMID3(BA) etc...
        div = offset / 2;
        mod = offset % 2;
        //FBO
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_COMP]);

        //FRAG PRG
        glEnable       (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_COPY_16_SMID_RG+mod]);

        //TEX
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable       (GL_TEXTURE_2D);
        glBindTexture  (GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR0+div]);

        glClear(GL_COLOR_BUFFER_BIT);
        break;

    case CONFIG_COPY_MODE_32_SMID_0:
    case CONFIG_COPY_MODE_32_SMID_1:
    case CONFIG_COPY_MODE_32_SMID_2:
    case CONFIG_COPY_MODE_32_SMID_3:
    case CONFIG_COPY_MODE_32_SMID_4:
    case CONFIG_COPY_MODE_32_SMID_5:
    case CONFIG_COPY_MODE_32_SMID_6:
    case CONFIG_COPY_MODE_32_SMID_7:
    case CONFIG_COPY_MODE_32_SMID_8:
    case CONFIG_COPY_MODE_32_SMID_9:
    case CONFIG_COPY_MODE_32_SMID_10:
    case CONFIG_COPY_MODE_32_SMID_11:
    case CONFIG_COPY_MODE_32_SMID_12:
    case CONFIG_COPY_MODE_32_SMID_13:
    case CONFIG_COPY_MODE_32_SMID_14:
    case CONFIG_COPY_MODE_32_SMID_15:
    case CONFIG_COPY_MODE_32_SMID_16:
    case CONFIG_COPY_MODE_32_SMID_17:
    case CONFIG_COPY_MODE_32_SMID_18:
    case CONFIG_COPY_MODE_32_SMID_19:
    case CONFIG_COPY_MODE_32_SMID_20:
    case CONFIG_COPY_MODE_32_SMID_21:
    case CONFIG_COPY_MODE_32_SMID_22:
    case CONFIG_COPY_MODE_32_SMID_23:
    case CONFIG_COPY_MODE_32_SMID_24:
    case CONFIG_COPY_MODE_32_SMID_25:
    case CONFIG_COPY_MODE_32_SMID_26:
    case CONFIG_COPY_MODE_32_SMID_27:
    case CONFIG_COPY_MODE_32_SMID_28:
    case CONFIG_COPY_MODE_32_SMID_29:
    case CONFIG_COPY_MODE_32_SMID_30:
    case CONFIG_COPY_MODE_32_SMID_31:

        offset = mode - CONFIG_COPY_MODE_32_SMID_0;
        //MRT0 contains SMID0(R) and SMID1(G) and SMID2(B) and SMID3(A)
        //MRT1 contains SMID4(R) and SMID5(G) and SMID6(B) and SMID7(A) etc...
        div = offset / 4;
        mod = offset % 4;
        //FBO
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboIds[FBO_COMP]);

        //FRAG PRG
        glEnable       (GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_ProgIds[PROG_FRAG_COPY_32_SMID_R+mod]);

        //TEX
        glActiveTexture(GL_TEXTURE0_ARB);
        glEnable       (GL_TEXTURE_2D);
        glBindTexture  (GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR0+div]);

        glClear(GL_COLOR_BUFFER_BIT);
        break;
    case CONFIG_RESET:
        break;
    }

    return mglGetRC();
}

RC GLCombustTest::Run()
{
    RC rc;

    glFinish();
    CallbackArguments args;
    CHECK_RC(FireCallbacks(ModsTest::MISC_A, Callbacks::STOP_ON_ERROR,
                           args, "Pre-Fill"));

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.Reset();
    }
    double l_Elapsed  = 0;
    UINT32 l_TimerIdx = 0;
    UINT32 l_Loops    = 0;
    m_FailingPixels = 0;
    m_FramesDrawn   = 0;
    ReconfigureGPU( CONFIG_DRAW  );
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glFinish();
    rc = CheckBufferGPU();
    ReconfigureGPU( CONFIG_DRAW  );
    {
        Utility::CleanupOnReturn<GpuTest, RC>
            stopPT(this, &GpuTest::StopPowerThread);
        const bool runningPowerThread = (StartPowerThread() == RC::OK);
        if (!runningPowerThread)
        {
            stopPT.Cancel();
        }

        UINT32 nextEndLoopMs = m_EndLoopPeriodMs;
        PrintProgressInit(m_LoopMs, "Test Configuration LoopMs");
        while (m_KeepRunning ||
               (l_Elapsed * 1000.0 < m_LoopMs) ||
               (((l_Loops+1) % 4) != 3) ) //2 loops for xor*2 loops for lwll/depth
        {

            //get time from two loops ago
            l_TimerIdx = l_TimerIdx ^ 1;
            l_Elapsed += m_Timers[l_TimerIdx].GetSeconds();

            //start fresh loop on this timer
            m_Timers[l_TimerIdx].Begin();

            // Adjust scissor with latest JS property.
            SetupScissor(m_DrawPct);

            if (((l_Loops+1) % 4) == 3)
            {
                UpdateTileMask();
            }

            CHECK_RC(DrawFunction());

            //rc = CheckBufferGPU();
            //ReconfigureGPU( CONFIG_DRAW  );

            if (l_Loops < m_DumpFrames)
            {
                char filename[64];
                snprintf(filename, sizeof(filename), "glc%04d_c.png", l_Loops);
                DumpImage(filename);
            }

            rc = DoEndLoop(&nextEndLoopMs, l_Elapsed);
            PrintProgressUpdate(min(static_cast<UINT64>(l_Elapsed * 1000), static_cast<UINT64>(m_LoopMs)));
            if (OK != rc)
                break;

            m_Timers[l_TimerIdx].End();
            l_Loops++;
        }

        m_FramesDrawn = l_Loops*m_DrawRepeats;
        glFinish();

        // Run this explicitly in the passing case to be able to check RC.
        if (runningPowerThread)
        {
            stopPT.Cancel();
            CHECK_RC(StopPowerThread());
        }
    }

    CHECK_RC(FireCallbacks(ModsTest::MISC_B, Callbacks::STOP_ON_ERROR,
                           args, "Pre-CheckBuffer"));

    // Run golden one time, to check last image vs. stored checksums.
    // This also allows us to dump .png files if requested.
    //rc = GetGoldelwalues()->Run();
    //Switching over to glstress::CheckBuffer style checking.
    rc = CheckBufferGPU();
    if (m_FreqSweep)
    {
        // Tell GpuMemTest not to run ResolveMemErrorResult
        SetDidResolveMemErrorResult(true);
    }

    if (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE)
    {
        m_GPCShuffleStats.PrintReport(GetVerbosePrintPri(), "GPC shuffle");
    }

    return rc;
}

RC GLCombustTest::DoEndLoop(UINT32 * pNextEndLoopMs, double l_Elapsed)
{
    RC rc;
    if ((l_Elapsed  * 1000.0) >= *pNextEndLoopMs)
    {
        rc = EndLoop(*pNextEndLoopMs);
        //if (rc == OK)
        //   rc = CheckBufferGPU();
        *pNextEndLoopMs += m_EndLoopPeriodMs;
    }
    return rc;
}

RC GLCombustTest::CheckBufferGPU()
{
    if (m_SkipDrawChecks)
    {
        return RC::OK;
    }

    m_FailingPixels=0;
    vector<GLuint> l_SmidFailingPixels(GetSmidCountForFsMode(), 0);
    for (int i = 0; i < GetSmidCountForFsMode(); i++)
    {

        ReconfigureGPU((gpu_config_e)( GetBaseConfigCopyForFsMode()+i ));
        FLOAT32  x = m_WorldX;
        FLOAT32  y = m_WorldY;

        glBeginOcclusionQueryLW(m_OccIds[OCC_QUERY0]);
        glBegin(GL_TRIANGLES);

        glTexCoord2f(0.0, 0.0);
        glVertex2f  ( -x,  -y);

        glTexCoord2f(2.0, 0.0);
        glVertex2f  (3*x,  -y);

        glTexCoord2f(0.0, 2.0);
        glVertex2f  ( -x, 3*y);

        glEnd();
        glEndOcclusionQueryLW();

        glGetOcclusionQueryuivLW(m_OccIds[OCC_QUERY0], GL_PIXEL_COUNT_LW,
                                 &l_SmidFailingPixels[i]);
        m_FailingPixels += l_SmidFailingPixels[i];
    }
    UINT32 bad   = 0x0;
    UINT32 worst = 0x0;
    UINT32 max_cnt = 0x0, max_idx=0x0;
    for (int i = 0; i < GetSmidCountForFsMode(); i++)
    {
        if (l_SmidFailingPixels[i] )
        {
            bad |= 1 << i;
            if (l_SmidFailingPixels[i] > max_cnt)
            {
                max_cnt = l_SmidFailingPixels[i];
                max_idx = i;
            }
        }
    }
    if (bad)
        worst = (1 << max_idx);
    if (GetSmidCountForFsMode() == 32)
    {
        Printf(Tee::PriNormal, "BadCBufMask = 0x%08x\n", bad);
        Printf(Tee::PriNormal, "WorstCBufMask = 0x%08x\n", worst);
    }
    else if (GetSmidCountForFsMode() == 16)
    {
        Printf(Tee::PriNormal, "BadCBufMask = 0x%04x\n", bad);
        Printf(Tee::PriNormal, "WorstCBufMask = 0x%04x\n", worst);
    }
    else if (GetSmidCountForFsMode() == 8)
    {
        Printf(Tee::PriNormal, "BadCBufMask = 0x%02x\n", bad);
        Printf(Tee::PriNormal, "WorstCBufMask = 0x%02x\n", worst);
    }
    else
    {
        // just let the return code handle it
    }
    if (m_FailingPixels > m_PixelErrorTol ) return RC::GPU_STRESS_TEST_FAILED;
    return OK;
}

void GLCombustTest::ReportErrors(UINT32 errs)
{
    Printf(Tee::PriNormal,
           "GLCombust Found %d pixel errors (threshold %d)\n",
           errs, m_PixelErrorTol );
}

RC GLCombustTest::DrawFunction ()
{
    for (UINT32 d = 0; d < m_DrawRepeats; d++)
    {
        glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_SHORT, 0);
    }
    if (m_DepthTest)
        glClear(GL_DEPTH_BUFFER_BIT);
    glLwllFace(m_LwllToggle);
    m_LwllToggle = (m_LwllToggle == GL_FRONT) ? GL_BACK : GL_FRONT;
    return mglGetRC();
}

void GLCombustTest::MatrixMultiply( float* src, float* dst, float* mvp )
{
    dst[0] = mvp[0] *src[0]+mvp[1] *src[1]+mvp[2] *src[2]+mvp[3] *src[3];
    dst[1] = mvp[4] *src[0]+mvp[5] *src[1]+mvp[6] *src[2]+mvp[7] *src[3];
    dst[2] = mvp[8] *src[0]+mvp[9] *src[1]+mvp[10]*src[2]+mvp[11]*src[3];
    dst[3] = mvp[12]*src[0]+mvp[13]*src[1]+mvp[14]*src[2]+mvp[15]*src[3];
}

//I want to modify vbuff in place, therefore no const qualifier.
void GLCombustTest::DoMVP(vector<char> *vbuff)
{
    static float mvp[16]  = { 1.7015725f,      3.0f, 0.15720493f,  -1.2068434f,
                              -2.5775481e-7f, -2.0f, 2.3350549f,    1.2245929f,
                              0.34202632f,    -5.5f, -0.23879051f,  11.822088f,
                              0.34185532f,    -0.5f, -0.23867112f,  13.816177f };
    char* ptr = &(vbuff->operator[](0));
    float src[4] = {0.0, 0.0, 0.0, 1.0};
    float dst[4] = {0.0, 0.0, 0.0, 1.0};
    /*3*4 pos; 3*4 norm; 2*4tex == increment by 32*/

    for (unsigned int i = 0; i < vbuff->size(); i+=32 )
    {
        float* pos  = reinterpret_cast<float*>(&(ptr[i])    );
        float* norm = reinterpret_cast<float*>(&(ptr[i+12]) );
        src[0] = 1.12f * norm[0] + pos[0];
        src[1] = 1.12f * norm[1] + pos[1];
        src[2] = 1.12f * norm[2] + pos[2];
        src[3] = 1.00;
        MatrixMultiply(src, dst, mvp);

        pos[0] = dst[0]/dst[3];
        pos[1] = dst[1]/dst[3];
        pos[2] = dst[2]/dst[3];
    }
}

RC GLCombustTest::SetupVertices ()
{
    //program generated geometry

    vector<char> fbuff1;
    vector<char> fbuff2;
    static const int FLOATS_PER_VERTEX = 8;

    Torus (fbuff1, fbuff2, &m_NumVertices, &m_NumIndices);

    if (m_SimpleVS)
        DoMVP(&fbuff1);

    glBindBuffer(GL_ARRAY_BUFFER_ARB, m_VboIds[VBO_VTX]);
    glBufferData(GL_ARRAY_BUFFER_ARB, m_NumVertices*FLOATS_PER_VERTEX*sizeof(GLfloat), (void*)&fbuff1[0], GL_STATIC_DRAW_ARB);
    glVertexPointer  ( 3, GL_FLOAT, (sizeof(GLfloat)*FLOATS_PER_VERTEX), ((GLvoid*)(0)) );
    glNormalPointer  (    GL_FLOAT, (sizeof(GLfloat)*FLOATS_PER_VERTEX), ((GLvoid*)(0+12)) );
    glTexCoordPointer( 2, GL_FLOAT, (sizeof(GLfloat)*FLOATS_PER_VERTEX), ((GLvoid*)(0+24)) );

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    //Setup index element state
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_VboIds[VBO_IDX]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_NumIndices*sizeof(GLushort), (void*)&fbuff2[0], GL_STATIC_DRAW_ARB);

    return mglGetRC();
}

RC GLCombustTest::SetupTextures ()
{
    static const int        TEX_SIZE = 32;
    glActiveTexture(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);
    // TEX_DRAW_COLOR - this texture contains the draw surface for the main test
    for (int i = 0; i < GetMrtCountForFsMode(); i++)
    {
        glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_DRAW_COLOR0+i]);
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
    }
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

    // TEX_COMP_COLOR - this texture contains pixels for oclwlsion query
    glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_COMP_COLOR]);
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

    //normal textures
    for (LwU32 i = 0; i < m_ExtraTex+1; i++)
    {
        vector<char> texbuffer;
        texbuffer.resize(TEX_SIZE*TEX_SIZE*4, 0);
        Memory::FillRandom(&texbuffer[0], 37+i, TEX_SIZE*TEX_SIZE*4);
        glBindTexture(GL_TEXTURE_2D, m_TexIds[TEX_NUM+i]);
        if (m_UseMM)
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
        else
            glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT );

        for (GLuint level = 0, sz = TEX_SIZE; sz; level++, sz>>=1)
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, sz, sz, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)&texbuffer[0]);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    return mglGetRC();
}

void GLCombustTest::DumpImage(const char *filename)
{
    const mglTexFmtInfo *pFmtInfo = mglGetTexFmtInfo(mglTexFmt2Color(GL_RGBA8));
    const UINT32 bpp = ColorUtils::PixelBytes(pFmtInfo->ExtColorFormat);
    const UINT32 size = m_ScreenWidth * m_ScreenHeight * bpp;

    if (size > m_ScreenBuf.size())
    {
        m_ScreenBuf.resize(size);
    }

    glReadPixels(
        0,
        0,
        m_ScreenWidth,
        m_ScreenHeight,
        GL_BGRA,
        GL_UNSIGNED_BYTE,
        &m_ScreenBuf[0]);

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
}

void GLCombustTest::Torus
(
    vector<char> &vbuff,
    vector<char> &ibuff,
    int* lW,
    int* nI
)
{
    int numXSeg = m_NumXSeg; //cross section segments
    int numTSeg = m_NumTSeg; //torus tubular segments
    double torusTubeDia = m_TorusTubeDia;
    double torusHoleDia = m_TorusHoleDia;

    int i, j, k;
    int vidx = 0;
    int iidx = 0;

    double s, t, x, y, z, nx, ny, nz, tcx, tcy, twopi;

    bool firstTri = true; //to indicate if this is the 1st tri in a strip
    int  stripVtxCount = 0;

    twopi = 2 * 3.14159265358979323846;

    constexpr bool toggle = true;

    int outerLoop;
    int innerLoop;

    //Control how strip are created, along cross section or lateral section
    if (toggle)
    {
        outerLoop = numXSeg;
        innerLoop = numTSeg;
    }
    else
    {
        outerLoop = numTSeg;
        innerLoop = numXSeg;
    }

    vattr_t* vattrbuff = new vattr_t[outerLoop*(innerLoop+1)*2];
    // insert 1 index per vertex on first 3 vertex, 3 index per vertex afterwards.
    GLushort* idxbuff  = new GLushort[outerLoop*(((innerLoop+1)*2)*3-6)];

    *lW = outerLoop*(innerLoop+1)*2;
    *nI = outerLoop*(((innerLoop+1)*2)*3-6);

    for (i = 0; i < outerLoop; i++)
    {
        //Triangle strip loops created in here, 1 for each iteration
        //These strips are colwerted into an index list of plain triangles
        firstTri = true;
        stripVtxCount = 0;
        for (j = 0; j <= innerLoop; j++)
        {
            for (k = 1; k >= 0; k--)
            {

                if (toggle)
                {
                    s = (i + k) % outerLoop;
                    t = j % innerLoop;
                }
                else
                {
                    t = (i + k) % innerLoop;
                    s = j % outerLoop;
                }

                x = ((torusHoleDia+torusTubeDia/2)+torusTubeDia/2*cos(s*twopi/numXSeg))
                    *cos(t*twopi/numTSeg);
                y = torusTubeDia/2 * sin(s * twopi / numXSeg);
                z = ((torusHoleDia+torusTubeDia/2)+torusTubeDia/2*cos(s*twopi/numXSeg))
                    *sin(t*twopi/numTSeg);

                nx = cos(s*twopi/numXSeg)*cos(t*twopi/numTSeg);
                ny = sin(s * twopi / numXSeg);
                nz = cos(s*twopi/numXSeg)*sin(t*twopi/numTSeg);

                tcx = x;
                tcy = z;

                if (j%2==1)
                {
                    //add grooves to torus
                    vattrbuff[vidx].x = float(x+nx*0.1);
                    vattrbuff[vidx].y = float(y+ny*0.1);
                    vattrbuff[vidx].z = float(z+nz*0.1);
                }
                else
                {
                    vattrbuff[vidx].x = (float)x;
                    vattrbuff[vidx].y = (float)y;
                    vattrbuff[vidx].z = (float)z;
                }
                vattrbuff[vidx].nx = (float)nx;
                vattrbuff[vidx].ny = (float)ny;
                vattrbuff[vidx].nz = (float)nz;

                vattrbuff[vidx].tcx = (float)tcx;
                vattrbuff[vidx].tcy = (float)tcy;

                //FIXME
                // (??) possibly the above FIXME refers to the fact that the
                // below code draws half the tris clockwise, the other half CCW.
                // See GLStressTest::GenGeometryTorus for a fixed version of
                // this index-generation code.
                stripVtxCount++;
                if (firstTri)
                {
                    idxbuff[iidx] = vidx;
                    iidx++;
                    if (stripVtxCount==3)
                        firstTri = false;
                }
                else
                {
                    idxbuff[iidx] = vidx-2;
                    iidx++;
                    idxbuff[iidx] = vidx;
                    iidx++;
                    idxbuff[iidx] = vidx-1;
                    iidx++;
                }
                vidx++;
            }
        }
    }
    for (unsigned int i =0; i < vidx*sizeof(vattr_t); i++)
    {
        vbuff.push_back( ((char*)vattrbuff)[i] );
    }
    for (unsigned int i =0; i< iidx*sizeof(GLushort); i++)
    {
        ibuff.push_back( ((char*)idxbuff)[i] );
    }

    delete[] vattrbuff;
    delete[] idxbuff;
}

RC GLCombustTest::Cleanup()
{
    StickyRC firstRc;
    if (m_mglTestContext.IsGLInitialized())
    {
        if (m_ProgIds[0])
            glDeleteProgramsLW        (PROG_NUM,  (const GLuint*)m_ProgIds);
        if (m_ConstIds[0])
            glDeleteBuffers           (CONST_NUM, (const GLuint*)m_ConstIds);
        if (m_FboIds[0])
            glDeleteFramebuffersEXT   (FBO_NUM,   (const GLuint*)m_FboIds);
        if (m_TexIds.size())
            glDeleteTextures          (TEX_NUM+m_ExtraTex+1,   (const GLuint*)&m_TexIds[0]);
        if (m_OccIds[0])
            glDeleteOcclusionQueriesLW(OCC_NUM,   (const GLuint*)m_OccIds);
        if (m_VboIds[0])
            glDeleteBuffers           (VBO_NUM,   (const GLuint*)m_VboIds);
    }
    memset(m_FboIds, 0, sizeof(m_FboIds));
    m_TexIds.clear();
    memset(m_VboIds, 0, sizeof(m_VboIds));
    memset(m_ProgIds, 0, sizeof(m_ProgIds));
    memset(m_OccIds, 0, sizeof(m_OccIds));
    memset(m_ConstIds, 0, sizeof(m_ConstIds));

    if (m_mglTestContext.IsGLInitialized())
    {
        ReconfigureGPU(CONFIG_RESET);
    }

    GetGoldelwalues()->ClearSurfaces();
    delete m_pGSurfs;
    m_pGSurfs = NULL;

    if (m_Timers)
    {
        delete[] m_Timers;
        m_Timers = 0;
    }

    if (m_mglTestContext.GetDisplay() && !m_FreqSweep)
    {
        firstRc = m_mglTestContext.GetDisplay()->TurnScreenOff(false);
    }
    firstRc = m_mglTestContext.Cleanup();
    firstRc = GpuMemTest::Cleanup();
    return firstRc;
}

/*virtual*/
void GLCombustTest::PowerControlCallback(LwU32 milliWatts)
{
    if (m_Watts)
    {
        // The control constant "kp" scales "error milliwatts" to
        // "fractional jump in DrawPct", and was chosen by trial and error.
        //
        // When errMilliWatts is positive, it reduces DrawPct a bit at a time
        // until we reduce power enough to be at the target.
        const double kp = -0.00001;
        const double errMw = milliWatts - m_Watts*1000.0;
        const double newDrawPct = m_DrawPct + m_DrawPct * kp * errMw;
        m_DrawPct = max(1.0, min(100.0, newDrawPct));
    }
}

JS_CLASS_INHERIT_FULL(GLCombustTest, GpuMemTest, "GLCombust OpenGL test",
                      0);

CLASS_PROP_READWRITE(GLCombustTest, LoopMs, UINT32,
        "Minimum test duration in milliseconds.");

CLASS_PROP_READWRITE(GLCombustTest, KeepRunning, bool,
        "While this is true, Run will continue even beyond Loops.");

CLASS_PROP_READWRITE(GLCombustTest, ExtraInstr, UINT32,
        "Idempotent instruction segments to append to fragment shader.");

CLASS_PROP_READWRITE(GLCombustTest, ExtraTex, UINT32,
        "Extra Textures to use.");

CLASS_PROP_READWRITE(GLCombustTest, ScreenWidth, UINT32,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, ScreenHeight, UINT32,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, NumXSeg, UINT32,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, NumTSeg, UINT32,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, TorusTubeDia, FLOAT64,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, TorusHoleDia, FLOAT64,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, EnableLwll, bool,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, BlendMode,  UINT32,
        ".0:none 1:additive 2:xor ");

CLASS_PROP_READWRITE(GLCombustTest, ShaderHashMode, UINT32,
        ".0:no hash 1:mod 257 hash");

CLASS_PROP_READWRITE(GLCombustTest, DepthTest, bool,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, UseMM, bool,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, PixelErrorTol, UINT32,
        ".");

CLASS_PROP_READWRITE(GLCombustTest, SimpleVS, bool,
        "Premultiply MVP matrix in VBO");
CLASS_PROP_READWRITE(GLCombustTest, EndLoopPeriodMs, UINT32,
        "EndLoopPeriodMs as requested in 978316 ");

CLASS_PROP_READWRITE(GLCombustTest, DrawPct, FLOAT64,
        "Percent of screen to draw (default 100.0).");

CLASS_PROP_READWRITE(GLCombustTest, PulseMode, UINT32,
        "Pulse Mode");

CLASS_PROP_READWRITE(GLCombustTest, TileMask, UINT32,
        "User or GPC tile bitmask");

CLASS_PROP_READWRITE(GLCombustTest, NumActiveGPCs, UINT32,
        "Number of active GPCs in GPC shuffle mode");

CLASS_PROP_READWRITE(GLCombustTest, SkipDrawChecks, bool,
        "Do not report pixel errors");

CLASS_PROP_READWRITE(GLCombustTest, FreqSweep, bool,
        "Prepare pipeline for freq sweeping (draw offscreen)");

CLASS_PROP_READWRITE_FULL(GLCombustTest, DrawRepeats, UINT32,
        "For each Loop, how many draws?",
        SPROP_VALID_TEST_ARG, 500);

CLASS_PROP_READWRITE(GLCombustTest, FailingPixels, UINT32,
        "Failing pixels last draw");

CLASS_PROP_READWRITE(GLCombustTest, FramesDrawn, UINT32,
        "# frames last draw");

CLASS_PROP_READWRITE(GLCombustTest, Watts, UINT32,
        "target power, in Watts");

CLASS_PROP_READWRITE(GLCombustTest, DumpFrames, UINT32,
        "Number of frames to dump images of (default 0).");

CLASS_PROP_READWRITE(GLCombustTest, FsMode, UINT32,
        "floorsweep mode 0-1 1-8 2-16 3-32");

PROP_CONST(GLCombustTest, PRE_FILL,      ModsTest::MISC_A);
PROP_CONST(GLCombustTest, PRE_CHECK,     ModsTest::MISC_B);

string GLCombustTest::GetVertexProgramSimple ()
{
    // commented out lines are documentation of shader offload of work to CPU.
    string s =
        "!!LWvp5.0\n"
        "ATTRIB vertex_attrib[] = { vertex.attrib[0..8] };\n"
        "OUTPUT result_texcoord[] = { result.texcoord[0..4] };\n"
        //Remove MVP multiply -- VBO already processed
        "MOV.F result.position.xyz, vertex.attrib[0];\n"
        "MOV.F result.position.w, {1.0}.x;\n"
        "MOV.F result.texcoord[0], vertex.attrib[8];\n"
        "MOV.F result.texcoord[1], {0}.x;\n"
        "MOV.F result.texcoord[2].xyz, vertex.attrib[2];\n"
        "MOV.F result.texcoord[3].xyz, vertex.attrib[0];\n"
        "MOV.F result.texcoord[4].xyz, {0}.x;\n"
        "END\n"
        "# 12 instructions, 2 R-regs\n";

    return s;
}
string GLCombustTest::GetVertexProgram() {
    string s =
        "!!LWvp5.0\n"
        "ATTRIB vertex_attrib[] = { vertex.attrib[0..8] };\n"
        "OUTPUT result_texcoord[] = { result.texcoord[0..4] };\n"
        "TEMP R0, R1;\n"
        "MUL.F R0.xyz, vertex.attrib[2], {1.12}.x;\n"
        "ADD.F R0.xyz, R0, vertex.attrib[0];\n"
        "MUL.F R1, R0.y, {3, -2, -5.5, -0.5};\n"
        "MAD.F R1, R0.x, {1.7015724, -2.5775481e-007, 0.34202632, 0.34185532}, R1;\n"
        "MAD.F R0, R0.z, {0.15720493, 2.3350549, -0.23879051, -0.23867112}, R1;\n"
        "ADD.F R0, R0, {-1.2068434, 1.2245929, 11.822088, 13.816177};\n"
        "MOV.F result.position, R0;\n"
        "ADD.F result.texcoord[3].xyz, -R0, {18, 15, -20};\n"
        "MOV.F result.texcoord[0], vertex.attrib[8];\n"
        "MOV.F result.texcoord[1], {0}.x;\n"
        "MOV.F result.texcoord[2].xyz, vertex.attrib[2];\n"
        "MOV.F result.texcoord[4].xyz, {0}.x;\n"
        "END\n"
        "# 12 instructions, 2 R-regs\n";

    return s;
}

string GLCombustTest::GetFragmentProgram ()
{
    string str = "!!LWfp5.0\n";
    str += "OPTION ARB_draw_buffers;\n";
    str += "BUFFER AlwaysZero= program.buffer[0][255];\n";
    str += "ATTRIB fragment_texcoord[] = { fragment.texcoord[0..3] };\n";
    str += "TEMP R0, R1, R2, TEMP_COLOR, TEMP_PRED;\n";
    for (UINT32 i = 0; i < (UINT32)m_ExtraTex+1; i++)
    {
        str += Utility::StrPrintf(
                "TEXTURE texture%d = texture[%d];\n",
                i, i);
    }
    if ( (m_PulseMode == PULSE_TILE_MASK_USER) ||
         (m_PulseMode == PULSE_TILE_MASK_GPC) ||
         (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE) )
    {
        str += "CAL tileMasking;\n";
    }
    str += "DP3.F R0.y, fragment.texcoord[3], fragment.texcoord[3];\n";
    str += "RSQ.F R0.y, R0.y;\n";
    str += "DP3.F R0.x, fragment.texcoord[2], fragment.texcoord[2];\n";
    str += "MUL.F R1.xyz, R0.y, fragment.texcoord[3];\n";
    str += "RSQ.F R0.x, R0.x;\n";
    str += "MUL.F R0.xyz, R0.x, fragment.texcoord[2];\n";
    str += "DP3.F.SAT R1.w, R0, R1;\n";
    str += "TEX.F R0, fragment.texcoord[0], texture0, 2D;\n";
    str += "TEX.F R1.xyz, fragment.texcoord[2], texture0, 2D;\n";

    for (UINT32 i = 1; i < (UINT32)m_ExtraTex+1; i++)
    {
        str += Utility::StrPrintf(
                "TEX.F R2, fragment.texcoord[0], texture%d,2D;\n", i);
        str += "ADD.F R0, R0, R2;\n";
    }
    if ( m_ExtraTex > 0 )
    {
        str += Utility::StrPrintf(
                "DIV.F R0, R0, %d;\n", (int) m_ExtraTex+1);
    }

    str += "MUL.F R0.xyz, R0, R1;\n";
    str += "MUL.F R1.xyz, R0, R1.w;\n";
    str += "MUL.F R1.xyz, R1, {2.1600001, 1.2}.xxyw;\n";

    if (m_ExtraInstr)
    {
        str += "MOV.F R2.x, R1.x;\n";
        for (unsigned int i = 0; i < m_ExtraInstr; i++)
        {
            str+= "MAD.F R2.x, R2.x, R2.x, R2.x;\n";
        }
        str+= "MOV.CC0 R2.y, AlwaysZero.y;\n";
        str+= "MOV.F R1.x(GT0.y), R2.x;\n";
    }

    switch (m_ShaderHashMode)
    {
    default:
    case SHM_NONE :
        str += "MAD.F TEMP_COLOR.xyz, R0, {1.08, 0.60000002}.xyyw, R1;\n";
        str += "MUL.F TEMP_COLOR.w, R0, {0.37892911}.x;\n";
        break;
    case SHM_MOD_257 :
        str += "MAD.F R1.xyz, R0, {1.08, 0.60000002}.xyyw, R1;\n";
        str += "MUL.F R1.w, R0, {0.37892911}.x;\n";
        str += "MOD.U R1, R1, 257;\n";
        str += "I2F R1, R1;\n";
        str += "MUL.F TEMP_COLOR, R1, {0.00392156862745}.xxxx;\n";
        break;
    }

#define MRT_BLOCK8(MRT0, SMID0)                                   \
        "SUB.U.CC0 R0.x, fragment.smid.x, " #SMID0 ";\n"          \
        "MOV result.color[" #MRT0 "].x(EQ0.x), TEMP_COLOR.x;\n"   \
        "MOV result.color[" #MRT0 "].y(EQ0.x), TEMP_COLOR.y;\n"   \
        "MOV result.color[" #MRT0 "].z(EQ0.x), TEMP_COLOR.z;\n"   \
        "MOV result.color[" #MRT0 "].w(EQ0.x), TEMP_COLOR.w;\n"   \
        "MOV result.color[" #MRT0 "].x(NE0.x), {0,0,0,0}.x;\n"    \
        "MOV result.color[" #MRT0 "].y(NE0.x), {0,0,0,0}.y;\n"    \
        "MOV result.color[" #MRT0 "].z(NE0.x), {0,0,0,0}.z;\n"    \
        "MOV result.color[" #MRT0 "].w(NE0.x), {0,0,0,0}.w;\n"

#define MRT_BLOCK16(MRT0, SMID0, SMID1)                           \
        "SUB.U.CC0 R0.x, fragment.smid.x, " #SMID0 ";\n"          \
        "MOV result.color[" #MRT0 "].x(EQ0.x), TEMP_COLOR.x;\n"   \
        "MOV result.color[" #MRT0 "].y(EQ0.x), TEMP_COLOR.y;\n"   \
        "MOV result.color[" #MRT0 "].x(NE0.x), {0,0,0,0}.x;\n"    \
        "MOV result.color[" #MRT0 "].y(NE0.x), {0,0,0,0}.y;\n"    \
        "SUB.U.CC0 R0.x, fragment.smid.x, " #SMID1 ";\n"          \
        "MOV result.color[" #MRT0 "].z(EQ0.x), TEMP_COLOR.z;\n"   \
        "MOV result.color[" #MRT0 "].w(EQ0.x), TEMP_COLOR.w;\n"   \
        "MOV result.color[" #MRT0 "].z(NE0.x), {0,0,0,0}.z;\n"    \
        "MOV result.color[" #MRT0 "].w(NE0.x), {0,0,0,0}.w;\n"

#define MRT_BLOCK32(MRT0, SMID0, SMID1, SMID2, SMID3)             \
        "SUB.U.CC0 R0.x, fragment.smid.x, " #SMID0 ";\n"          \
        "MOV result.color[" #MRT0 "].x(EQ0.x), TEMP_COLOR.x;\n"   \
        "MOV result.color[" #MRT0 "].x(NE0.x), {0,0,0,0}.x;\n"    \
        "SUB.U.CC0 R0.x, fragment.smid.x, " #SMID1 ";\n"          \
        "MOV result.color[" #MRT0 "].y(EQ0.x), TEMP_COLOR.y;\n"   \
        "MOV result.color[" #MRT0 "].y(NE0.x), {0,0,0,0}.y;\n"    \
        "SUB.U.CC0 R0.x, fragment.smid.x, " #SMID2 ";\n"          \
        "MOV result.color[" #MRT0 "].z(EQ0.x), TEMP_COLOR.z;\n"   \
        "MOV result.color[" #MRT0 "].z(NE0.x), {0,0,0,0}.z;\n"    \
        "SUB.U.CC0 R0.x, fragment.smid.x, " #SMID3 ";\n"          \
        "MOV result.color[" #MRT0 "].w(EQ0.x), TEMP_COLOR.w;\n"   \
        "MOV result.color[" #MRT0 "].w(NE0.x), {0,0,0,0}.w;\n"

    switch (m_FsMode)
    {
    case FS_MODE_NO_FS:
        str+= "MOV result.color[0], TEMP_COLOR;\n";
        break;
    case FS_MODE_UP_TO_8_SMID:
        // FIXME: I had trouble with the SEQ/CMP version, debug later ex. below
        //        str += "SEQ.S TEMP_PRED, fragment.smid.xxxx, {0,1,2,3};\n";
        //        str += "CMP result.color[0].x, TEMP_PRED.x, TEMP_COLOR.x, {0,0,0,0}.x;\n";

        str += MRT_BLOCK8(0, 0);
        str += MRT_BLOCK8(1, 1);
        str += MRT_BLOCK8(2, 2);
        str += MRT_BLOCK8(3, 3);
        str += MRT_BLOCK8(4, 4);
        str += MRT_BLOCK8(5, 5);
        str += MRT_BLOCK8(6, 6);
        str += MRT_BLOCK8(7, 7);

        break;

    case FS_MODE_UP_TO_16_SMID:
        str += MRT_BLOCK16(0, 0, 1);
        str += MRT_BLOCK16(1, 2, 3);
        str += MRT_BLOCK16(2, 4, 5);
        str += MRT_BLOCK16(3, 6, 7);
        str += MRT_BLOCK16(4, 8, 9);
        str += MRT_BLOCK16(5, 10, 11);
        str += MRT_BLOCK16(6, 12, 13);
        str += MRT_BLOCK16(7, 14, 15);
        break;

    case FS_MODE_UP_TO_32_SMID:
        str += MRT_BLOCK32(0, 0, 1, 2, 3);
        str += MRT_BLOCK32(1, 4, 5, 6, 7);
        str += MRT_BLOCK32(2, 8, 9, 10, 11);
        str += MRT_BLOCK32(3, 12, 13, 14, 15);
        str += MRT_BLOCK32(4, 16, 17, 18, 19);
        str += MRT_BLOCK32(5, 20, 21, 22, 23);
        str += MRT_BLOCK32(6, 24, 25, 26, 27);
        str += MRT_BLOCK32(7, 28, 29, 30, 31);
        break;
    }

    if ( (m_PulseMode == PULSE_TILE_MASK_GPC) || (m_PulseMode == PULSE_TILE_MASK_GPC_SHUFFLE) )
    {
        str +=
R"lwfp(RET;
tileMasking:
 BUFFER smidmap[] = { program.buffer[1] };
 BUFFER enabledgpc[] = { program.buffer[2] };
 TEMP kiltemp;
 MOV kiltemp.x, fragment.smid.x;
 MOV kiltemp.x, smidmap[kiltemp.x];
 ROUND.U kiltemp.x, kiltemp.x;
 MOV.U.CC kiltemp.x, enabledgpc[kiltemp.x];
 KIL EQ.x;
 RET;
)lwfp";
    }
    else if (m_PulseMode == PULSE_TILE_MASK_USER)
    {
    str +=
R"lwfp(RET;
tileMasking:
 BUFFER enabledtile[] = { program.buffer[2] };
 TEMP kiltemp;
 DIV kiltemp.x, fragment.position.x, 16;
 FLR.U kiltemp.x, kiltemp.x;
 MOD.U kiltemp.x, kiltemp.x, 32;
 MOV.U.CC kiltemp.x, enabledtile[kiltemp.x];
 KIL EQ.x;
 RET;
)lwfp";
    }

    str +=
        "END\n";

    return str;
}

//Kill the pixel if it is 0 in all channels
string GLCombustTest::GetFragmentProgramCopy()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF EQ.x;\n"
        " IF EQ.y;\n"
        "  IF EQ.z;\n"
        "   IF EQ.w;\n"
        "    KIL {-1};\n"
        "   ENDIF;\n"
        "  ENDIF;\n"
        " ENDIF;\n"
        "ENDIF;\n"
        "MOV result.color, R1;\n"
        "END\n";
    return str;
}

//Kill the pixel if it is 0 in RG channels
string GLCombustTest::GetFragmentProgramCopyRG()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF EQ.x;\n"
        " IF EQ.y;\n"
        "  KIL {-1};\n"
        " ENDIF;\n"
        "ENDIF;\n"
        "MOV result.color.x, R1.x;\n"
        "MOV result.color.y, R1.y;\n"
        "END\n";
    return str;
}

//Kill the pixel if it is 0 in BA channels
string GLCombustTest::GetFragmentProgramCopyBA()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF EQ.z;\n"
        " IF EQ.w;\n"
        "  KIL {-1};\n"
        " ENDIF;\n"
        "ENDIF;\n"
        "MOV result.color.z, R1.z;\n"
        "MOV result.color.w, R1.w;\n"
        "END\n";
    return str;
}

//Kill the pixel if it is 0 in R channels
string GLCombustTest::GetFragmentProgramCopyR()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF EQ.x;\n"
        " KIL {-1};\n"
        "ENDIF;\n"
        "MOV result.color.r, R1.r;\n"
        "END\n";
    return str;
}

//Kill the pixel if it is 0 in G channels
string GLCombustTest::GetFragmentProgramCopyG()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF EQ.y;\n"
        " KIL {-1};\n"
        "ENDIF;\n"
        "MOV result.color.y, R1.y;\n"
        "END\n";
    return str;
}

//Kill the pixel if it is 0 in B channels
string GLCombustTest::GetFragmentProgramCopyB()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF EQ.z;\n"
        " KIL {-1};\n"
        "ENDIF;\n"
        "MOV result.color.z, R1.z;\n"
        "END\n";
    return str;
}

//Kill the pixel if it is 0 in A channels
string GLCombustTest::GetFragmentProgramCopyA()
{
    string str =
        "!!LWfp4.0\n"
        "main:\n"
        "TEMP R0;\n"
        "TEMP R1;\n"
        "TEX R1, fragment.texcoord[0], texture[0], 2D;\n"
        "MOV.U R0, {0}.xyzw;\n"
        "ADDC R0, R0, -R1;\n"
        "IF EQ.w;\n"
        " KIL {-1};\n"
        "ENDIF;\n"
        "MOV result.color.w, R1.w;\n"
        "END\n";
    return str;
}
