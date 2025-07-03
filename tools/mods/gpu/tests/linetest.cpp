/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file linetest.cpp Implements the reads-passing-writes line test.

#include <vector>
#include "lwos.h"
#include "core/include/lwrm.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "random.h"
#include "core/include/utility.h"
#include "gpu/include/dmawrap.h"
#include "gpu/utility/gpuutils.h"
#include "gputest.h"
#include "gpu/include/gpudev.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gralloc.h"
#include "gpu/utility/surf2d.h"
#include "gpu/js/fpk_comm.h"
#include "class/cl502d.h" // LW50_TWOD
#include "class/cla06fsubch.h"
#include "core/include/platform.h"

using namespace std;

//------------------------------------------------------------------------------
//! \brief Line Test
//!
//! Draws alternating sequences of horizontal and vertical line strips to detect
//! out-of-order reads/writes (reads-passing-writes) due to incorrect SBIOS
//! configuration. The errors typically appear around the endpoints of
//! overlapping, connected line patterns rendered using read-modify-write ROPs.
class LineTest: public GpuTest
{
    // Channel constants
    enum {
        s_TwoD = LWA06F_SUBCHANNEL_2D
    };

    //! LW50_TWOD line primitive segment attributes (one coord to form strip)
    struct LineSegment
    {
        UINT32 color; // line segment color
        UINT32 rop;   // raster operation
        UINT32 beta1; // beta1 value
        UINT32 beta4; // beta4 value
        UINT32 op;    // rop / blend operation
        UINT32 x;     // line x coordinate
        UINT32 y;     // line y coordinate
    };

    //! Record some error information for reporting in verbose mode
    struct ErrorData
    {
        ErrorData(UINT32 _x, UINT32 _y, UINT32 _expected, UINT32 _actual)
        : x(_x), y(_y), expected(_expected), actual(_actual) {}

        UINT32 x;
        UINT32 y;
        UINT32 expected;
        UINT32 actual;
    };

    typedef LwRm::Handle Handle;
    typedef vector<LineSegment> SegmentList;
    typedef vector<ErrorData> ErrorList;

public:
    LineTest();
    virtual ~LineTest();

    virtual bool IsSupported();
    virtual bool IsSupportedVf();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC SetDefaultsForPicker(UINT32 index);

    // JavaScript property accessors
    SETGET_PROP(LineSegments, UINT32)
    SETGET_PROP(StopOnError,  bool);
    SETGET_PROP(AlwaysOnDev,  bool);    // debugging
    SETGET_PROP(FrameDelayMS, UINT32);  // debugging

private:
    RC   AllocateSurface(Surface2D *pSurface, bool inSystemMemory);
    RC   SetupGraphics();

    RC   PrepareSurface(const Surface2D &surface);
    RC   WaitOnNotifier(UINT32 subchannel);
    bool CompareSurfaces(Surface2D *pGoodSurf,
                         Surface2D *pTestSurf,
                         ErrorList *pErrors);

    RC   RenderLines(const SegmentList &segs);
    RC   RenderPolyLines(const SegmentList &segs);
    RC   SetAttributes(const LineSegment &attr);
    void PickAttributes(LineSegment *attr);
    void GenerateLines(LineSegment *segs, UINT32 count, bool horizontal);
    void DisplayVerboseInfo(const SegmentList &segs,
                            const ErrorList &errors,
                            bool horizontal);

    // JavaScript properties
    UINT32 m_LineSegments;
    bool   m_StopOnError;
    bool   m_AlwaysOnDev;
    UINT32 m_FrameDelayMS;

    // Internal properties
    Channel  *m_pCh;
    Handle    m_hCh;
    TwodAlloc m_TwoD;
    Notifier  m_Notifier;

    Surface2D m_DevSurf;
    Surface2D m_HostLine;
    Surface2D m_HostPoly;

    UINT32 m_MaximumX;
    UINT32 m_MaximumY;

    FancyPicker::FpContext *m_pFpCtx;
    Random                 *m_pRand;
};

//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LineTest, GpuTest,
                 "Line Test - Directed test for reads-passing-writes issue");

CLASS_PROP_READWRITE(LineTest, LineSegments, UINT32,
                     "Number of line segments to render for each iteration.");
CLASS_PROP_READWRITE(LineTest, StopOnError, bool,
                     "Stop test exelwtion upon encountering a mismatch.");
CLASS_PROP_READWRITE(LineTest, AlwaysOnDev,  bool,
                     "Always create the surface in device memory.");
CLASS_PROP_READWRITE(LineTest, FrameDelayMS, UINT32,
                     "Number of milliseconds to wait after each frame.");

//------------------------------------------------------------------------------
LineTest::LineTest()
    :m_LineSegments(100)
    ,m_StopOnError(true)
    ,m_AlwaysOnDev(false)
    ,m_FrameDelayMS(0)
    ,m_pCh(0)
    ,m_hCh(0)
    ,m_MaximumX(0)
    ,m_MaximumY(0)
{
    SetName("LineTest");
    CreateFancyPickerArray(FPK_LINETEST_NUM_PICKERS);

    m_pFpCtx   = GetFpContext();
    m_pRand    = &(m_pFpCtx->Rand);
}

//------------------------------------------------------------------------------
LineTest::~LineTest()
{
}

//------------------------------------------------------------------------------
bool LineTest::IsSupported()
{
    GpuDevice *pDev = GetBoundGpuDevice();

    return GpuTest::IsSupported() &&
           m_TwoD.IsSupported(pDev) &&
           pDev->SupportsRenderToSysMem();
}

//------------------------------------------------------------------------------
bool LineTest::IsSupportedVf()
{
    return !(GetBoundGpuSubdevice()->IsSMCEnabled());
}

//------------------------------------------------------------------------------
void LineTest::PrintJsProperties(Tee::Priority pri)
{
    static const char *BoolToStr[] = { "false", "true" };

    GpuTest::PrintJsProperties(pri);
    Printf(pri, "\tLineSegments:\t%d\n", m_LineSegments);
    Printf(pri, "\tStopOnError:\t%s\n",  BoolToStr[m_StopOnError ? 1 : 0]);
    Printf(pri, "\tAlwaysOnDev:\t%s\n",  BoolToStr[m_AlwaysOnDev ? 1 : 0]);
}

//------------------------------------------------------------------------------
RC LineTest::Setup()
{
    RC rc;
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());

    // Allocate channel and surfaces on both device and host side
    CHECK_RC(m_TestConfig.AllocateChannelWithEngine(&m_pCh,
                                                    &m_hCh,
                                                    &m_TwoD));

    bool renderInSysMem = false;
    if (!pSubdev->HasFeature(Device::GPUSUB_HAS_FRAMEBUFFER_IN_SYSMEM) &&
        !m_AlwaysOnDev)
    {
        Printf(Tee::PriLow,
               "Framebuffer not in sysmem, allocating surface in sysmem.\n");
        renderInSysMem = true;
    }

    CHECK_RC(AllocateSurface(&m_DevSurf, renderInSysMem));
    CHECK_RC(AllocateSurface(&m_HostLine, true));
    CHECK_RC(AllocateSurface(&m_HostPoly, true));

    if (m_DevSurf.GetDisplayable())
    {
        CHECK_RC(GetDisplayMgrTestContext()->DisplayImage(&m_DevSurf,
            DisplayMgr::DONT_WAIT_FOR_UPDATE));
    }

    // Allocate LW50_TWOD and notifier
    CHECK_RC(SetupGraphics());
    CHECK_RC(m_Notifier.Allocate(m_pCh, 10, &m_TestConfig));
    CHECK_RC(m_Notifier.Instantiate(s_TwoD));

    m_MaximumX = m_TestConfig.SurfaceWidth() - 1;
    m_MaximumY = m_TestConfig.SurfaceHeight() - 1;

    return OK;
}

//------------------------------------------------------------------------------
RC LineTest::Run()
{
    RC rc;
    bool mismatch = false;
    bool horizontal = false;
    FLOAT64 timeoutMs = m_TestConfig.TimeoutMs();
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    DmaWrapper DmaWrap(&m_TestConfig);

    SegmentList segments(m_LineSegments);
    ErrorList errors;

    UINT32 startLoop        = m_TestConfig.StartLoop();
    UINT32 endLoop          = startLoop + m_TestConfig.Loops();
    UINT32 restartSkipCount = m_TestConfig.RestartSkipCount();

    m_pRand->SeedRandom(m_TestConfig.Seed());

    for (m_pFpCtx->LoopNum = startLoop;
         m_pFpCtx->LoopNum < endLoop;
         m_pFpCtx->LoopNum++)
    {
        if ((m_pFpCtx->LoopNum == startLoop) ||
            (m_pFpCtx->LoopNum % restartSkipCount == 0))
        {
            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / restartSkipCount;
            GetFancyPickerArray()->Restart();
        }

        // Generate one set of data so that two identical images can be
        // generated: one using individual lines, one using polylines
        GenerateLines(&segments[0], m_LineSegments, horizontal);

        // Draw the first image as a series of polylines
        CHECK_RC(PrepareSurface(m_DevSurf));
        CHECK_RC(RenderPolyLines(segments));
        CHECK_RC(DmaWrap.SetSurfaces(&m_DevSurf, &m_HostPoly));
        CHECK_RC(DmaWrap.Copy(0,                    // Starting src X, in bytes not pixels.
                              0,                    // Starting src Y, in lines.
                              m_DevSurf.GetPitch(), // Width of copied rect, in bytes.
                              m_DevSurf.GetHeight(),// Height of copied rect, in bytes
                              0,                    // Starting dst X, in bytes not pixels.
                              0,                    // Starting dst Y, in lines.
                              timeoutMs,
                              pSubdev->GetSubdeviceInst()));

        // Draw the second image as a series of individual lines
        CHECK_RC(PrepareSurface(m_DevSurf));
        CHECK_RC(RenderLines(segments));
        CHECK_RC(DmaWrap.SetSurfaces(&m_DevSurf, &m_HostLine));
        CHECK_RC(DmaWrap.Copy(0,                    // Starting src X, in bytes not pixels.
                              0,                    // Starting src Y, in lines.
                              m_DevSurf.GetPitch(), // Width of copied rect, in bytes.
                              m_DevSurf.GetHeight(),// Height of copied rect, in bytes
                              0,                    // Starting dst X, in bytes not pixels.
                              0,                    // Starting dst Y, in lines.
                              timeoutMs,
                              pSubdev->GetSubdeviceInst()));

        if (!CompareSurfaces(&m_HostPoly, &m_HostLine, &errors))
        {
            mismatch = true;
            Printf(Tee::PriHigh,
                   "%ld mismatched pixels on iteration %ld (%s step).\n",
                   (long)errors.size(), (long)m_pFpCtx->LoopNum,
                   horizontal ? "horizontal" : "vertical");

            if (m_TestConfig.Verbose())
            {
                // Display all of the selected picker settings for each error
                DisplayVerboseInfo(segments, errors, horizontal);
            }

            if (m_StopOnError)
            {
                // Only halt the test prematurely if requested to
                break;
            }
        }

        // Clear out errors and alternate sweep direction between runs
        errors.clear();
        horizontal = !horizontal;

        if (0 != m_FrameDelayMS)
        {
            Tasker::Sleep(m_FrameDelayMS);
        }
    }

    if (mismatch)
    {
        Printf(Tee::PriHigh,
               "Surface miscompares were detected during test exelwtion.\n"
               "This may be due to a misconfigured SBIOS or memory failure.\n");
        return RC::BAD_MEMORY;
    }

    return OK;
}

//------------------------------------------------------------------------------
RC LineTest::Cleanup()
{
    StickyRC rc;

    if (GetDisplayMgrTestContext())
    {
        rc = GetDisplayMgrTestContext()->DisplayImage(
            static_cast<Surface2D *>(nullptr), DisplayMgr::WAIT_FOR_UPDATE);
    }
    m_Notifier.Free();
    m_HostPoly.Free();
    m_HostLine.Free();
    m_DevSurf.Free();
    rc = GpuTest::Cleanup();
    rc = m_TestConfig.FreeChannel(m_pCh);
    return rc;
}

RC LineTest::SetDefaultsForPicker(UINT32 index)
{
    MASSERT(index < FPK_LINETEST_NUM_PICKERS);

    FancyPicker &picker = (*GetFancyPickerArray())[index];

    switch (index)
    {
        case FPK_LINETEST_COLOR:
            // LW502D_SET_RENDER_SOLID_PRIM_COLOR
            picker.ConfigRandom();
            picker.AddRandRange(1, 0x00000000, 0xFFFFFFFF);
            picker.CompileRandom();
            break;

        case FPK_LINETEST_ROP:
            // LW502D_SET_ROP
            picker.ConfigRandom();
            picker.AddRandItem(1, 0x11); // DSon
            picker.AddRandItem(1, 0x22); // DSna
            picker.AddRandItem(1, 0x44); // SDna
            picker.AddRandItem(1, 0x66); // DSx
            picker.AddRandItem(1, 0x77); // DSan
            picker.AddRandItem(1, 0x88); // DSa
            picker.AddRandItem(1, 0x99); // DSxn
            picker.AddRandItem(1, 0xBB); // DSno
            picker.AddRandItem(1, 0xDD); // SDno
            picker.AddRandItem(1, 0xEE); // DSo
            picker.CompileRandom();
            break;

        case FPK_LINETEST_BETA1:
            // LW502D_SET_BETA1
            picker.ConfigRandom();
            picker.AddRandRange(1, 0x00000000, 0xFFFFFFFF);
            picker.CompileRandom();
            break;

        case FPK_LINETEST_BETA4:
            // LW502D_SET_BETA4
            picker.ConfigRandom();
            picker.AddRandRange(1, 0x00000000, 0xFFFFFFFF);
            picker.CompileRandom();
            break;

        case FPK_LINETEST_OP:
            // LW502D_SET_OPERATION
            picker.ConfigRandom();
            picker.AddRandItem(1, LW502D_SET_OPERATION_V_ROP);
            picker.AddRandItem(1, LW502D_SET_OPERATION_V_BLEND_AND);
            picker.AddRandItem(1, LW502D_SET_OPERATION_V_BLEND_PREMULT);

            // The following ops don't appear to reproduce the problem at all...
            // picker.AddRandItem(1, LW502D_SET_OPERATION_V_ROP_AND);
            // picker.AddRandItem(1, LW502D_SET_OPERATION_V_SRCCOPY);
            // picker.AddRandItem(1, LW502D_SET_OPERATION_V_SRCCOPY_AND);
            // picker.AddRandItem(1, LW502D_SET_OPERATION_V_SRCCOPY_PREMULT);
            picker.CompileRandom();
            break;

        case FPK_LINETEST_XPOS:
            // Line X coordinates
            picker.ConfigRandom();
            picker.AddRandRange(1, 0, m_MaximumX);
            picker.CompileRandom();
            break;

        case FPK_LINETEST_YPOS:
            // Line Y coordinates
            picker.ConfigRandom();
            picker.AddRandRange(1, 0, m_MaximumY);
            picker.CompileRandom();
            break;

        default:
            MASSERT(!"LineTest::SetDefaultsForPicker unhandled index");
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
RC LineTest::SetupGraphics()
{
    RC rc;

    UINT32 surf_width  = m_DevSurf.GetWidth();
    UINT32 surf_height = m_DevSurf.GetHeight();
    UINT32 surf_depth  = m_DevSurf.GetDepth();
    UINT32 surf_pitch  = m_DevSurf.GetPitch();

    // Sticking to using basic/safe formats for each given depth
    UINT32 src_format, dst_format, prim_format;
    switch (surf_depth)
    {
    case 8:
        src_format  = LW502D_SET_SRC_FORMAT_V_Y8;
        dst_format  = LW502D_SET_DST_FORMAT_V_Y8;
        prim_format = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y8;
        break;
    case 15:
        src_format  = LW502D_SET_SRC_FORMAT_V_A1R5G5B5;
        dst_format  = LW502D_SET_DST_FORMAT_V_A1R5G5B5;
        prim_format = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A1R5G5B5;
        break;
    case 16:
        src_format  = LW502D_SET_SRC_FORMAT_V_R5G6B5;
        dst_format  = LW502D_SET_DST_FORMAT_V_R5G6B5;
        prim_format = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_R5G6B5;
        break;
    default:
        // Everything else defaults to 32-bit ARGB
        src_format  = LW502D_SET_SRC_FORMAT_V_A8R8G8B8;
        dst_format  = LW502D_SET_DST_FORMAT_V_A8R8G8B8;
        prim_format = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8;
        break;
    }

    CHECK_RC(m_pCh->SetObject(s_TwoD, m_TwoD.GetHandle()));

    // Sticking with pitch, since block linear probably wouldn't gain us much
    CHECK_RC(m_pCh->Write(s_TwoD,
                          LW502D_SET_SRC_MEMORY_LAYOUT,
                          LW502D_SET_SRC_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(m_pCh->Write(s_TwoD,
                          LW502D_SET_DST_MEMORY_LAYOUT,
                          LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH));

    // Set identical dimensions and color formats for source and destination
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_SRC_PITCH, surf_pitch));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_DST_PITCH, surf_pitch));

    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_SRC_WIDTH, surf_width));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_DST_WIDTH, surf_width));

    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_SRC_HEIGHT, surf_height));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_DST_HEIGHT, surf_height));

    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_SRC_FORMAT, src_format));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_DST_FORMAT, dst_format));

    CHECK_RC(m_pCh->Write(s_TwoD,
                          LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT,
                          prim_format));

    CHECK_RC(m_pCh->Write(s_TwoD,
                          LW502D_SET_CLIP_ENABLE,
                          DRF_DEF(502D, _SET_CLIP_ENABLE, _V, _FALSE)));

    m_pCh->Flush();

    return OK;
}

//------------------------------------------------------------------------------
RC LineTest::AllocateSurface(Surface2D *pSurface, bool inSystemMemory)
{
    RC rc;
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    pSurface->SetWidth(m_TestConfig.SurfaceWidth());
    pSurface->SetHeight(m_TestConfig.SurfaceHeight());
    pSurface->SetColorFormat(ColorUtils::ColorDepthToFormat(
                             m_TestConfig.DisplayDepth()));
    pSurface->SetProtect(Memory::ReadWrite);
    pSurface->SetTiled(m_TestConfig.UseTiledSurface());
    pSurface->SetLayout(Surface2D::Pitch);
    pSurface->SetType(LWOS32_TYPE_IMAGE);

    if (pSubdev->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
    {
        pSurface->SetAddressModel(Memory::Paging);
    }

    if (inSystemMemory)
    {
        // Allocate surface in system memory
        pSurface->SetLocation(m_TestConfig.MemoryType());
    }
    else
    {
        // Allocate surface in device memory, allow mapping to the display
        pSurface->SetDisplayable(true);
        pSurface->SetLocation(m_TestConfig.DstLocation());
    }

    CHECK_RC(pSurface->Alloc(GetBoundGpuDevice()));
    CHECK_RC(pSurface->Map(pSubdev->GetSubdeviceInst()));
    CHECK_RC(pSurface->BindGpuChannel(m_hCh));

    return OK;
}

//------------------------------------------------------------------------------
RC LineTest::PrepareSurface(const Surface2D &surface)
{
    RC rc;
    Handle handle = surface.GetCtxDmaHandleGpu();
    UINT64 offset = surface.GetCtxDmaOffsetGpu();
    UINT32 width  = surface.GetWidth();
    UINT32 height = surface.GetHeight();

    UINT32 offset_lo = (UINT32)(offset);
    UINT32 offset_hi = (UINT32)(offset >> 32);

    // Set the context dma handle and offset
    if (LW50_TWOD == m_TwoD.GetClass())
    {
        CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_DST_CONTEXT_DMA, handle));
        CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_SRC_CONTEXT_DMA, handle));
    }
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_DST_OFFSET_UPPER, offset_hi));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_DST_OFFSET_LOWER, offset_lo));

    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_SRC_OFFSET_UPPER, offset_hi));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_SRC_OFFSET_LOWER, offset_lo));

    // Clear the surface to black
    m_pCh->Write(s_TwoD, LW502D_SET_RENDER_SOLID_PRIM_COLOR, 0);
    m_pCh->Write(s_TwoD, LW502D_SET_OPERATION, LW502D_SET_OPERATION_V_SRCCOPY);
    m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_MODE,
                 LW502D_RENDER_SOLID_PRIM_MODE_V_RECTS);

    m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0), 0);
    m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_Y(0), 0);

    m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0), width);
    m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_Y(0), height);

    CHECK_RC(m_pCh->Flush());
    return OK;
}

//------------------------------------------------------------------------------
void LineTest::GenerateLines(LineSegment *segs, UINT32 count, bool horizontal)
{
    if (horizontal)
    {
        float xstep = m_MaximumX / (float)m_LineSegments;
        float xlwr = 0.0f;

        for (UINT32 i = 0; i < count; i++)
        {
            PickAttributes(&segs[i]);

            // Add some horizontal jitter, but don't cross lines
            segs[i].x = (long)(xlwr + m_pRand->GetRandomFloat(0, xstep - 0.5f));
            xlwr += xstep;
        }
    }
    else
    {
        float ystep = m_MaximumY / (float)m_LineSegments;
        float ylwr = 0.0f;

        for (UINT32 i = 0; i < count; i++)
        {
            PickAttributes(&segs[i]);

            // Add some vertical jitter, but don't cross lines
            segs[i].y = (long)(ylwr + m_pRand->GetRandomFloat(0, ystep - 0.5f));
            ylwr += ystep;
        }
    }
}

//------------------------------------------------------------------------------
RC LineTest::RenderLines(const SegmentList &segs)
{
    RC rc;

    CHECK_RC(m_pCh->Write(s_TwoD,
                          LW502D_RENDER_SOLID_PRIM_MODE,
                          LW502D_RENDER_SOLID_PRIM_MODE_V_LINES));

    for (size_t i = 0; i < segs.size() - 1; ++i)
    {
        const LineSegment &s0 = segs[i], &s1 = segs[i + 1];

        // For individual lines, first set the attributes for the operation,
        // then send two sets of coordinates to define one line segment
        SetAttributes(s0);

        m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0), s0.x);
        m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_Y(0), s0.y);
        m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(1), s1.x);
        m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_Y(1), s1.y);

        // Wait on the notifier after every line specified. The single
        // line routine should (hopefully) never read memory out of order
        CHECK_RC(WaitOnNotifier(s_TwoD));
    }

    return OK;
}

//------------------------------------------------------------------------------
RC LineTest::RenderPolyLines(const SegmentList &segs)
{
    RC rc;

    CHECK_RC(m_pCh->Write(s_TwoD,
                          LW502D_RENDER_SOLID_PRIM_MODE,
                          LW502D_RENDER_SOLID_PRIM_MODE_V_POLYLINE));

    for (size_t i = 0; i < segs.size(); ++i)
    {
        const LineSegment &sn = segs[i];

        m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0), sn.x);
        m_pCh->Write(s_TwoD, LW502D_RENDER_SOLID_PRIM_POINT_Y(0), sn.y);

        // For polylines, set the attributes for the next line after sending
        // the current line's coordinate, since the first point isn't rendered
        SetAttributes(sn);
    }

    CHECK_RC(WaitOnNotifier(s_TwoD));
    return OK;
}

//------------------------------------------------------------------------------
RC LineTest::WaitOnNotifier(UINT32 subchannel)
{
    RC rc;

    m_Notifier.Clear(0);
    CHECK_RC(m_Notifier.Write(subchannel));
    CHECK_RC(m_pCh->Write(subchannel, LW502D_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_Notifier.Wait(0, m_TestConfig.TimeoutMs()));

    // On SOC, flush the GPU cache for golden values
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        CHECK_RC(m_pCh->WaitIdle(GetTestConfiguration()->TimeoutMs()));
        CHECK_RC(m_pCh->WriteL2FlushDirty());
        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_pCh->WaitIdle(GetTestConfiguration()->TimeoutMs()));
    }

    return OK;
}

//------------------------------------------------------------------------------
void LineTest::DisplayVerboseInfo
(
    const SegmentList &segments,
    const ErrorList &errors,
    bool horizontal
)
{
    static const char *OpTypeStr[] = {
        "SRCCOPY_AND",
        "ROP_AND",
        "BLEND_AND",
        "SRCCOPY",
        "ROP",
        "SRCCOPY_PREMULT",
        "BLEND_PREMULT"
    };

    unsigned long idx = 0;
    ErrorList::const_iterator iter;

    // Print out a list of all the mismatched pixels
    for (iter = errors.begin(); iter != errors.end(); ++iter)
    {
        Printf(Tee::PriHigh,
               "Mismatch %lu - (%u,%u) - read 0x%X - expected 0x%X\n",
               idx++, iter->x, iter->y, iter->actual, iter->expected);
    }

    idx = 0;

    // Print out the entire list of rendered line segments for lookup. It would
    // also be possible to just print out the two overlapping lines for each
    // error above, but if there are a lot of mismatches (multiple per line),
    // then that would likely result in a lot more duplicated information.
    for (size_t i = 0; i < segments.size() - 1; ++i)
    {
        const LineSegment &s0 = segments[i], &s1 = segments[i + 1];

        Printf(Tee::PriHigh,
               "Line %lu - {(%u,%u),(%u,%u)} - Color 0x%X - "
               "ROP 0x%X - Beta1 0x%X - Beta4 0x%X - OpType %s\n",
               idx++, s0.x, s0.y, s1.x, s1.y, s0.color,
               s0.rop, s0.beta1, s0.beta4, OpTypeStr[s0.op]);
    }
}

//------------------------------------------------------------------------------
void LineTest::PickAttributes(LineSegment *attr)
{
    MASSERT(attr != NULL);

    FancyPickerArray &pickers = *GetFancyPickerArray();

    attr->color = pickers[FPK_LINETEST_COLOR].Pick();
    attr->rop   = pickers[FPK_LINETEST_ROP].Pick();
    attr->beta1 = pickers[FPK_LINETEST_BETA1].Pick();
    attr->beta4 = pickers[FPK_LINETEST_BETA4].Pick();
    attr->op    = pickers[FPK_LINETEST_OP].Pick();
    attr->x     = pickers[FPK_LINETEST_XPOS].Pick();
    attr->y     = pickers[FPK_LINETEST_YPOS].Pick();
}

//------------------------------------------------------------------------------
RC LineTest::SetAttributes(const LineSegment &attr)
{
    RC rc;

    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_ROP, attr.rop));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_BETA1, attr.beta1));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_BETA4, attr.beta4));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_OPERATION, attr.op));
    CHECK_RC(m_pCh->Write(s_TwoD, LW502D_SET_RENDER_SOLID_PRIM_COLOR,
                          attr.color));

    CHECK_RC(m_pCh->Flush());
    return OK;
}

//------------------------------------------------------------------------------
bool LineTest::CompareSurfaces
(
    Surface2D *pGoodSurf,
    Surface2D *pTestSurf,
    ErrorList *pErrors
)
{
    UINT32 width   = pGoodSurf->GetWidth();
    UINT32 height  = pGoodSurf->GetHeight();
    UINT08 *addr_a = (UINT08 *)pGoodSurf->GetAddress();
    UINT08 *addr_b = (UINT08 *)pTestSurf->GetAddress();
    UINT32 pixel_a;
    UINT32 pixel_b;

    if ((width != pTestSurf->GetWidth()) || (height != pTestSurf->GetHeight()))
    {
        MASSERT(!"Unexpected surface dimension mismatch.");
        Printf(Tee::PriNormal, "Surface dimension mismatch.\n");
        return false;
    }

    // Loop through every pixel of the two surfaces and verify they match
    for (UINT32 y = 0; y < height; ++y)
    {
        for (UINT32 x = 0; x < width; ++x)
        {
            pixel_a = MEM_RD32(addr_a + pGoodSurf->GetPixelOffset(x, y));
            pixel_b = MEM_RD32(addr_b + pTestSurf->GetPixelOffset(x, y));

            // Conform to the hardware polling requirement requested by
            // "-poll_hw_hz" (i.e. HW should not be accessed faster than a
            // certain rate).  By default this will not sleep or yield unless
            // the command line argument is present.
            Tasker::PollMemSleep();

            if (pixel_a != pixel_b)
            {
                // Store error information to report later if verbose is enabled
                pErrors->push_back(ErrorData(x, y, pixel_a, pixel_b));
            }
        }
    }

    return (0 == pErrors->size());
}

