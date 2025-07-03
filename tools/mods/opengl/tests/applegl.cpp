/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012,2014-2016,2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// OpenGL "hello world" sort of thing...

#include "core/include/massert.h"
#include "opengl/modsgl.h"
#include "random.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/tests/testconf.h"
#include "core/include/golden.h"
#include "opengl/glgoldensurfaces.h"
#include "core/include/display.h"
#include "core/include/gpu.h"
#include "gpu/tests/gputest.h"
#include "core/include/fpicker.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/chanwmgr.h"

#define RAND()  (GetFpContext()->Rand.GetRandom())

#define     UNIPOLAR_RAND   (GetFpContext()->Rand.GetRandomFloat(0.0, 1.0))
#define     BIPOLAR_RAND    (GetFpContext()->Rand.GetRandomFloat(-1.0, 1.0))
#define     RAND_X          ((int) (UNIPOLAR_RAND * m_pTstCfg->SurfaceWidth()))
#define     RAND_Y          ((int) (UNIPOLAR_RAND * m_pTstCfg->SurfaceHeight()))
#define     RAND_Xd         ((int) (BIPOLAR_RAND * m_pTstCfg->SurfaceWidth() * 0.05))
#define     RAND_Yd         ((int) (BIPOLAR_RAND * m_pTstCfg->SurfaceHeight() * 0.05))

//------------------------------------------------------------------------------
class AppleGL : public GpuTest
{
public:
    AppleGL();
    TestConfiguration * m_pTstCfg;                     // standard test options
    Goldelwalues      * m_pGolden;
    GLGoldenSurfaces  * m_pGSurfs;
    mglTestContext      m_mglTestContext;

   RC Run();
   RC Setup();
   RC Cleanup();

   bool IsSupported();

   RC   InitGlStuff();
   RC   CleanupGlStuff();
   RC   LoopRandomMethods();
   RC   DoReadPixels(void * dummy, void * pDest, int bufIndex);

   INT32             m_DisplayWidth;      // height in pixels     (from m_TstCfg)
   INT32             m_DisplayHeight;     // width in pixels      (from m_TstCfg)
   UINT32            m_DisplayDepth;      // color depth in bits     (from m_TstCfg)
   UINT32            m_ZDepth;            // Z buffer depth in bits  (from m_TstCfg)
   ColorUtils::Format m_ColorSurfaceFormat;   // if !0, use this surface format regardless of TestConfiguration.DisplayDepth.
   GLenum            m_SurfFormat;
   GLuint            m_TexId;
private:
};

AppleGL::AppleGL()
{
    m_pGSurfs = NULL;
    m_pTstCfg = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
    m_DisplayWidth = 0;
    m_DisplayHeight = 0;
    m_DisplayDepth = 0;
    m_ZDepth = 0;
    m_ColorSurfaceFormat = ColorUtils::LWFMT_NONE;
    m_SurfFormat = 0;
    m_TexId = 0;
}

//------------------------------------------------------------------------------
bool AppleGL::IsSupported()
{
    if (!mglHwIsSupported(GetBoundGpuSubdevice()))
       return false;

    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
        return false;

    return true;
}

//------------------------------------------------------------------------------
RC AppleGL::Setup()
{
    RC rc = OK;

    CHECK_RC(GpuTest::Setup());

    CHECK_RC( m_mglTestContext.SetProperties(
            GetTestConfiguration(),
            false,  // double buffer
            GetBoundGpuDevice(),
            GetDispMgrReqs(),
            0,      // don't override color format
            true,   // Force render to framebuffer object.
            false,  // Don't render FBOs to sysmem.
            0));    // Do not use layered FBO

    m_SurfFormat = m_mglTestContext.ColorSurfaceFormat();


    m_pGSurfs = new GLGoldenSurfaces();
    m_DisplayWidth    = m_pTstCfg->SurfaceWidth();
    m_DisplayHeight   = m_pTstCfg->SurfaceHeight();
    m_DisplayDepth    = m_pTstCfg->DisplayDepth();
    m_ZDepth          = m_pTstCfg->ZDepth();
    m_ColorSurfaceFormat = mglTexFmt2Color(m_SurfFormat);

    CHECK_RC(m_mglTestContext.Setup());
    Display *pDisplay = m_mglTestContext.GetDisplay();

    m_pGSurfs->DescribeSurfaces(
            m_DisplayWidth,
            m_DisplayHeight,
            m_ColorSurfaceFormat,
            ColorUtils::ZDepthToFormat(m_pTstCfg->ZDepth()),
            pDisplay->Selected());
    m_pGolden->SetSurfaces(m_pGSurfs);
    m_pGolden->SetYIlwerted(true);

    if (m_mglTestContext.UsingFbo())
    {
        m_pGSurfs->SetReadPixelsBuffer(GL_COLOR_ATTACHMENT0_EXT);
    }
    else
    {
       m_pGSurfs->SetReadPixelsBuffer(GL_FRONT);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC AppleGL::Run()
{
    RC rc;
    CHECK_RC(LoopRandomMethods());
    return rc;
}

//------------------------------------------------------------------------------
RC AppleGL::Cleanup()
{
    StickyRC firstRc;

    firstRc = CleanupGlStuff();

    m_pGolden->ClearSurfaces();
    delete m_pGSurfs;
    m_pGSurfs = NULL;

    firstRc = m_mglTestContext.Cleanup();
    firstRc = GpuTest::Cleanup();

    return firstRc;
}

RC AppleGL::InitGlStuff()
{
    glGenTextures(1, &m_TexId);

    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    gluOrtho2D( 0.0, (GLfloat) m_pTstCfg->SurfaceWidth(), 0.0,
               (GLfloat) m_pTstCfg->SurfaceHeight() );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    glEnable( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, m_TexId ); //bind our texture to a texture2d
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );

    GetFpContext()->Rand.SeedRandom(m_pTstCfg->Seed());
    int txW = 64;
    int txH = 64;
    unsigned long * pbuf = new unsigned long [txW * txH];
    int t,s;
    for (t = 0; t < txW; t++)
    {
        for (s = 0; s < txH; s++)
        {
            pbuf[t + s*txW] = RAND();
        }
    }

    glTexImage2D( GL_TEXTURE_2D, 0, 4, txW, txH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pbuf );  //load our data
    delete [] pbuf;
    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
    glEnable( GL_DEPTH_TEST );

    return OK;
}

RC AppleGL::CleanupGlStuff()
{
   RC rc;
   if (m_mglTestContext.IsGLInitialized())
   {
       glDeleteTextures(1, &m_TexId);
       glDisable( GL_TEXTURE_2D );
       glBindTexture( GL_TEXTURE_2D, 0 );
       glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
       glDisable( GL_DEPTH_TEST );
       rc = mglGetRC();
   }
   return rc;
}

RC AppleGL::LoopRandomMethods()
{
    RC rc;

    UINT32 StartLoop        = m_pTstCfg->StartLoop();
    UINT32 RestartSkipCount = m_pTstCfg->RestartSkipCount();
    UINT32 Loops            = m_pTstCfg->Loops();
    UINT32 EndLoop          = StartLoop + Loops;
    UINT32 Seed             = m_pTstCfg->Seed();
    FancyPicker::FpContext * pFpCtx = GetFpContext();

    CHECK_RC( InitGlStuff());

    if ((StartLoop % RestartSkipCount) != 0)
    {
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");
    }

    m_pGolden->SetLoop( StartLoop );
    for (pFpCtx->LoopNum = StartLoop; pFpCtx->LoopNum < EndLoop; ++pFpCtx->LoopNum)
    {

        if ((pFpCtx->LoopNum == StartLoop) || ((pFpCtx->LoopNum % RestartSkipCount) == 0))
        {
            // Restart point.

            Printf(Tee::PriLow, "\n\tRestart: loop %d, seed %#010x\n",
                pFpCtx->LoopNum, Seed + pFpCtx->LoopNum);

            pFpCtx->Rand.SeedRandom(Seed + pFpCtx->LoopNum);

            // We need to clear the stencil on lw40 to hit the fastclear path
            glClearStencil(0xff);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            glFlush();
            CHECK_RC(mglGetRC());
        }

        // Draws one quad
        {
            UINT32 x = 0;
            UINT32 y = 0;

            glBegin( GL_QUADS );
            glColor3f ( UNIPOLAR_RAND, UNIPOLAR_RAND, UNIPOLAR_RAND );
            glTexCoord2i( 0, 0 );
            glVertex2i( x=RAND_X, y=RAND_Y );
            glColor3f ( UNIPOLAR_RAND, UNIPOLAR_RAND, UNIPOLAR_RAND );
            glTexCoord2i( 0, 1 );
            glVertex2i( x+RAND_Xd, y+RAND_Yd );
            glColor3f ( UNIPOLAR_RAND, UNIPOLAR_RAND, UNIPOLAR_RAND );
            glTexCoord2i( 1, 1 );
            glVertex2i( x+RAND_Xd, y+RAND_Yd );
            glColor3f ( UNIPOLAR_RAND, UNIPOLAR_RAND, UNIPOLAR_RAND );
            glTexCoord2i( 1, 0 );
            glVertex2i( x+RAND_Xd, y+RAND_Yd );
            glEnd();

        }

        if ((pFpCtx->LoopNum % RestartSkipCount) == (RestartSkipCount-1))
        {
            CHECK_RC(m_mglTestContext.CopyFboToFront());
            glFinish();
        }

        // Goldens
        {
            rc = m_pGolden->Run();
        }
        if (OK != rc)
        {
            m_mglTestContext.CopyFboToFront();
            return rc;
        }
    }

    return rc;
}

RC AppleGL::DoReadPixels(void * dummy, void * pDest, int bufIndex)
{
   // We *don't* know the actual location of the surface.
   GLenum fmt;
   GLenum typ;
   GLuint height = m_DisplayHeight;

   if (bufIndex == 0)
   {
      switch (m_SurfFormat)
      {
      case GL_RGB5_A1:        fmt = GL_BGRA;
         typ = GL_UNSIGNED_SHORT_5_5_5_1;
         break;

      case GL_RGB5:           fmt = GL_BGR;
         typ = GL_UNSIGNED_SHORT_5_6_5;
         break;

      case GL_RGBA8:          fmt = GL_BGRA;
         typ = GL_UNSIGNED_BYTE;
         break;

      default: MASSERT(0);
         return RC::UNSUPPORTED_COLORFORMAT;
      }
   }
   else
   {
      MASSERT(bufIndex == 1);

      switch (m_ZDepth)
      {
      case 32: fmt = GL_DEPTH_STENCIL_LW;
         typ = GL_UNSIGNED_INT_24_8_LW;
         break;
      case 16: fmt = GL_DEPTH_COMPONENT;
         typ = GL_UNSIGNED_SHORT;
         break;
      default: return RC::UNSUPPORTED_DEPTH;
      }
   }

   glReadPixels(0, 0, m_DisplayWidth, height, fmt, typ, pDest);
   glFinish();

   return mglGetRC();
}

//------------------------------------------------------------------------------
// I2CTest object, properties and methods.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(AppleGL, GpuTest, "Apple's QuadTextureTest..");

