/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/channel.h"
#include "gpu/include/dispchan.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gputest.h"
#include "gpu/utility/surf2d.h"
#include "core/include/platform.h"
#include "ctrl/ctrl0073.h"
#include "gpu/display/evo_disp.h"

/* This test uses writeback channel to write surface content
   into WB surface allocated  on FB.
*/
class Wbor: public GpuTest
{
public:
    Wbor();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual bool IsSupported();

private:
    UINT32 m_Width;
    UINT32 m_Height;
    UINT32 m_Depth;
    UINT32 m_RefreshRate;
    UINT32 m_DetectedWbors;
    GpuTestConfiguration *m_pTstCfg;
    vector<Surface2D> m_BaseSurfaces;
    Surface2D m_WborSurface;
    Surface2D m_SysSurface;
    vector<Surface2D> m_FDSurfaces;
    DmaWrapper m_DmaWrap;
    EvoRasterSettings m_ers;
    Display *m_pDisplay;
    UINT32 m_OriginalSelectedDisplay;
    Goldelwalues *          m_pGolden;
    GpuGoldenSurfaces *     m_pGGSurfs;
    RC InitResources();
    void FreeResources();
    RC GetDetectedWBORs();

    enum SurfacePattern
    {
        HR_GRAD
       ,HC_LUMA25
       ,RND
       ,NUM_PTRN
    };
};

Wbor::Wbor():
     m_Width(0)
    ,m_Height(0)
    ,m_Depth(0)
    ,m_RefreshRate(0)
    ,m_DetectedWbors(0)
    ,m_pTstCfg(GetTestConfiguration())
    ,m_pDisplay(nullptr)
    ,m_OriginalSelectedDisplay(0)
    ,m_pGGSurfs(NULL)
{
    SetName("Wbor");
    m_pGolden = GetGoldelwalues();
    m_BaseSurfaces.resize(NUM_PTRN);
}

bool Wbor::IsSupported()
{
    RC rc;
    if (GetDisplay()->GetDisplayClassFamily() != Display::EVO)
    {
        Printf(Tee::PriLow, "Test is supported only with EVO display\n");
        return false;
    }

    rc = GetDetectedWBORs();
    // To raise error during test run instead of silent skip
    if (rc != OK)
        return true;

    if (!m_DetectedWbors)
    {
        Printf (Tee::PriLow, "Test is not supported when there are no WBOR entries in DCB\n");
        return false;
    }
    return true;
}

RC Wbor::GetDetectedWBORs()
{
    if (m_DetectedWbors)
        return OK;

    RC rc;
    Display *pDisplay = GetDisplay();
    UINT32 connectorsToCheck;
    CHECK_RC(pDisplay->GetPrimaryConnectors(&connectorsToCheck));

    while (connectorsToCheck)
    {
        UINT32 connectorToCheck = connectorsToCheck & ~(connectorsToCheck - 1);
        connectorsToCheck ^= connectorToCheck;

        LW0073_CTRL_SPECIFIC_OR_GET_INFO_PARAMS orGetInfoParams = {0};
        orGetInfoParams.displayId = connectorToCheck;
        CHECK_RC(pDisplay->RmControl(
                    LW0073_CTRL_CMD_SPECIFIC_OR_GET_INFO,
                    &orGetInfoParams,
                    sizeof(orGetInfoParams)
                    ));

        switch (orGetInfoParams.type)
        {
            case LW0073_CTRL_SPECIFIC_OR_TYPE_WBOR:
                m_DetectedWbors |= connectorToCheck;
                break;
            default:
                break;
        }
    }
    return rc;
}

RC Wbor::InitResources()
{
    RC rc;
    UINT32 subdeviceInst = GetBoundGpuSubdevice()->GetSubdeviceInst();
    m_Width       = m_pTstCfg->SurfaceWidth();
    m_Height      = m_pTstCfg->SurfaceHeight();
    m_Depth       = m_pTstCfg->DisplayDepth();
    m_RefreshRate = m_pTstCfg->RefreshRate();

    m_DmaWrap = DmaWrapper();
    m_DmaWrap.Initialize(m_pTstCfg, m_pTstCfg->NotifierLocation());

    m_SysSurface.SetWidth(m_Width);
    m_SysSurface.SetHeight(m_Height);
    m_SysSurface.SetColorFormat(ColorUtils::A8R8G8B8);
    m_SysSurface.SetType(LWOS32_TYPE_IMAGE);
    m_SysSurface.SetLocation(Memory::NonCoherent);
    m_SysSurface.SetLayout(Surface2D::Pitch);
    CHECK_RC(m_SysSurface.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SysSurface.Map());

    for (UINT32 idx = 0; idx < m_BaseSurfaces.size(); idx++)
    {
        // Allocate the base image surface that will be written to WB surface
        m_BaseSurfaces[idx].SetWidth(m_Width);
        m_BaseSurfaces[idx].SetHeight(m_Height);
        m_BaseSurfaces[idx].SetColorFormat(ColorUtils::A8R8G8B8);
        m_BaseSurfaces[idx].SetLayout(Surface2D::Pitch);
        m_BaseSurfaces[idx].SetType(LWOS32_TYPE_IMAGE);
        m_BaseSurfaces[idx].SetDisplayable(true);
        m_BaseSurfaces[idx].SetLocation(Memory::Fb);
        CHECK_RC(m_BaseSurfaces[idx].Alloc(GetBoundGpuDevice()));
        switch (idx)
        {
            case HR_GRAD:
                CHECK_RC(m_SysSurface.FillPatternRect(
                            0,
                            0,
                            m_Width,
                            m_Height,
                            1,
                            1,
                            "gradient",
                            "mfgdiag",
                            "horizontal",
                            subdeviceInst));
                break;
            case HC_LUMA25:
                CHECK_RC(m_SysSurface.FillPatternRect(
                            0,
                            0,
                            m_Width,
                            m_Height,
                            1,
                            1,
                            "hue_circle",
                            "luma25",
                            "",
                            subdeviceInst));
                break;
            case RND:
                {
                    string seed = Utility::StrPrintf("seed=%u", m_pTstCfg->Seed());
                    CHECK_RC(m_SysSurface.FillPatternRect(
                                0,
                                0,
                                m_Width,
                                m_Height,
                                1,
                                1,
                                "random",
                                seed.c_str(),
                                "",
                                subdeviceInst));
                }
                break;
            default:
                return RC::SOFTWARE_ERROR;
        }
        m_DmaWrap.SetSurfaces(&m_SysSurface, &m_BaseSurfaces[idx]);
        m_DmaWrap.Copy(0, 0, m_SysSurface.GetPitch(), m_SysSurface.GetHeight(),
                0, 0, m_pTstCfg->TimeoutMs(), 0);
    }

    /*
       Frame Descriptor surface is required to poll for the completion of
       surface written back by writeback channel(i.e, on m_WborSurface).
       This surface will be written by HW (we are mainly interested in
       LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_STATUS_11_TYPE_DONE status).
       We need to provide the separate FD surface for each base image
       we want to set on WB surface because it only flushes out a new descriptor
       when context dma offset gets changed. Size of this surface:
       LW_DISP_WRITE_BACK_FRAME_DESCRIPTOR_1_SIZEOF = 0x30
    */
    const UINT32 fdSize = 0x30;
    UINT08 *surfAddr;
    // WB need new descriptor per base image surface per loop
    m_FDSurfaces.resize(NUM_PTRN * m_pTstCfg->Loops());
    for (UINT32 idx = 0; idx < m_FDSurfaces.size(); idx++)
    {
        m_FDSurfaces[idx].SetWidth(fdSize);
        m_FDSurfaces[idx].SetPitch(fdSize);
        m_FDSurfaces[idx].SetHeight(1);
        m_FDSurfaces[idx].SetColorFormat(ColorUtils::Y8);
        m_FDSurfaces[idx].SetLocation(Memory::Coherent);
        CHECK_RC(m_FDSurfaces[idx].Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_FDSurfaces[idx].Map());
        surfAddr = static_cast<UINT08*>(m_FDSurfaces[idx].GetAddress());
        Platform::MemSet(surfAddr, 0, m_FDSurfaces[idx].GetSize());
    }

    // Adjust blank/sync periods to meet the evo raster settings
    m_ers.RasterWidth  = m_Width + 10;
    m_ers.ActiveX      = m_Width;
    m_ers.SyncEndX     = 1;
    m_ers.BlankStartX  = m_Width + 1;
    m_ers.BlankEndX    = m_ers.SyncEndX;
    m_ers.PolarityX    = false;
    m_ers.RasterHeight = m_Height + 10;
    m_ers.ActiveY      = m_Height;
    m_ers.SyncEndY     = 1;
    m_ers.BlankStartY  = m_Height + 1;
    m_ers.BlankEndY    = m_ers.SyncEndY;
    m_ers.PolarityY    = false;
    m_ers.Interlaced   = false;
    m_ers.PixelClockHz = m_ers.RasterWidth * m_ers.RasterHeight * m_RefreshRate;

    // Allocate the write back surface.
    m_WborSurface.SetWidth(m_Width);
    m_WborSurface.SetHeight(m_Height);
    m_WborSurface.SetColorFormat(ColorUtils::A8R8G8B8);
    m_WborSurface.SetType(LWOS32_TYPE_IMAGE);
    m_WborSurface.SetLocation(Memory::Fb);
    m_WborSurface.SetLayout(Surface2D::Pitch);
    m_WborSurface.SetDisplayable(true);
    CHECK_RC(m_WborSurface.Alloc(GetBoundGpuDevice()));

    surfAddr = static_cast<UINT08*>(m_SysSurface.GetAddress());
    Platform::MemSet(surfAddr, 0, m_SysSurface.GetSize());

    m_DmaWrap.SetSurfaces(&m_SysSurface, &m_WborSurface);
    m_DmaWrap.Copy(0, 0, m_SysSurface.GetPitch(), m_SysSurface.GetHeight(),
            0, 0, m_pTstCfg->TimeoutMs(), 0);
    m_pDisplay->AttachWBSurface(&m_WborSurface);

    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    return OK;
}

RC Wbor::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    CHECK_RC(GpuTest::Setup());
    // Reserve a Display object, disable UI, set default graphics mode.
    CHECK_RC(GpuTest::AllocDisplay());
    m_pDisplay = GetDisplay();
    m_OriginalSelectedDisplay = m_pDisplay->Selected();

    CHECK_RC(GetDetectedWBORs());
    // Initialize important surfaces here
    CHECK_RC(InitResources());
    return rc;
}

RC Wbor::Cleanup()
{
    StickyRC rc;
    // This was required in order to detach head from writeback OR
    rc = m_pDisplay->DetachAllDisplays();
    // free allocated surfaces
    FreeResources();

    rc = m_pDisplay->Select(m_OriginalSelectedDisplay);
    m_pDisplay->SetTimings(NULL);
    m_pDisplay = nullptr;

    rc = GpuTest::Cleanup();
    return rc;
}

void Wbor::FreeResources()
{
    m_pGolden->ClearSurfaces();
    delete m_pGGSurfs;
    m_pGGSurfs = NULL;
    m_WborSurface.Free();
    m_SysSurface.Free();

    for (UINT32 idx = 0; idx < m_FDSurfaces.size(); ++idx)
        m_FDSurfaces[idx].Free();

    for (UINT32 idx = 0; idx < m_BaseSurfaces.size(); ++idx)
        m_BaseSurfaces[idx].Free();

    m_DmaWrap.Cleanup();
}

RC Wbor::Run()
{
    RC rc;
    Printf(Tee::PriLow, "m_DetectedWbors = 0x%x\n", m_DetectedWbors);

    // Lwrrently testing only single wbor connector
    UINT32 wborDispId = m_DetectedWbors & ~(m_DetectedWbors - 1);

    const UINT32 minPixelClock = 50000000;
    Display::MaxPclkQueryInput maxPclkQuery = {wborDispId,
                                               &m_ers};
    vector<Display::MaxPclkQueryInput> maxPclkQueryVector(1, maxPclkQuery);
    CHECK_RC(m_pDisplay->SetRasterForMaxPclkPossible(&maxPclkQueryVector, minPixelClock));

    CHECK_RC(m_pDisplay->SetTimings(&m_ers));
    CHECK_RC(m_pDisplay->Select(wborDispId));
    m_pGGSurfs->AttachSurface(&m_WborSurface, "C", wborDispId);
    CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));

    CHECK_RC(m_pDisplay->SetMode(m_Width, m_Height, m_Depth, m_RefreshRate));

    UINT32 numLoops = m_pTstCfg->Loops();
    if (m_pGolden->GetAction() == Goldelwalues::Store)
        numLoops = 1;

    for (UINT32 loopIdx = 0; loopIdx < numLoops; ++loopIdx)
    {
        for (UINT32 idx = 0; idx < m_BaseSurfaces.size(); ++idx)
        {
            m_pGolden->SetLoop(idx);
            CHECK_RC(m_pDisplay->SetImage(&m_BaseSurfaces[idx]));
            // Write the base surface image to WB surface
            CHECK_RC(m_pDisplay->PollAndGetWBFrame(wborDispId,
                        &m_FDSurfaces[loopIdx * numLoops + idx]));
            CHECK_RC(m_pGolden->Run());
        }
    }
    return rc;
}

JS_CLASS_INHERIT(Wbor, GpuTest, "Wbor test.");
