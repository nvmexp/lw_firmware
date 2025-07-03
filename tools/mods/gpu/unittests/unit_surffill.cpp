/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/unittests/unittest.h"
#include "gpu/include/gpumgr.h"
#include "memory.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "random.h"
#include "gpu/utility/surffill.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/utility.h"

#include "ctrl/ctrl0080.h"
#include "core/include/fileholder.h"
#include "core/include/lwrm.h"

namespace
{
    //----------------------------------------------------------------
    //! \brief Unit tests for the ISurfaceFiller classes
    //!
    class SurfaceFillerTest: public UnitTest
    {
    public:
        SurfaceFillerTest();
        virtual void Run();
    private:
        enum FillerClass
        {
            MAPPING_SURFACE_FILLER,
            COPY_ENGINE_SURFACE_FILLER,
            THREE_D_SURFACE_FILLER,
            NUM_FILLER_CLASSES
        };
        void TryToRunOneTest(GpuDevice *pGpuDevice, FillerClass fillerClass);
        void RunSurfaceFillerTest(SurfaceUtils::ISurfaceFiller *pFiller,
                                  Surface2D *pSurface, UINT64 fillValue,
                                  UINT32 bpp, const string& testInfo);
        Random m_Random;
        UINT32 m_Seed;
        UINT32 m_SurfaceFillerTestCount;
        Surface2D m_AuxSysmemSurface;
    };
    ADD_UNIT_TEST(SurfaceFillerTest);
}

//--------------------------------------------------------------------
SurfaceFillerTest::SurfaceFillerTest() :
    UnitTest("SurfaceFillerTest"),
    m_SurfaceFillerTestCount(0)

{
    m_Seed = static_cast<UINT32>(Xp::GetWallTimeUS());
    m_Random.SeedRandom(m_Seed);
}

//--------------------------------------------------------------------
//! \brief Run tests on each GpuDevice in the system
//!
/* virtual */ void SurfaceFillerTest::Run()
{
    static constexpr UINT32 NUM_TESTS_PER_CLASS = 30;
    static constexpr UINT32 MAX_ATTEMPTS_PER_CLASS = 1000;

    GpuDevMgr *pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);

    if (pGpuDevMgr == nullptr)
    {
        Printf(Tee::PriNormal, "GPU not initialized, skipping test\n");
        return;
    }

    for (GpuDevice *pGpuDevice = pGpuDevMgr->GetFirstGpuDevice();
         pGpuDevice != nullptr;
         pGpuDevice = pGpuDevMgr->GetNextGpuDevice(pGpuDevice))
    {
        for (UINT32 fillerClass = 0;
             fillerClass < NUM_FILLER_CLASSES; ++fillerClass)
        {
            UINT32 numAttempts = 0;
            m_SurfaceFillerTestCount = 0;
            while (numAttempts < MAX_ATTEMPTS_PER_CLASS &&
                   m_SurfaceFillerTestCount < NUM_TESTS_PER_CLASS)
            {
                TryToRunOneTest(pGpuDevice,
                                static_cast<FillerClass>(fillerClass));
                ++numAttempts;
            }
        }
    }
}

//--------------------------------------------------------------------
//! Create a random surface on a GpuDevice, and try to fill it with a
//! random value with the indicated SurfaceFiller subclass.
//!
//! Note that this test doesn't guarantee that the randomly-generated
//! surface will be supported by the SurfaceFiller subclass we want to test.  The 3D
//! engine, in particular, has some very quirky requirements.  Rather
//! than trying to coordinate this test with all those requirements,
//! we call IsSupported() to see whether the subclass supports the
//! surface, and use a m_SurfaceFillerTestCount counter to keep track
//! of how many surfaces we were able to test.
//!
void SurfaceFillerTest::TryToRunOneTest
(
    GpuDevice *pGpuDevice,
    FillerClass fillerClass
)
{
    using namespace SurfaceUtils;
    static constexpr UINT32 MAX_DIMENSION = 0x100;
    RC rc;

    Surface2D surface;
    surface.SetWidth(m_Random.GetRandom(1, MAX_DIMENSION));
    surface.SetHeight(m_Random.GetRandom(1, MAX_DIMENSION));

    static Random::PickItem layoutPicker[] =
    {
        PI_ONE(1, Surface2D::Pitch),
        PI_ONE(1, Surface2D::BlockLinear),
    };
    STATIC_PI_PREP(layoutPicker);
    const auto layout = static_cast<Surface2D::Layout>(m_Random.Pick(layoutPicker));
    surface.SetLayout(layout);

    static Random::PickItem compressedPicker[] =
    {
        PI_ONE(1, false),
        PI_ONE(1, true),
    };
    STATIC_PI_PREP(compressedPicker);
    const bool isCompressed = (layout == Surface2D::BlockLinear)
                              ? !!m_Random.Pick(compressedPicker) : false;
    surface.SetCompressed(isCompressed);

    static Random::PickItem locationPicker[] =
    {
        PI_ONE(1, Memory::Fb),
        PI_ONE(1, Memory::NonCoherent),
        PI_ONE(1, Memory::Coherent),
    };
    STATIC_PI_PREP(locationPicker);
    surface.SetLocation(static_cast<Memory::Location>(
        isCompressed ? Memory::Fb : m_Random.Pick(locationPicker)));

    static Random::PickItem colorPicker[] =
    {
        PI_ONE(1, ColorUtils::R5G6B5),
        PI_ONE(1, ColorUtils::A8R8G8B8),
        PI_ONE(1, ColorUtils::A2R10G10B10),
        PI_ONE(1, ColorUtils::Y8),
        PI_ONE(1, ColorUtils::A2B10G10R10),
        PI_ONE(1, ColorUtils::A8B8G8R8),
        PI_ONE(1, ColorUtils::A1R5G5B5),
        PI_ONE(1, ColorUtils::R16_G16_B16_A16),
        PI_ONE(1, ColorUtils::Z24),
        PI_ONE(1, ColorUtils::R5G6B5),
        PI_ONE(1, ColorUtils::B8_G8_R8),
    };
    STATIC_PI_PREP(colorPicker);
    const ColorUtils::Format colorFormat = static_cast<ColorUtils::Format>(
            m_Random.Pick(colorPicker));
    surface.SetColorFormat(colorFormat);

    const UINT32 bpp = ColorUtils::PixelBits(colorFormat);

    if (surface.GetLayout() != Surface2D::BlockLinear)
    {
        surface.SetHiddenAllocSize(m_Random.GetRandom(0, MAX_DIMENSION));
        surface.SetExtraAllocSize(m_Random.GetRandom(0, MAX_DIMENSION));
    }
    static Random::PickItem attrPicker[] =
    {
        PI_ONE(1, DRF_DEF(OS32, _ATTR, _COMPR, _ANY)),
        PI_ONE(40, _UINT32_MAX),
    };
    STATIC_PI_PREP(attrPicker);
    UINT32 attrPick = m_Random.Pick(attrPicker);
    if (attrPick != _UINT32_MAX)
        surface.SetDefaultVidHeapAttr(attrPick);

    static Random::PickItem attr2Picker[] =
    {
        PI_ONE(1, DRF_DEF(OS32, _ATTR2, _ZBC, _PREFER_NO_ZBC)),
        PI_ONE(10, _UINT32_MAX),
    };
    STATIC_PI_PREP(attr2Picker);
    UINT32 attr2Pick = m_Random.Pick(attr2Picker);
    if (attr2Pick != _UINT32_MAX)
        surface.SetDefaultVidHeapAttr2(attr2Pick);

    UNIT_ASSERT_RC(surface.Alloc(pGpuDevice));
    DEFER { surface.Free(); };

    // Allocate auxiliary surface which we will use to read the contents of the
    // filled surface in certain cases.
    // * Avoid reflected mappings (needed by BlockLinear or compressed surfaces).
    const bool needAux = surface.GetLayout() != Surface2D::Pitch ||
                         isCompressed;
    if (needAux)
    {
        m_AuxSysmemSurface.SetWidth(surface.GetWidth());
        m_AuxSysmemSurface.SetHeight(surface.GetHeight());
        m_AuxSysmemSurface.SetLocation(Memory::Coherent);
        m_AuxSysmemSurface.SetColorFormat(surface.GetColorFormat());
        UNIT_ASSERT_RC(m_AuxSysmemSurface.Alloc(pGpuDevice));
    }
    DEFER { if (needAux) m_AuxSysmemSurface.Free(); };

    const UINT64 fillValue = m_Random.GetRandom64();

    const string testInfo = Utility::StrPrintf(
               "%s %ux%u %s %s %s %s attr=0x%08x "
               "attr2=0x%08x fill=0x%08llx %ubpp aux=%s",
               fillerClass == MAPPING_SURFACE_FILLER ? "MappingSurfaceFiller" :
               fillerClass == COPY_ENGINE_SURFACE_FILLER ? "CopyEngineSurfaceFiller" :
                                                           "ThreeDSurfaceFiller",
               surface.GetWidth(), surface.GetHeight(),
               ColorUtils::FormatToString(surface.GetColorFormat()).c_str(),
               (surface.GetLocation() == Memory::Fb ? "Fb" :
                surface.GetLocation() == Memory::Coherent ? "Coherent" :
                surface.GetLocation() == Memory::NonCoherent ? "NonCoherent" :
                "UnexpectedLocation"),
               (surface.GetLayout() == Surface2D::BlockLinear ? "BlockLinear" :
                surface.GetLayout() == Surface2D::Pitch ? "Pitch" :
                "UnexpectedLayout"),
               isCompressed ? "compressed" : "uncompressed",
               attrPick, attr2Pick,
               fillValue, bpp, needAux ? "true" : "false");

    const UINT64 expectedFillSize = surface.GetArrayPitch();

    switch (fillerClass)
    {
        case MAPPING_SURFACE_FILLER:
            if (MappingSurfaceFiller::IsSupported(surface, fillValue, bpp,
                                                  0, expectedFillSize))
            {
                Printf(Tee::PriLow, "Testing %s\n", testInfo.c_str());
                MappingSurfaceFiller filler(pGpuDevice);
                RunSurfaceFillerTest(&filler, &surface, fillValue, bpp, testInfo);
            }
            break;
        case COPY_ENGINE_SURFACE_FILLER:
            if (CopyEngineSurfaceFiller::IsSupported(surface, fillValue, bpp,
                                                  0, expectedFillSize))
            {
                Printf(Tee::PriLow, "Testing %s\n", testInfo.c_str());
                CopyEngineSurfaceFiller filler(pGpuDevice);
                RunSurfaceFillerTest(&filler, &surface, fillValue, bpp, testInfo);
            }
            break;
        case THREE_D_SURFACE_FILLER:
            if (ThreeDSurfaceFiller::IsSupported(surface, fillValue, bpp,
                                                  0, expectedFillSize))
            {
                Printf(Tee::PriLow, "Testing %s\n", testInfo.c_str());
                ThreeDSurfaceFiller filler(pGpuDevice);
                RunSurfaceFillerTest(&filler, &surface, fillValue, bpp, testInfo);
            }
            break;
        default:
            MASSERT(!"Illegal fillerClass");
    }
}

//--------------------------------------------------------------------
//! \brief Test a SurfaceFiller on a surface on all subdevices.
//!
//! This test uses the supplied SurfaceFiller to fill the surface with
//! a random value, then use ISurfaceReader to read the data back out,
//! and compares the results.
//!
void SurfaceFillerTest::RunSurfaceFillerTest
(
    SurfaceUtils::ISurfaceFiller *pFiller,
    Surface2D *pSurface,
    UINT64 fillValue,
    UINT32 bpp,
    const string& testInfo
)
{
    static constexpr UINT32 MAX_FAILURE_DUMPS = 10;

    Tasker::DetachThread detach;

    MASSERT(pFiller);
    MASSERT(pSurface);
    GpuDevice *pGpuDevice = pSurface->GetGpuDev();

    const UINT32 subdevInst = m_Random.GetRandom(0, pGpuDevice->GetNumSubdevices() - 1);
    const GpuSubdevice *pRandomSubdevice = pGpuDevice->GetSubdevice(subdevInst);
    RC rc;

    pFiller->SetTargetSubdevice(pRandomSubdevice);
    UNIT_ASSERT_RC(pFiller->Fill(pSurface, fillValue, bpp));

    Surface2D* const pFinalSurface = m_AuxSysmemSurface.IsAllocated()
                                     ? &m_AuxSysmemSurface : pSurface;
    vector<UINT08> dstData(pFinalSurface->GetArrayPitch());

    // Copy surface to sysmem if needed, where we can read it from
    const UINT32 pixelBytes = ColorUtils::PixelBytes(pFinalSurface->GetColorFormat());
    if (m_AuxSysmemSurface.IsAllocated())
    {
        GpuTestConfiguration testConfig;
        testConfig.BindGpuDevice(pGpuDevice);
        UNIT_ASSERT_RC(testConfig.InitFromJs());

        DmaWrapper dmaWrap;
        UNIT_ASSERT_RC(dmaWrap.Initialize(&testConfig, Memory::Coherent));
        UNIT_ASSERT_RC(dmaWrap.SetSurfaces(pSurface, pFinalSurface));
        UNIT_ASSERT_RC(dmaWrap.Copy(0, 0, pSurface->GetWidth() * pixelBytes, pSurface->GetHeight(),
                                    0, 0,
                                    testConfig.TimeoutMs(), subdevInst));
    }

    UNIT_ASSERT_RC(SurfaceUtils::ReadSurface(
                   *pFinalSurface, 0, &dstData[0], dstData.size(), subdevInst));

    const UINT08* data   = &dstData[0];
    const UINT32  pitch  = pFinalSurface->GetPitch();
    const UINT32  width  = pFinalSurface->GetWidth();
    const UINT32  height = pFinalSurface->GetHeight();

    MASSERT(pitch >= width * pixelBytes);
    MASSERT(pitch * height <= dstData.size());

    const UINT64 expected = fillValue & ((bpp == 64) ? ~0ULL : ((1ULL << bpp) - 1ULL));

    UINT32 numFailures = 0;

    ++m_SurfaceFillerTestCount;

    for (UINT32 y = 0; y < height; y++)
    {
        for (UINT32 x = 0; x < width; x++, data += pixelBytes)
        {
            UINT64 value = 0;
            switch (bpp)
            {
                case 8:
                    value = *data;
                    break;
                case 16:
                    value = *reinterpret_cast<const UINT16*>(data);
                    break;
                case 24:
                    value = data[0] |
                            (static_cast<UINT32>(data[1]) << 8) |
                            (static_cast<UINT32>(data[2]) << 16);
                    break;
                case 32:
                    value = *reinterpret_cast<const UINT32*>(data);
                    break;
                case 64:
                    value = *reinterpret_cast<const UINT64*>(data);
                    break;
                default:
                    MASSERT(!"Illegal bpp");
            }
            if (value != expected)
            {
                if (numFailures == 0)
                {
                    Printf(Tee::PriError, "Failed %s with seed 0x%08x\n",
                           testInfo.c_str(), m_Seed);
                }
                UNIT_ASSERT_EQ(value, expected);
                Printf(Tee::PriError, "Pixel at [%u, %u] expected 0x%08llx but got 0x%08llx\n",
                       x, y, expected, value);
                ++numFailures;
                if (numFailures >= MAX_FAILURE_DUMPS)
                {
                    Printf(Tee::PriError, "Too many errors, STOP\n");
                    return;
                }
            }
        }
        data += pitch - width * pixelBytes;
    }
}
