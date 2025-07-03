/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2010,2014,2016,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file    goldtest.cpp
 * @brief   Test of golden value functionality.
 */

#include "gpu/tests/gputest.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "lwos.h"
#include "core/include/fpicker.h"
#include "random.h"
#include "gpu/utility/surf2d.h"
#include "core/include/display.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/golden.h"
#include "core/include/xp.h"
#include "random.h"
#include "class/cl2080.h"
#include <vector>

class GoldelwalueTest : public GpuTest
{
public:
    GoldelwalueTest();
    virtual ~GoldelwalueTest();
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC TestSurfaceScribble();
    RC AllocSurfaces();
    void GetRandomOffsets(vector<UINT64> *offsets);

private:
    Goldelwalues         *m_pGolden;
    GpuTestConfiguration *m_pTestCfg;
    GpuGoldenSurfaces    *m_pGGSurfs;
    Random               *m_pRand;
    Surface2D             m_SysSurf;
    Surface2D            *m_pSurface;
    Channel              *m_pCh;
    LwRm::Handle          m_hCh;
    UINT32                m_FormatSize;

    // JavaScript properties
    UINT32 m_SurfaceFormat;
    UINT32 m_SurfaceWidth;
    UINT32 m_SurfaceHeight;
    UINT32 m_CorruptPixels;

public:
    SETGET_PROP(SurfaceFormat, UINT32);
    SETGET_PROP(SurfaceWidth,  UINT32);
    SETGET_PROP(SurfaceHeight, UINT32);
    SETGET_PROP(CorruptPixels, UINT32);
};

JS_CLASS_INHERIT(GoldelwalueTest, GpuTest, "Golden value regression test.");

CLASS_PROP_READWRITE(GoldelwalueTest, SurfaceWidth, UINT32,
                     "Width of golden surface under testing.");
CLASS_PROP_READWRITE(GoldelwalueTest, SurfaceHeight, UINT32,
                     "Height of golden surface under testing.");
CLASS_PROP_READWRITE(GoldelwalueTest, SurfaceFormat, UINT32,
                     "Color format of surface under testing.");
CLASS_PROP_READWRITE(GoldelwalueTest, CorruptPixels, UINT32,
                     "Number of corrupt pixels to scribble on surface.");

//-----------------------------------------------------------------------------
GoldelwalueTest::GoldelwalueTest()
: m_pGGSurfs(nullptr),
  m_pSurface(nullptr),
  m_pCh(nullptr),
  m_hCh(0),
  m_FormatSize(0),
  m_SurfaceFormat((UINT32)ColorUtils::A8R8G8B8),
  m_SurfaceWidth(128),
  m_SurfaceHeight(128),
  m_CorruptPixels(0)
{
    SetName("GoldelwalueTest");

    m_pTestCfg = GetTestConfiguration();
    m_pGolden  = GetGoldelwalues();
    m_pRand    = &(GetFpContext()->Rand);
}

//-----------------------------------------------------------------------------
/* virtual */
GoldelwalueTest::~GoldelwalueTest()
{
}

//-----------------------------------------------------------------------------
/* virtual */
bool GoldelwalueTest::IsSupported()
{
    return true;
}

//-----------------------------------------------------------------------------
/* virtual */
RC GoldelwalueTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    m_pRand->SeedRandom(m_pTestCfg->Seed());

    CHECK_RC(m_pTestCfg->AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GRAPHICS));

    // Allocate and attach GPU golden surfaces
    CHECK_RC(AllocSurfaces());

    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    m_pGGSurfs->AttachSurface(m_pSurface, "C", GetDisplay()->Selected());

    CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */
RC GoldelwalueTest::Run()
{
    RC rc;

    // Optionally test corrupting and restoring random pixels
    if (m_CorruptPixels != 0)
    {
        CHECK_RC(TestSurfaceScribble());
    }

    // Always run here to either generate good image or verify we restored
    // everything correctly after running TestSurfaceScribble
    m_pGolden->SetLoop(0);
    CHECK_RC(m_pGolden->Run());

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */
RC GoldelwalueTest::Cleanup()
{
    StickyRC firstRc;

    if (m_pGGSurfs)
    {
        delete m_pGGSurfs;
        m_pGGSurfs = NULL;
    }

    m_pGolden->ClearSurfaces();
    m_SysSurf.Free();

    if (m_pCh)
    {
        firstRc = m_pTestCfg->FreeChannel(m_pCh);
        m_pCh = NULL;
    }

    firstRc = GpuTest::Cleanup();
    return firstRc;
}

//-----------------------------------------------------------------------------
RC GoldelwalueTest::TestSurfaceScribble()
{
    RC rc;
    if (m_pSurface->GetAddress() == nullptr)
    {
        return RC::SOFTWARE_ERROR;
    }

    vector<UINT64> offsets;
    GetRandomOffsets(&offsets);

    for (UINT32 i = 0; i < offsets.size(); ++i)
    {
        // map & write memory directly because Surface2D doesn't like certain
        // color depths, although we only care about modifying a single byte
        UINT08 *address = (UINT08 *)m_pSurface->GetAddress() + offsets[i];

        UINT08 color = MEM_RD08(address);

        // First corrupt the given pixel, verify test fails
        MEM_WR08(address, ~color);
        m_pGolden->SetLoop(0);

        if (OK == m_pGolden->Run())
        {
            Printf(Tee::PriHigh,
                   "Failed test at surface offset = %llX, address = %p\n"
                   "Corrupted pixel, but golden values matched.\n",
                   offsets[i], address);

            return RC::SOFTWARE_ERROR;
        }

        // Reset the pixel to good color, verify we still pass
        MEM_WR08(address, color);
        m_pGolden->SetLoop(0);

        rc = m_pGolden->Run();
        if (OK != rc)
        {
            Printf(Tee::PriHigh,
                   "Failed test at surface offset = %llX, address = %p\n"
                   "Restored corrupted pixel, but still found mismatch.\n",
                   offsets[i], address);

            return rc;
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC GoldelwalueTest::AllocSurfaces()
{
    RC rc;

    // Determine the surface format and pixel size in bytes
    m_FormatSize = ColorUtils::PixelBytes((ColorUtils::Format)m_SurfaceFormat);

    // Allocate the surface in system memory
    m_SysSurf.SetWidth(m_SurfaceWidth);
    m_SysSurf.SetHeight(m_SurfaceHeight);
    m_SysSurf.SetColorFormat((ColorUtils::Format)m_SurfaceFormat);
    m_SysSurf.SetLocation(Memory::Coherent);
    m_SysSurf.SetProtect(Memory::ReadWrite);
    m_SysSurf.SetLayout(Surface2D::Pitch);
    m_SysSurf.SetDisplayable(false);
    m_SysSurf.SetType(LWOS32_TYPE_IMAGE);
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_PAGING))
        m_SysSurf.SetAddressModel(Memory::Paging);

    CHECK_RC(m_SysSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SysSurf.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_SysSurf.BindGpuChannel(m_hCh));

    m_pSurface = &m_SysSurf;

    // Fill the surface with random data
    for (UINT32 y = 0; y < m_SurfaceHeight; ++y)
    {
        for (UINT32 x = 0; x < m_SurfaceWidth; ++x)
        {
            UINT64 offset = m_pSurface->GetPixelOffset(x, y);

            // fill each component with a random byte
            for (UINT32 comp = 0; comp < m_FormatSize; ++comp)
            {
                UINT08 *address = (UINT08 *)m_pSurface->GetAddress()
                                                + offset + comp;
                MEM_WR08(address, (UINT08)m_pRand->GetRandom(0, 0xFF));
            }
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
void GoldelwalueTest::GetRandomOffsets(vector<UINT64> *offsets)
{
    MASSERT(offsets != NULL);

    offsets->clear();
    offsets->push_back(m_pSurface->GetPixelOffset(0, 0));
    offsets->push_back(m_FormatSize - 1 +
                       m_pSurface->GetPixelOffset(m_SurfaceWidth - 1,
                                                  m_SurfaceHeight - 1));

    UINT32 randomPixels = (m_CorruptPixels > 3) ? (m_CorruptPixels - 3) : 0;

    for (UINT32 i = 0; i < randomPixels; ++i)
    {
        // Callwlate a random offset from (x, y) pair
        UINT64 random_offset = m_pSurface->GetPixelOffset(
                m_pRand->GetRandom(0, m_SurfaceWidth - 1),   // X position
                m_pRand->GetRandom(0, m_SurfaceHeight - 1)); // Y position

        // Callwlate a random byte within the given pixel
        random_offset += m_pRand->GetRandom(0, m_FormatSize - 1);

        offsets->push_back(random_offset);
    }
}

