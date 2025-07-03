/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Implement the "mgl" functions for DOS/djgpp, macosx, linux w/o X, etc.
//
// On these platforms, we use our private LW_MODS GL driver and the resman,
// bypassing the native display driver (if any).

#include "modsgl.h"
#include "mgl_dtop.h"
#include "core/include/display.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/tasker.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "Lwcm.h"
#include "core/include/abort.h"

#include "gpu/tests/gputestc.h"
#include "lwReg.h"   // drivers/common/inc

// Define pointers to extension functions
#define GLEXTTAB(func, type) type __mods_##func;
#include "core/include/glexttab.h"
#undef GLEXTTAB

//-----------------------------------------------------------------------------
// Class mglTestContext static data.

UINT32  mglTestContext::s_NumStarted = 0;
void *  mglTestContext::s_Mutex = 0;
UINT32  mglTestContext::s_NumContexts = 0;

static INT32 GL_WindowOffsetX = 0;
static INT32 GL_WindowOffsetY = 0;
static INT32 GL_WindowWidth = 0;
static INT32 GL_WindowHeight = 0;
static GLboolean GL_WindowInitialized = GL_FALSE;

//-----------------------------------------------------------------------------
mglTestContext::mglTestContext()
:   m_GLInitialized(false),
    m_DispWidth(0),
    m_DispHeight(0),
    m_SurfWidth(0),
    m_SurfHeight(0),
    m_DispDepth(0),
    m_ZDepth(0),
    m_DispRR(0),
    m_FsaaMode(FSAADisabled),
    m_DoubleBuffer(false),
    m_pGpuDev(NULL),
    m_DisplayMgrReqs(DisplayMgr::PreferRealDisplay),
    m_BlockLinear(false),
    m_AA_METHOD(AA_METHOD_NONE),
    m_Samples(0),
    m_ColorCompress(false),
    m_DownsampleHint(GL_FASTEST),
    m_PropertiesOK(false),
    m_RenderToSysmem(false),
    m_UsingFbo(false),
    m_HaveFrontbuffer(false),
    m_EnableEcovDumping(false),
    m_NumLayersInFBO(0),
    m_pGrCtx(NULL),
    m_pDrawable(NULL),
    m_pDesktop(NULL),
    m_FboId(0),
    m_CopyFboFragProgId(0)
{
    s_NumContexts++;
    if (0 == s_Mutex)
    {
        s_Mutex = Tasker::AllocMutex("mglTestContext::s_Mutex",
                                     Tasker::mtxUnchecked);
    }
}

//-----------------------------------------------------------------------------
mglTestContext::~mglTestContext()
{
    s_NumContexts--;
    if (0 == s_NumContexts && 0 != s_Mutex)
    {
        Tasker::FreeMutex(s_Mutex);
        s_Mutex = 0;
    }
}

void mglTestContext::SetDumpECover(bool dumpECover)
{
    m_EnableEcovDumping = dumpECover;
}

//-----------------------------------------------------------------------------
RC mglTestContext::UpdateDisplay()
{
    if (m_pDesktop && m_HaveFrontbuffer)
        return m_pDesktop->UpdateDisplay(m_DispRR);

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Pass properties from JS into the object.
//
// ToDo: create a GpuTest derived class to hide most of this.
RC mglTestContext::SetProperties
(
    GpuTestConfiguration * pTstCfg,
    bool   DoubleBuffer,
    GpuDevice * pGpuDev,
    DisplayMgr::Requirements reqs,
    UINT32 ColorSurfaceFormatOverride,
    bool   ForceFbo,
    bool   RenderToSysmem,
    INT32  NumLayersInFBO
)
{
    RC rc;

    MASSERT(pTstCfg);
    MASSERT(pGpuDev);
    MASSERT(0 == m_FboTexIds.size());  // Setup should not have run yet.

    // Store properties that we don't have to callwlate.
    m_DispWidth         = pTstCfg->DisplayWidth();
    m_DispHeight        = pTstCfg->DisplayHeight();
    m_SurfWidth         = pTstCfg->SurfaceWidth();
    m_SurfHeight        = pTstCfg->SurfaceHeight();
    m_DispDepth         = pTstCfg->DisplayDepth();
    m_ZDepth            = pTstCfg->ZDepth();
    m_DispRR            = pTstCfg->RefreshRate();
    m_FsaaMode          = pTstCfg->GetFSAAMode();
    m_DoubleBuffer      = DoubleBuffer;
    m_pGpuDev           = pGpuDev;
    m_DisplayMgrReqs    = reqs;
    m_UsingFbo          = ForceFbo;
    m_RenderToSysmem    = RenderToSysmem;
    m_NumLayersInFBO    = NumLayersInFBO;

    // validate this color format first
    if (ColorSurfaceFormatOverride &&
        mglTexFmt2Color(ColorSurfaceFormatOverride) == ColorUtils::LWFMT_NONE)
    {
        Printf(Tee::PriHigh, "ColorSurfaceFormat:%x is not supported\n",
                ColorSurfaceFormatOverride);
        return RC::ILWALID_ARGUMENT;
    }

    m_SurfColorFormats.clear();
    m_SurfColorFormats.push_back(ColorSurfaceFormatOverride);

    UINT32 dtopColorFormat =
            mglColor2TexFmt(ColorUtils::ColorDepthToFormat(m_DispDepth));

    if (0 == m_SurfColorFormats[0])
        m_SurfColorFormats[0] = dtopColorFormat;

    if (m_SurfColorFormats[0] != dtopColorFormat)
        m_UsingFbo = true;

    if (m_DisplayMgrReqs == DisplayMgr::RequireNullDisplay &&
        m_FsaaMode == FSAADisabled)
    {
        // Not using display (forced offscreen) -- use FBO surfaces.
        m_UsingFbo = true;
    }

    // Figure out some full-scene-antialiasing stuff:

    // Logically, non-FSAA should be considered 1 sample, but the GL driver
    // wants 0 samples for non-FSAA.
    m_Samples = 0;
    m_DownsampleHint = GL_FASTEST;
    m_AA_METHOD = AA_METHOD_NONE;
    m_ColorCompress = FsaaModeIsFOS(m_FsaaMode);

    switch (m_FsaaMode)
    {
        case FSAADisabled:
            break;

        case FSAA2xQuinlwnx:
            m_DownsampleHint = GL_NICEST;
            m_Samples = 2;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_2X_QUINLWNX;
            break;

        case FSAA2x:
            m_Samples = 2;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_2X_DIAGONAL;
            break;

        case FSAA4xGaussian:
            m_DownsampleHint = GL_NICEST;
            m_Samples = 4;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_4X_GAUSSIAN;
            break;

        case FSAA4x:
            m_Samples = 4;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_4X;
            break;

        case FSAA2xDac:
            m_Samples = 2;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_2X_DIAGONAL;
            break;

        case FSAA2xQuinlwnxDac:
            m_Samples = 2;
            m_DownsampleHint = GL_NICEST;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_2X_QUINLWNX;
            break;

        case FSAA4xDac:
            m_Samples = 4;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_4X;
            break;

        case FSAA8x:
            m_Samples = 8;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_8X;
            break;

        case FSAA16x:
            m_Samples = 16;
            m_AA_METHOD = AA_METHOD_MULTISAMPLE_16X;
            break;

        case FSAA4v4:
            m_Samples = 4;
            m_AA_METHOD = AA_METHOD_VCAA_8X_4v4;
            break;

        case FSAA4v12:
            m_Samples = 4;
            m_AA_METHOD = AA_METHOD_VCAA_16X_4v12;
            break;

        case FSAA8v8:
            m_Samples = 8;
            m_AA_METHOD = AA_METHOD_VCAA_16X_8v8;
            break;

        case FSAA8v24:
            m_Samples = 8;
            m_AA_METHOD = AA_METHOD_VCAA_32X_8v24;
            break;

        default:
            MASSERT(0);  // We missed an enum value in this case statement.
            break;
    }

    m_BlockLinear =
        (m_pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HAS_BLOCK_LINEAR) ||
         Platform::GetSimulationMode() == Platform::Amodel) ?
        true : false;

    if (OK == rc)
        m_PropertiesOK = true;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Return true if settings provided by SetProperties work with device.
bool mglTestContext::IsSupported()
{
    if (! m_PropertiesOK)
    {
        MASSERT(!"Must call mglTestContext::SetProperties before IsSuported");
        return false;
    }

    GpuSubdevice * pGpuSub  = m_pGpuDev->GetSubdevice(0);

    if (!mglHwIsSupported(pGpuSub))
        return false;

    if (m_RenderToSysmem && pGpuSub->IsSOC())
    {
        Printf(Tee::PriLow, "Render-to-sysmem variant of the test disabled, "
                            "because it is not necessary,\n"
                            "since all surfaces are in sysmem anyway.\n");
        return false;
    }
    if (m_FsaaMode != FSAADisabled)
    {
        // Bug 200209583
        // We end up using FBO for FSAA tests for 16 BPP
        // Skip FSAA tests for 16 BPP, until this is supported
        if (m_DispDepth == 16)
            return false;

        if (! pGpuSub->HasFeature(Device::GPUSUB_SUPPORTS_FSAA))
            return false;

        if (m_Samples >= 8 && !pGpuSub->HasFeature(Device::GPUSUB_SUPPORTS_FSAA8X))
            return false;
    }

    const UINT32 zBpp = m_ZDepth/8;

    // Estimate total FB needed.

    const UINT32 zBytes = m_SurfWidth * m_SurfHeight * zBpp;
    const UINT32 dBytes = m_DispWidth * m_DispHeight * m_DispDepth/8;

    UINT32 rqdBytes = 0;

    if (m_FsaaMode != FSAADisabled)
    {
        // Z is bloated in FSAA modes.
        rqdBytes += m_Samples * zBytes;

        // We have normal-size dtop and an offscreen bloated C.
        rqdBytes += dBytes;
        rqdBytes += m_Samples * dBytes;
    }
    else
    {
        rqdBytes += zBytes;
        rqdBytes += dBytes;

        if (m_DoubleBuffer)
            rqdBytes += dBytes;

        // Assume we don't have FSAA and FBO at the same time for now.
        if (m_UsingFbo)
        {
            for (UINT32 si = 0; si < m_SurfColorFormats.size(); si++)
            {
                const UINT32 bpp = ColorUtils::PixelBytes(
                        mglTexFmt2Color(m_SurfColorFormats[si]));

                rqdBytes += m_SurfWidth * m_SurfHeight * bpp;
            }
        }
    }

    if (m_NumLayersInFBO > 0)
    {
        rqdBytes *= m_NumLayersInFBO;
    }

    // At 1024x768x32bpp, we estimate:
    // 4xFSAA  as 27MB, which doesn't work with 32MB FB.
    // 8xFSAA  as 51MB, which doesn't work with 64MB FB.
    // 8v8FSAA as 75MB, which doesn't work (reliably) with 128MB FB.
    //
    // So add a 75% fudge factor to avoid these modes where they will fail.
    // Ick.
    rqdBytes = (rqdBytes * 7) / 4;

    if (rqdBytes > pGpuSub->FbHeapSize())
    {
        if (Memory::Fb == Surface2D::GetActualLocation(Memory::Fb, pGpuSub))
        {
            Printf(Tee::PriLow, "Insufficient FB, not supported.\n");
            return false;
        }
    }

    for (size_t i = 0; i < m_SurfColorFormats.size(); ++i)
    {
        const mglTexFmtInfo * pFmtInfo = mglGetTexFmtInfo(
                mglTexFmt2Color(m_SurfColorFormats[i]));

        if ((0 == pFmtInfo) || (0 == pFmtInfo->InternalFormat))
        {
            Printf(Tee::PriLow, "Unsupported FBO color format 0x%x\n",
                    m_SurfColorFormats[i]);
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
void mglTestContext::SetSurfSize
(
    UINT32 surfWidth,
    UINT32 surfHeight
)
{
    m_SurfWidth = surfWidth;
    m_SurfHeight = surfHeight;
}

//-----------------------------------------------------------------------------
//! \brief Set up an extra color surface for MRT testing.
//!
//! Must be called after SetProperties and before Setup.
//!
RC mglTestContext::AddColorBuffer
(
    UINT32 ColorSurfaceFormat
)
{
    MASSERT(m_PropertiesOK);        // SetProperties has run
    MASSERT(m_pDesktop == NULL);    // Setup has not
    MASSERT(ColorSurfaceFormat);    // 0 is not a valid format!

    m_SurfColorFormats.push_back(ColorSurfaceFormat);

    // MRT must use FBO surfaces.
    m_UsingFbo = true;

    return OK;
 }

//-----------------------------------------------------------------------------
UINT32 mglTestContext::NumColorSurfaces() const
{
    return (UINT32) m_SurfColorFormats.size();
}

//-----------------------------------------------------------------------------
//! Return the GL color format selected by SetProperties/Setup.
//! Will be one of the following based on the
//! TestConfiguration::DisplayDepth parameter:
//!   16 : GL_RGB5  (i.e. r5g6b5)
//!   24 : GL_RGB8
//!   32 : GL_RGBA8
//! Or, may be any GL texture internal-format if ColorSurfaceFormatOverride
//! was used.
UINT32 mglTestContext::ColorSurfaceFormat(UINT32 colorIndex) const
{
    if (NumColorSurfaces() <= colorIndex)
        return 0;

    return m_SurfColorFormats[colorIndex];
}
UINT32 mglTestContext::ColorSurfaceFormat() const
{
    return ColorSurfaceFormat(0);
}

//-----------------------------------------------------------------------------
//! Returns true if GL context is initialized and it is safe to make calls to GL
bool mglTestContext::IsGLInitialized() const
{
    return m_GLInitialized;
}

//-----------------------------------------------------------------------------
//! Returns true if we are using FBO (FrameBufferObject) surfaces instead
//! of the primary surface.
bool mglTestContext::UsingFbo() const
{
    return m_UsingFbo;
}

//-----------------------------------------------------------------------------
//! Return pointer to the primary surface from the DisplayMgr::TestContext.
Surface2D * mglTestContext::GetPrimarySurface() const
{
    if (m_pDesktop && m_HaveFrontbuffer)
        return m_pDesktop->GetSurface2D();

    return 0;
}

DisplayMgr::TestContext *mglTestContext::GetTestContext() const
{
    if (m_pDesktop)
        return m_pDesktop->GetTestContext();
    return nullptr;
}

//-----------------------------------------------------------------------------
Display * mglTestContext::GetDisplay() const
{
    if (m_pDesktop && m_HaveFrontbuffer)
        return m_pDesktop->GetDisplay();

    if (m_pGpuDev)
        return m_pGpuDev->GetDisplayMgr()->GetNullDisplay();

    MASSERT(!"Tried to GetDisplay without pGpuDev!");
    return NULL;
}

//-----------------------------------------------------------------------------
UINT32 mglTestContext::GetPrimaryDisplay() const
{
    if (m_pDesktop && m_HaveFrontbuffer)
        return m_pDesktop->GetPrimaryDisplay();

    if (m_pGpuDev)
        return m_pGpuDev->GetDisplayMgr()->GetNullDisplay()->Selected();

    MASSERT(!"Tried to GetDisplay without pGpuDev!");
    return 1;
}

//-----------------------------------------------------------------------------
void mglTestContext::GetSurfaceTextureIDs(UINT32 *depthTextureID,
                                          UINT32 *colorSurfaceID) const
{
    MASSERT(depthTextureID && colorSurfaceID);
    *depthTextureID = m_FboTexIds[FBO_TEX_DEPTH_STENCIL];
    *colorSurfaceID = m_FboTexIds[FBO_TEX_COLOR0];
}

//-----------------------------------------------------------------------------
//! \brief Specify texture image depending on type of texture
/* static */ void mglTestContext::SpecifyTexImage(GLenum dim, GLint level,
            GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth,
            GLint border, GLenum format, GLenum type, GLvoid *data)
{
    if (dim == GL_TEXTURE_2D_ARRAY_EXT)
    {
        glTexImage3D(dim, level, internalFormat, width, height, depth,
                     border, format, type, data);
    }
    else
    {
        glTexImage2D(dim, level, internalFormat, width, height,
                     border, format, type, data);
    }

    glTexParameteri(dim, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(dim, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(dim, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(dim, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

//-----------------------------------------------------------------------------
//! \brief Attach the texture image to FBO
void mglTestContext::AttachTexToFBO(GLenum attachment, GLuint texture) const
{
    if (m_pGpuDev->GetSubdevice(0)->HasFeature
                            (Device::GPUSUB_SUPPORTS_MULTIPLE_LAYERS_AND_VPORT))
        glFramebufferTextureEXT(GL_FRAMEBUFFER_EXT, attachment, texture, 0);
    else
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment,
                                  GL_TEXTURE_2D, texture, 0);
}

//-----------------------------------------------------------------------------
//! \brief Start OpenGL, set display mode, create contexts, etc.
//!
//! Uses mglDesktop to share a surface with other GL tests on the GpuDevice.
//! Uses DisplayMgr to share display with other running tests.
//! Uses thread-local-storage in GL driver to bind the GC to this thread.
//!
//! Allocates the shared frontbuffer following the same logic as is used
//! in DisplayMgr::TestContext (mostly just first come, first served).
//!
//! For tests that don't get the frontbuffer and tests that want a
//! non-displayable color format, allocates a framebuffer object.
//!
//! Call copyFboToFront() to render the FBO contents to the front buffer,
//! (noop if test doesn't own frontbuffer).
RC mglTestContext::Setup()
{
    PROFILE_ZONE(GENERIC)

    RC rc;
    Tasker::MutexHolder mh(s_Mutex);

    if (!m_PropertiesOK)
        return RC::SOFTWARE_ERROR;

    // The DOS GL library thinks it's a dll.  Do a fake "thread attach".
    CHECK_RC( mglErrorToRC( dglThreadAttach() ));

    // Start OpenGL on this device (disables user interface).
    // Allocs primary surface(s) and describes them to GL.
    // Possibly the surface is already in use, we'll have to draw offscreen.

    bool mustDrawOffscreen = false;

    CHECK_RC(mglDesktop::Setup(
            m_pGpuDev,
            m_DispWidth,
            m_DispHeight,
            m_SurfWidth,
            m_SurfHeight,
            m_DispDepth,
            m_FsaaMode,
            m_ColorCompress,
            m_BlockLinear,
            m_AA_METHOD,
            m_DownsampleHint,
            &mustDrawOffscreen));

    if (mustDrawOffscreen)
    {
        m_UsingFbo = true;
        if (m_DisplayMgrReqs == DisplayMgr::RequireRealDisplay)
            return RC::DISPLAY_IN_USE;

        // Tell mglDesktop::Alloc not to alloc us the real display or the
        // primary surface, we can't use it.
        m_DisplayMgrReqs = DisplayMgr::RequireNullDisplay;
    }

    // Mark the mglDesktop used.
    // If we get the real Display and front-buffer, set mode & image.
    CHECK_RC(mglDesktop::Alloc(
            m_pGpuDev,
            this,
            m_DisplayMgrReqs,
            m_DispWidth,
            m_DispHeight,
            m_DispRR,
            &m_pDesktop,
            &m_HaveFrontbuffer));

    if (!m_HaveFrontbuffer &&
        m_FsaaMode == FSAADisabled)
    {
        // Another test has the frontbuffer, we must draw offscreen.
        m_UsingFbo = true;
    }

    // We draw offscreen for many possible reasons:
    //  - The frontbuffer is in use by another test.
    //  - The frontbuffer is idle but of the wrong format/size/fsaa/etc and
    //    we can't realloc it to spec because other tests are UsingFbo on
    //    the same device.  The realloc would ilwalidate their GCs.
    //  - We asked to draw offscreen with RequireNullDisplay.
    //  - We are using a non-default colorformat that display can't scan out.
    //
    // Due to that last case, both m_HaveFrontbuffer and m_USingFbo may be true.
    // But they both can't be false, because we have to draw somewhere! - is this comment valid?
    // I could run gputest successfully without usign fbo and with null_display
    if (m_FsaaMode == FSAADisabled && !m_HaveFrontbuffer && !m_UsingFbo)
    {
        Printf(Tee::PriHigh, "Do not have front buffer to render to and not using a FBO.\n");
        return RC::DISPLAY_IN_USE;
    }

    if (m_FsaaMode != FSAADisabled && m_UsingFbo)
    {
        Printf(Tee::PriHigh, "Can not use FBO in FSAA mode.\n");
        return RC::SOFTWARE_ERROR;
    }

    // @@@
    // How can Cleanup know when to decrement Desktop::RefCount or s_NumStarted?
    // Since it lwrrently cannot be sure, we'll over-free after a Setup failure
    // and crash on shutdown.
    // Adding m_IncrementedRefCount and m_IncrementedNumStarted flags to the
    // object would make the code brittle, there's surely a cleaner way.

    bool firstStart = (0 == s_NumStarted);
    s_NumStarted++;

    // Set up offsets for the "window" that we're rendering to, either set by
    // mglSetWindow(), or directly derived from the parameters passed in.

    INT32 WindowOffsetX = INT32(0);
    INT32 WindowOffsetY = INT32(0); // @@@ y-ilwersion?
    INT32 WindowWidth   = INT32(m_SurfWidth);
    INT32 WindowHeight  = INT32(m_SurfHeight);

    if (GL_WindowInitialized)
    {
        // @@@ Used only by lwogtest, how does it work?
        WindowOffsetX += GL_WindowOffsetX;
        WindowOffsetY += GL_WindowOffsetY;
        WindowWidth = GL_WindowWidth;
        WindowHeight = GL_WindowHeight;
    }

    // Setup the "window" data for the simplified window manager.
    // Chooses a pixel-format matching the desktop and allocs a GL drawable.
#if defined(CREATE_DRAWABLE_USES_AA_METHOD)
    // In order to support VCAA aaMethod in MODS we want to pass the aaMethod to
    // dglCreateDrawableWindow instead of the number of sample. This modification
    // to the dgl interface create an incompatibility between MODS and ogl.
    // The token CREATE_DRAWABLE_USES_AA_METHOD is defined in the new version of dgl.h.
    // Here MODS will pass the aaMethod or the number of sample if this token is defined
    // or if it's not.
    CHECK_RC( mglErrorToRC( dglCreateDrawableWindow
    (
        (GLint)(m_pGpuDev->GetDeviceInst()),
        WindowOffsetX,
        WindowOffsetY,
        WindowWidth,
        WindowHeight,
        m_ZDepth,
        m_DoubleBuffer,
        m_AA_METHOD,
        &m_pDrawable
    )));
#else
    CHECK_RC( mglErrorToRC( dglCreateDrawableWindow
    (
        (GLint)(m_pGpuDev->GetDeviceInst()),
        WindowOffsetX,
        WindowOffsetY,
        WindowWidth,
        WindowHeight,
        m_ZDepth,
        m_DoubleBuffer,
        m_Samples,
        &m_pDrawable
    )));
#endif

    // Create a graphics context for the window.
    CHECK_RC( mglErrorToRC( dglCreateContext
    (
        m_pDrawable,
        &m_pGrCtx
    )));

    // Bind the context to the drawable and the current thread.
    CHECK_RC( mglErrorToRC( dglMakeLwrrent
    (
        m_pGrCtx,
        m_pDrawable,
        m_pDrawable
    )));

   // Get extension function pointers.
   if (firstStart)
   {
#define GLEXTTAB(func, type) __mods_##func = (type)dglGetProcAddress((const GLubyte *)#func);
#include "core/include/glexttab.h"
#undef GLEXTTAB

        // Workaround for bug 719997 issues: DOS-GL has only LW_tessellation_program
        // extension-functions, others have only ARB_tessellation shader functions!
        // They are identical except for the names.
        // Copy function-pointers back and forth so that both names work equally well.

        if (NULL == __mods_glPatchParameteri)
            __mods_glPatchParameteri = __mods_glPatchParameteriLW;
        else if (NULL == __mods_glPatchParameteriLW)
            __mods_glPatchParameteriLW = __mods_glPatchParameteri;

        if (NULL == __mods_glPatchParameterfv)
            __mods_glPatchParameterfv = __mods_glPatchParameterfvLW;
        else if (NULL == __mods_glPatchParameterfvLW)
            __mods_glPatchParameterfvLW = __mods_glPatchParameterfv;
    }

    if (m_UsingFbo)
    {
        bool FboSupported = mglExtensionSupported("GL_EXT_framebuffer_object");

        if (!FboSupported)
        {
            m_UsingFbo = false;
            Printf(Tee::PriHigh,
            "GL_EXT_framebuffer_object not supported, can't draw offscreen.\n");
            return RC::UNSUPPORTED_COLORFORMAT;
        }

        GLint MaxColorBufs;
        glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &MaxColorBufs);

        if (MaxColorBufs < (GLint) NumColorSurfaces())
        {
            Printf(Tee::PriHigh,
                "Requested %d color buffers, but this HW supports only %d.",
                NumColorSurfaces(), MaxColorBufs);
            return RC::UNSUPPORTED_COLORFORMAT;
        }

        if (!mglExtensionSupported("GL_EXTX_framebuffer_mixed_formats"))
        {
            // This gpu requires all color surfaces have the same format.
            // Note: EXTX above is not a typo.

            for (UINT32 mrtIdx = 1; mrtIdx < NumColorSurfaces(); mrtIdx++)
            {
                if (m_SurfColorFormats[mrtIdx] != m_SurfColorFormats[0])
                {
                    // Aurora GPU does not support mixed formats
                    Printf(Tee::PriWarn,
                            "MRT surf %d forced to %s (from %s), mixed formats not supported.\n",
                            mrtIdx,
                            mglEnumToString(m_SurfColorFormats[0]),
                            mglEnumToString(m_SurfColorFormats[mrtIdx]));

                    m_SurfColorFormats[mrtIdx] = m_SurfColorFormats[0];
                }
            }
        }

        MASSERT(0 == m_FboTexIds.size());

#if defined GL_EXT_framebuffer_object
        Printf(Tee::PriLow, "Alloc FBO surface for C0 (%s)\n",
                mglEnumToString(m_SurfColorFormats[0]));

        // Create the framebuffer_object itself.
        glGenFramebuffersEXT(1, &m_FboId);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboId);

        // Alloc all texture ids for the framebuffer_object attachments.
        m_FboTexIds.insert(
                m_FboTexIds.begin(),
                NumColorSurfaces() + 1,
                GLuint(0));

        const mglTexFmtInfo * pFmtInfo;

        // Set up the texture for the Z surface.

        glGenTextures(1, &m_FboTexIds[FBO_TEX_DEPTH_STENCIL]);

        if (m_RenderToSysmem)
        {
            glEnable(GL_FORCE_SYSMEM_TEXTURE_LW);
        }

        ColorUtils::Format zfmt;
        switch (m_ZDepth)
        {
            default:    MASSERT(0);
                        /* fall through */

            case 16:    zfmt = ColorUtils::Z16; break;

            case 24:    zfmt = ColorUtils::X8Z24; break;

            case 32:    zfmt = ColorUtils::Z24S8; break;
        }
        pFmtInfo = mglGetTexFmtInfo(zfmt);
        if (NULL == pFmtInfo)
            return RC::UNSUPPORTED_DEPTH;

        GLenum texDim;

        if (m_NumLayersInFBO > 0)
        {
            //Layered rendering supports only 1 color surface
            if (NumColorSurfaces() > 1)
            {
                Printf(Tee::PriHigh, "Layered Rendering supports a single color surface.");
                return RC::SOFTWARE_ERROR;
            }

            GLint maxTexLayers;
            glGetIntegerv (GL_MAX_ARRAY_TEXTURE_LAYERS_LW, &maxTexLayers);
            m_NumLayersInFBO = min(max<GLint>(1, m_NumLayersInFBO),
                                   maxTexLayers);
            texDim = GL_TEXTURE_2D_ARRAY_EXT;
        }
        else
        {
            texDim = GL_TEXTURE_2D;
        }

        // Setup depth/stencil attachment
        glBindTexture(texDim, m_FboTexIds[FBO_TEX_DEPTH_STENCIL]);

        mglTestContext::SpecifyTexImage(texDim, 0, pFmtInfo->InternalFormat,
                                m_SurfWidth, m_SurfHeight, m_NumLayersInFBO, 0,
                                pFmtInfo->ExtFormat, pFmtInfo->Type, NULL);

        AttachTexToFBO(GL_DEPTH_ATTACHMENT_EXT, m_FboTexIds[FBO_TEX_DEPTH_STENCIL]);
        if (m_ZDepth >= 32)
        {
            AttachTexToFBO(GL_STENCIL_ATTACHMENT_EXT,
                           m_FboTexIds[FBO_TEX_DEPTH_STENCIL]);
        }

        // Set up the texture(s) for the color surface(s).
        // For layered rendering we have only 1 color surface
        for (UINT32 ci = 0; ci < NumColorSurfaces(); ci++)
        {
            glGenTextures(1, &m_FboTexIds[FBO_TEX_COLOR0 + ci]);

            pFmtInfo = mglGetTexFmtInfo(mglTexFmt2Color(ColorSurfaceFormat(ci)));
            if (NULL == pFmtInfo)
                return RC::UNSUPPORTED_COLORFORMAT;

            // Set up the texture for the color surface.
            glActiveTexture(GL_TEXTURE0_ARB);
            glBindTexture(texDim, m_FboTexIds[FBO_TEX_COLOR0 + ci]);

            mglTestContext::SpecifyTexImage(texDim, 0, ColorSurfaceFormat(ci),
                        m_SurfWidth, m_SurfHeight, m_NumLayersInFBO, 0,
                        pFmtInfo->ExtFormat, pFmtInfo->Type, NULL);

            AttachTexToFBO(GL_COLOR_ATTACHMENT0_EXT + ci,
                           m_FboTexIds[FBO_TEX_COLOR0 + ci]);
        }
        glBindTexture(texDim, 0);

        if (m_RenderToSysmem)
        {
            glDisable(GL_FORCE_SYSMEM_TEXTURE_LW);
        }

        // Did we set up the framebuffer correctly?
        CHECK_RC(mglCheckFbo());

        const char * CopyFboFragProg;
        if (m_NumLayersInFBO > 0)
        {
            CopyFboFragProg = "!!LWfp4.0\n"
                "TEX.F result.color, fragment.texcoord[0], texture[0], ARRAY2D;\n"
                "END\n";
        }
        else
        {
            CopyFboFragProg = "!!FP1.0\n"
                "TEX o[COLR], f[TEX0], TEX0, 2D;\n"
                "END\n";
        }

        glGenProgramsLW(1, &m_CopyFboFragProgId);
        CHECK_RC(mglLoadProgram(
                GL_FRAGMENT_PROGRAM_LW,
                m_CopyFboFragProgId,
                (GLsizei) strlen(CopyFboFragProg),
                (const GLubyte *)CopyFboFragProg));

        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

        if (!mglExtensionSupported("GL_ARB_draw_buffers"))
        {
            glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
        }
        else
        {
            // MRT case, enable multiple color buffers at once.
            vector<GLenum>bufs(NumColorSurfaces(), 0);
            for (int i = 0; i < (int) NumColorSurfaces(); i++)
                bufs[i] = GL_COLOR_ATTACHMENT0_EXT + i;

            glDrawBuffersARB(NumColorSurfaces(), &bufs[0]);
        }

#endif // defined GL_EXT_framebuffer_object
    }

    m_GLInitialized = true;

    // Wait for all contexts across multiple GPUs to finish Setup()
    // so that the OpenGL/Vulkan driver will see all GPUs during the
    // main test's Setup().
    mh.Release();
    Tasker::WaitOnBarrier();

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Release all OpenGL and other resources.
RC mglTestContext::Cleanup()
{
    Tasker::MutexHolder mh(s_Mutex);

    if (false == m_PropertiesOK)
    {
        // SetProperties hasn't run.
        return OK;
    }

    // This is cleanup code, we will preserve the first error but continue
    // freeing resources all the way to the end no matter what.
    RC rc;
    RC firstRc = OK;

    if (m_UsingFbo)
    {
#if defined GL_EXT_framebuffer_object
        if (m_FboId)
            glDeleteFramebuffersEXT(1, &m_FboId);

        for (size_t i = 0; i < m_FboTexIds.size(); i++)
        {
            glDeleteTextures(1, &m_FboTexIds[i]);
        }
        m_FboTexIds.clear();

        if (m_CopyFboFragProgId)
            glDeleteProgramsLW(1, &m_CopyFboFragProgId);

        m_FboId = 0;

        m_CopyFboFragProgId = 0;
#endif
    }

    if (m_pGrCtx)
    {
        // Capture any RM robust-channels error data before freeing the context.
        rc = mglGetChannelError();
        if (OK == firstRc)
            firstRc = rc;

        // Free the graphics context.
        rc = mglErrorToRC( dglDeleteContext(m_pGrCtx) );
        if (OK == firstRc)
            firstRc = rc;

        m_pGrCtx = 0;
    }

    if (m_pDrawable)
    {
        // Free the drawable.
        rc = mglErrorToRC( dglDeleteDrawable(m_pDrawable));
        if (OK == firstRc)
            firstRc = rc;

        m_pDrawable = 0;
    }

    // Bug 528129: [GT21X] Running GLTests in interactive MODS causes asserts
    // Problem: mglDesktop::Free() calls EnableUserInterface() and
    // dglThreadDetach() makes some RM calls which don't work in VGA mode.
    // Hence, we DisableUserInterface() here and EnableUserInterface() after
    // dglThreadDetach()
    Platform::DisableUserInterface();

    if (m_pDesktop)
    {
        mglDesktop::Free(m_pDesktop, this);
        m_pDesktop = 0;
    }

    // The test has to call SetProperties again before calling Setup.
    m_PropertiesOK = false;

    m_SurfColorFormats.clear();

    if (s_NumStarted)
    {
        s_NumStarted--;
    }

    // Free thread-local-storage in the GL driver for this thread.
    rc = mglErrorToRC( dglThreadDetach() );
    if (OK == firstRc)
        firstRc = rc;

    Platform::EnableUserInterface();

    if ((m_EnableEcovDumping) && (Platform::GetSimulationMode()==Platform::Fmodel))
    {
        Platform::EscapeWrite("LwrrentTestEnded", 0, 4, 0);
    }

    m_GLInitialized = false;

    return firstRc;
}

//------------------------------------------------------------------------------
//! Copy framebuffer-object color surface to primary surface, assuming the
//! test actually has both.  Noop otherwise.
RC mglTestContext::CopyFboToFront()
{
    RC rc;
    if (m_UsingFbo && m_HaveFrontbuffer)
    {
#if defined GL_EXT_framebuffer_object

        GLfloat worldLeft = -1.0;
        GLfloat worldRight = 1.0;
        GLfloat worldBottom = -1.0;
        GLfloat worldTop = 1.0;
        GLfloat worldNear = -1.0;
        GLfloat worldFar = 1.0;

        glPushAttrib(GL_ALL_ATTRIB_BITS);

        // disable anything that would change the appearance of the color buffer
        // when copying from src to dest.
        glDisable(GL_COLOR_LOGIC_OP);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_DITHER);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

        // Bind the FBO color buffer as texture 0.
        glActiveTexture(GL_TEXTURE0);
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glLoadIdentity();

        if (m_NumLayersInFBO > 0)
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY_EXT, m_FboTexIds[FBO_TEX_COLOR0]);
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D, m_FboTexIds[FBO_TEX_COLOR0]);
        }

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(worldLeft,   worldRight,
                worldBottom, worldTop,
                worldNear,   worldFar);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glTranslated(0.0, 0.0, -(worldFar+worldNear)/2);

        glEnable(GL_FRAGMENT_PROGRAM_LW);
        glBindProgramLW(GL_FRAGMENT_PROGRAM_LW, m_CopyFboFragProgId);

        GLfloat worldHeight = worldTop - worldBottom;
        GLfloat worldWidth  = worldRight - worldLeft;

        // Draw the smallest triangle that covers the screen.
        // The GPU will clip away the overlap.
        glBegin(GL_TRIANGLES);

        // glMultiTexCoord3f is used in case of displaying a layered image where we
        // copy the first layer to screen
        glMultiTexCoord3f(GL_TEXTURE0, 0.0, 2.0, 0.0); // upper left + 1 screen height
        glVertex2f(worldLeft, worldTop + worldHeight);

        glMultiTexCoord3f(GL_TEXTURE0, 0.0, 0.0, 0.0); // lower left
        glVertex2f(worldLeft, worldBottom);

        glMultiTexCoord3f(GL_TEXTURE0, 2.0, 0.0, 0.0);  // lower right + 1 screen width
        glVertex2f( worldRight + worldWidth,  worldBottom);

        glEnd();
        glFlush();

        glDisable(GL_FRAGMENT_PROGRAM_LW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboId);
        glPopAttrib();

        rc = mglGetRC();
#endif
    }
    return rc;
}

UINT32 mglTestContext::NumColorSamples() const
{
    return m_Samples;
}

//------------------------------------------------------------------------------
// mglHwIsSupported()
//
// Returns true if this HW is supported (accellerated) by the driver.
// For example, returns false for lw11 in rel95 and later.
//
bool mglHwIsSupported(GpuSubdevice *pSubdevice)
{
    MASSERT(pSubdevice);

    bool supported = true;

    ThreeDAlloc threeDAlloc;
    if (!threeDAlloc.IsSupported(pSubdevice->GetParentDevice()))
    {
        Printf(Tee::PriLow, "This GPU does not support graphics.\n");
        supported = false;
    }

    return supported;
}

//------------------------------------------------------------------------------
// mglSetWindow()
//
// Specifies the parameters used for the window created internally for
// rendering.  Used to test window offset parameters.
//
void mglSetWindow(int XOffset, int YOffset, int Width, int Height)
{
    GL_WindowOffsetX = XOffset;
    GL_WindowOffsetY = YOffset;
    GL_WindowWidth = Width;
    GL_WindowHeight = Height;
    GL_WindowInitialized = GL_TRUE;
}

//------------------------------------------------------------------------------
RC mglShutdownLibrary()
{
   // Clear out extension function pointers
#define GLEXTTAB(func, type) __mods_##func = NULL;
#include "core/include/glexttab.h"
#undef GLEXTTAB

   return mglErrorToRC( dglProcessDetach() );
}

//------------------------------------------------------------------------------
void mglSwapBuffers()
{
   dglSwapBuffers();
}

void mglTestContext::SwapBuffers()
{
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    mglSwapBuffers();
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_FboId);
}

//------------------------------------------------------------------------------
MglDispatchMode mglGetDispatchMode()
{
   UINT32 m = dglGetDispatchMode();
   switch (m)
   {
      case 0:  return mglSoftware;
      case 1:  return mglVpipe;
      case 2:  return mglHardware;

      default: return mglUnknown;
   }
}

//------------------------------------------------------------------------------
RC mglGetChannelError()
{
   // Check for Ctrl-C
   RC rc;
   CHECK_RC(Abort::Check());

   // Check for robust channels errors
   GLuint err = dglGetRobustChannelsError();
   return Channel::RobustChannelsCodeToModsRC(err);
}
