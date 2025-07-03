/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013,2015,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RndMisc         utility random stuff (test control)
//
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#include "glrandom.h"   // declaration of our namespace
#include "core/include/massert.h"
#include "core/include/utility.h"
using namespace GLRandom;

//------------------------------------------------------------------------------
RndMisc::RndMisc(GLRandomTest * pglr)
: GLRandomHelper(pglr, RND_TSTCTRL_NUM_PICKERS, "Misc")
{
    m_XfbMode = 0;
    m_XfbCapturePossible = false;
    m_XfbPlaybackPossible = false;
    m_3dPixelsSupported = true;
}

//------------------------------------------------------------------------------
RndMisc::~RndMisc()
{
}

//------------------------------------------------------------------------------
RC RndMisc::SetDefaultPicker(int picker)
{
    switch (picker)
    {
        default:
            return RC::BAD_PICKER_INDEX;

        case RND_TSTCTRL_STOP:
            // (picked before all other pickers in a loop) stop testing, return current pass/fail code
            m_Pickers[RND_TSTCTRL_STOP].ConfigConst(GL_FALSE);
            break;

        case RND_TSTCTRL_LOGMODE:
            // (picked just after STOP) self-check logging/checking of generated textures and random picks
            m_Pickers[RND_TSTCTRL_LOGMODE].ConfigConst(RND_TSTCTRL_LOGMODE_Skip);
            break;

        case RND_TSTCTRL_SUPERVERBOSE:
            // (picked after all other pickers in a loop) do/don't print picks just before sending them
            m_Pickers[RND_TSTCTRL_SUPERVERBOSE].ConfigConst(GL_FALSE);
            break;

        case RND_TSTCTRL_SKIP:
            // (picked after SUPERVERBOSE) mask of blocks that should skip sending to GL this loop
            m_Pickers[RND_TSTCTRL_SKIP].ConfigConst(0);
            break;

        case RND_TSTCTRL_RESTART_SKIP:
            m_Pickers[RND_TSTCTRL_RESTART_SKIP].ConfigConst(0);
            break;

        case RND_TSTCTRL_COUNT_PIXELS:
            // do/don't enable zpass-pixel-count mode on this draw
            m_Pickers[RND_TSTCTRL_COUNT_PIXELS].ConfigRandom();
            m_Pickers[RND_TSTCTRL_COUNT_PIXELS].AddRandItem(9, GL_FALSE);
            m_Pickers[RND_TSTCTRL_COUNT_PIXELS].AddRandItem(1, GL_TRUE);
            m_Pickers[RND_TSTCTRL_COUNT_PIXELS].CompileRandom();
            break;

        case RND_TSTCTRL_FINISH:
            // (picked after the send on each loop) do/don't call glFinish() after the draw.
            m_Pickers[RND_TSTCTRL_FINISH].ConfigConst(GL_TRUE);
            break;

        case RND_TSTCTRL_CLEAR_ALPHA_F:
            // (clear color: alpha, float32)
            m_Pickers[RND_TSTCTRL_CLEAR_ALPHA_F].FConfigConst(0.25f);
            break;

        case RND_TSTCTRL_CLEAR_RED_F:
            // (clear color: red, float32)
            m_Pickers[RND_TSTCTRL_CLEAR_RED_F].FConfigConst(0.25f);
            break;

        case RND_TSTCTRL_CLEAR_GREEN_F:
            // (clear color: green, float32)
            m_Pickers[RND_TSTCTRL_CLEAR_GREEN_F].FConfigConst(0.5f);
            break;

        case RND_TSTCTRL_CLEAR_BLUE_F:
            // (clear color: blue, float32)
            m_Pickers[RND_TSTCTRL_CLEAR_BLUE_F].FConfigConst(0.75f);
            break;

        case RND_TSTCTRL_DRAW_ACTION:
            // draw vertexes 90% pixels 5% 3dpixels 5%
            m_Pickers[RND_TSTCTRL_DRAW_ACTION].ConfigRandom();
            m_Pickers[RND_TSTCTRL_DRAW_ACTION].AddRandItem(18,
                                              RND_TSTCTRL_DRAW_ACTION_vertices);
            m_Pickers[RND_TSTCTRL_DRAW_ACTION].AddRandItem( 1,
                                                RND_TSTCTRL_DRAW_ACTION_pixels);
            if (m_3dPixelsSupported)
            {
                m_Pickers[RND_TSTCTRL_DRAW_ACTION].AddRandItem( 1,
                                              RND_TSTCTRL_DRAW_ACTION_3dpixels);
            }
            m_Pickers[RND_TSTCTRL_DRAW_ACTION].CompileRandom();
            break;

        case RND_TSTCTRL_ENABLE_XFB:
            m_Pickers[RND_TSTCTRL_ENABLE_XFB].ConfigConst(GL_FALSE);
            break;

        case RND_TSTCTRL_ENABLE_GL_DEBUG:
            m_Pickers[RND_TSTCTRL_ENABLE_GL_DEBUG].ConfigConst(GL_FALSE);
            break;

        case RND_TSTCTRL_LWTRACE:
            m_Pickers[RND_TSTCTRL_LWTRACE].ConfigConst(RND_TSTCTRL_LWTRACE_registry);
            break;

        case RND_TSTCTRL_RESTART_LWTRACE:
            m_Pickers[RND_TSTCTRL_RESTART_LWTRACE].ConfigConst(RND_TSTCTRL_LWTRACE_registry);
            break;

        case RND_TSTCTRL_PWRWAIT_DELAY_MS:
            m_Pickers[RND_TSTCTRL_PWRWAIT_DELAY_MS].ConfigRandom();
            m_Pickers[RND_TSTCTRL_PWRWAIT_DELAY_MS].AddRandRange(80, 6,  25);
            m_Pickers[RND_TSTCTRL_PWRWAIT_DELAY_MS].AddRandRange(20, 25, 100);
            m_Pickers[RND_TSTCTRL_PWRWAIT_DELAY_MS].CompileRandom();
            break;
        }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Perform OpenGL and object specific initialization.
RC RndMisc::InitOpenGL()
{
    RC rc = OK;
    bool b3dPixelsSupported = m_pGLRandom->HasExt(GLRandomTest::ExtGL_LW_draw_texture) != GL_FALSE;
    if (b3dPixelsSupported != m_3dPixelsSupported)
    {
        m_3dPixelsSupported = b3dPixelsSupported;
        if (!m_Pickers[RND_TSTCTRL_DRAW_ACTION].WasSetFromJs())
        {
            SetDefaultPicker(RND_TSTCTRL_DRAW_ACTION);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void RndMisc::Pick()
{
    MASSERT(0);  // This shouldn't be called.  Use PickFirst and PickLast.
}

//------------------------------------------------------------------------------
void RndMisc::Print(Tee::Priority pri)
{
}

//------------------------------------------------------------------------------
//Special callback routine used to print out OpenGL debug messages. Very useful
//when trying to determine why we are getting into a software fallback condition.
void GlRandomPrintGLDebugMsg(GLenum source,
                              GLenum type,
                              GLuint id,
                              GLenum severity,
                              GLsizei length,
                              const GLchar* message,
                              GLvoid* userParam)
{
    GLRandomTest * pGLRandom = (GLRandomTest *)userParam;
    Printf(Tee::PriHigh,
           "GL_DBG:Loop:%d source:%d type:%d id:%d severity:%d length:%d msg:%s\n",
           pGLRandom->GetLoop(),
           source,
           type,
           id,
           severity,
           length,
           message);

}

//------------------------------------------------------------------------------
RC RndMisc::Send()
{
    unsigned int isGLDebugEnabled = glIsEnabled(GL_DEBUG_OUTPUT);
    if( m_Pickers[RND_TSTCTRL_ENABLE_GL_DEBUG].GetPick() != isGLDebugEnabled)
    {
        if(m_Pickers[RND_TSTCTRL_ENABLE_GL_DEBUG].GetPick() != 0)
        {
            glEnable(GL_DEBUG_OUTPUT);
            // see g_debugMessages.h for more IDs
            uint dbgIds[] =
                            {0x00020092};          // __GL_DEBUG_PROGRAM_PERFORMANCE_WARNING
            // Disable performance releated messages and leave all others
            // enabled by default.
            glDebugMessageControlARB(
                GL_DEBUG_SOURCE_API_ARB,               // source,
                GL_DEBUG_TYPE_PERFORMANCE_ARB,         // type,
                GL_DONT_CARE,                          // severity,
                static_cast<UINT32>(NUMELEMS(dbgIds)), // count,
                dbgIds,                                // const uint* ids,
                false);                                // enabled
            glDebugMessageCallbackARB((GLDEBUGPROCARB)GlRandomPrintGLDebugMsg,m_pGLRandom);
        }
        else
        {
            glDisable(GL_DEBUG_OUTPUT);
            glDebugMessageCallbackARB(NULL,NULL);
        }
    }
    return mglGetRC();
}

//------------------------------------------------------------------------------
RC RndMisc::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
void RndMisc::PickFirst()
{
    m_Pickers[RND_TSTCTRL_STOP    ].Pick();
    m_Pickers[RND_TSTCTRL_LOGMODE ].Pick();
    m_Pickers[RND_TSTCTRL_DRAW_ACTION].Pick();
    m_XfbMode = m_Pickers[RND_TSTCTRL_ENABLE_XFB].Pick();

    // Default to the playback enabled state and allow remaining pickers to
    // decide if its not the case based on their picks and rules.
    m_XfbCapturePossible = (m_XfbMode & xfbModeCapture);
    m_XfbPlaybackPossible =
        ((m_XfbMode & xfbModePlayback) == xfbModePlayback);
    m_Pickers[RND_TSTCTRL_ENABLE_GL_DEBUG].Pick();
    m_Pickers[RND_TSTCTRL_PWRWAIT_DELAY_MS].Pick();
}

//------------------------------------------------------------------------------
void RndMisc::PickLast()
{
    m_Pickers[RND_TSTCTRL_SUPERVERBOSE  ].Pick();
    m_Pickers[RND_TSTCTRL_SKIP          ].Pick();
    m_Pickers[RND_TSTCTRL_COUNT_PIXELS  ].Pick();
    m_Pickers[RND_TSTCTRL_FINISH        ].Pick();
    m_Pickers[RND_TSTCTRL_LWTRACE       ].Pick();

    // Log our picks.
    UINT32 temp = m_Pickers[RND_TSTCTRL_ENABLE_XFB].GetPick();
    m_pGLRandom->LogData(RND_TSTCTRL, &temp, sizeof(temp));

    temp = m_Pickers[RND_TSTCTRL_COUNT_PIXELS].GetPick();
    m_pGLRandom->LogData(RND_TSTCTRL, &temp, sizeof(temp));
}
//------------------------------------------------------------------------------
RC RndMisc::Restart()
{
    m_Pickers.Restart();
    m_Pickers[RND_TSTCTRL_RESTART_SKIP].Pick();
    m_Pickers[RND_TSTCTRL_RESTART_LWTRACE].Pick();
    return OK;
}
//------------------------------------------------------------------------------
UINT32 RndMisc::LogMode() const
{
    return m_Pickers[RND_TSTCTRL_LOGMODE].GetPick();
}

//------------------------------------------------------------------------------
bool RndMisc::Stop() const
{
    return (0 != m_Pickers[RND_TSTCTRL_STOP].GetPick());
}

//------------------------------------------------------------------------------
bool RndMisc::SuperVerbose() const
{
    return (0 != m_Pickers[RND_TSTCTRL_SUPERVERBOSE].GetPick());
}

//------------------------------------------------------------------------------
UINT32 RndMisc::DrawAction() const
{
    return m_Pickers[RND_TSTCTRL_DRAW_ACTION].GetPick();
}

//------------------------------------------------------------------------------
UINT32 RndMisc::SkipMask() const
{
    return m_Pickers[RND_TSTCTRL_SKIP].GetPick();
}

//------------------------------------------------------------------------------
UINT32 RndMisc::RestartSkipMask() const
{
    return m_Pickers[RND_TSTCTRL_RESTART_SKIP].GetPick();
}

//------------------------------------------------------------------------------
UINT32 RndMisc::TraceAction() const
{
    return m_Pickers[RND_TSTCTRL_LWTRACE].GetPick();
}

//------------------------------------------------------------------------------
UINT32 RndMisc::RestartTraceAction() const
{
    return m_Pickers[RND_TSTCTRL_RESTART_LWTRACE].GetPick();
}

//------------------------------------------------------------------------------
bool RndMisc::Finish() const
{
    return (0 != m_Pickers[RND_TSTCTRL_FINISH].GetPick());
}

//------------------------------------------------------------------------------
bool RndMisc::CountPixels() const
{
    if (m_pGLRandom->HasExt(GLRandomTest::ExtLW_occlusion_query) &&
        m_Pickers[RND_TSTCTRL_COUNT_PIXELS].GetPick())
    {
        return true;
    }
    else
    {
        return false;
    }
}

//------------------------------------------------------------------------------
UINT32 RndMisc::GetPwrWaitDelayMs() const
{
    return m_Pickers[RND_TSTCTRL_PWRWAIT_DELAY_MS].GetPick();
}

//------------------------------------------------------------------------------
const GLfloat * RndMisc::ClearColor()
{
    m_ClearColor[0] = m_Pickers[RND_TSTCTRL_CLEAR_RED_F].FGetPick();
    m_ClearColor[1] = m_Pickers[RND_TSTCTRL_CLEAR_GREEN_F].FGetPick();
    m_ClearColor[2] = m_Pickers[RND_TSTCTRL_CLEAR_BLUE_F].FGetPick();
    m_ClearColor[3] = m_Pickers[RND_TSTCTRL_CLEAR_ALPHA_F].FGetPick();

    return m_ClearColor;
}

//------------------------------------------------------------------------------
bool RndMisc::XfbEnabled(UINT32 mode) const
{
    return ((m_XfbMode & mode) == mode);
}
UINT32 RndMisc::GetXfbMode() const
{
    return m_XfbMode;
}
//------------------------------------------------------------------------------
bool RndMisc::XfbIsCapturePossible() const
{
    return (m_XfbCapturePossible != 0);
}

//------------------------------------------------------------------------------
void RndMisc::XfbCaptureNotPossible()
{
    m_XfbCapturePossible = false;
    // If capture is not possible than neither is playback.
    m_XfbPlaybackPossible = false;
}

//------------------------------------------------------------------------------
bool RndMisc::XfbIsPlaybackPossible() const
{
    return (m_XfbPlaybackPossible != 0);
}

//------------------------------------------------------------------------------
void RndMisc::XfbPlaybackNotPossible()
{
    m_XfbPlaybackPossible = false;
}

//------------------------------------------------------------------------------
void RndMisc::XfbDisablePlayback()
{
    m_XfbMode &= ~xfbModePlayback;
}

