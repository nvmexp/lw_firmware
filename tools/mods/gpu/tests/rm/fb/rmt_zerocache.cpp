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

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/channel.h"
#include "lwos.h"
#include "gpu/utility/surf2d.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pmusub.h"
#include "ctrl/ctrl85b6.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl208f.h"

#include "class/cl50a0.h"  // LW50_MEMORY_VIRTUAL
#include "class/cl866f.h"  // GT21A_CHANNEL_GPFIFO
#include "class/cl8697.h"  // GT21A_TESLA
#include "class/cl8697sw.h"
#include "gpu/include/notifier.h"
#include "core/include/memcheck.h"
//!
//! \file rmt_zerocache.cpp
//! \brief Test to verify basic APIs associated with zero-cache mode.  A power
//! saving mode that powers off on-chip RAMs associated with the GPU cache.
//!

#define ZERO_CACHE_TEST_FILL_PATTERN 0xffffffff
//
// If this is going to be non-zero then you need to make sure the
// clear-value sent to HW is adjusted
//
#define ZERO_CACHE_TEST_CLEAR_PATTERN 0x0

#define ZERO_CACHE_TEST_MODE_PMU 1
#define ZERO_CACHE_TEST_MODE_RM  2

class ZeroCacheTest : public RmTest
{
public:
    ZeroCacheTest();
    virtual ~ZeroCacheTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC preTest();

    RC doZeroCacheTest(UINT32, UINT32);

    RC doHwClear();

    bool m_verifyCaching;

    Channel* m_pCh;
    LwRm::Handle m_hCh;
    UINT32 m_subCh;

    LwRm::Handle m_obj3d;

    Surface2D *m_pSurf;

    PMU *m_pPmu;
    LW2080_CTRL_FB_GET_GPU_CACHE_INFO_PARAMS m_cacheInfo;

    LwRm::Handle m_hSubdevDiag;
    Notifier     m_Notifier;

    void* m_pDirect;

};

//! \brief ZeroCacheTest constructor
//!
//! Placeholder : doesn't do much, most functionality in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
ZeroCacheTest::ZeroCacheTest()
{
    SetName("ZeroCacheTest");
}

//! \brief ZeroCacheTest destructor
//!
//! Placeholder : doesn't do much, most functionality in Cleanup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ZeroCacheTest::~ZeroCacheTest()
{

}

//! \brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Only supported on iGT21A with 3d class enabled.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ZeroCacheTest::IsTestSupported()
{
    bool       bIsSupported;

   bIsSupported =
   (IsClassSupported(GT21A_CHANNEL_GPFIFO) &&
    IsClassSupported(GT21A_TESLA) &&
    IsClassSupported(LW50_MEMORY_VIRTUAL));

   Printf(Tee::PriLow,"%d:ZeroCacheTest::IsSupported retured %d \n",
         __LINE__, bIsSupported);

   if( bIsSupported )
       return RUN_RMTEST_TRUE;
   return "One of these GT21A_CHANNEL_GPFIFO, GT21A_TESLA, LW50_MEMORY_VIRTUAL classes is not supported on current platform";
}

//! \brief  Setup(): Allocates resources necessary for this test.
//!
//! Allocating the channel, memory, and objects as well as exelwting any necessary
//! cache state setup.
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC ZeroCacheTest::Setup()
{
    RC rc = OK;
    LwRmPtr pLwRm;
    LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS setParams = {0};

    CHECK_RC(m_TestConfig.InitFromJs());

    m_hSubdevDiag= GetBoundGpuSubdevice()->GetSubdeviceDiag();

    // Cache isn't simulated in FModel, so we don't check that data is in cache
    m_verifyCaching = (Platform::GetSimulationMode() != Platform::Fmodel);

    if (GetBoundGpuSubdevice()->GetPmu(&m_pPmu) == RC::UNSUPPORTED_FUNCTION)
    {
        m_pPmu = NULL;
    }

    // All setup below this is to verify caching
    if (m_verifyCaching == false)
    {
        return OK;
    }

    m_subCh = 0;

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC(pLwRm->Alloc(m_hCh, &m_obj3d, GT21A_TESLA, NULL));
    m_Notifier.Allocate(m_pCh, 1, GetTestConfiguration());
    m_pSurf = new Surface2D;

    // 128x128x4 = 64KB - will fit completely in cache even when floorswept
    m_pSurf->SetWidth(128);
    m_pSurf->SetHeight(128);
    m_pSurf->SetColorFormat(ColorUtils::A8R8G8B8);
    m_pSurf->SetLayout(Surface2D::Pitch);
    m_pSurf->SetGpuCacheMode(Surface2D::GpuCacheOn);
    m_pSurf->SetCompressed(false);
    //
    // Has to be sysmem until Bug 333648 is fixed so we can get a direct
    // mapping
    //
    m_pSurf->SetLocation(Memory::NonCoherent);
    m_pSurf->BindGpuChannel(m_hCh);
    CHECK_RC(m_pSurf->Alloc(GetBoundGpuDevice()));

    CHECK_RC(pLwRm->MapMemory(m_pSurf->GetMemHandle(), 0, m_pSurf->GetAllocSize(), &m_pDirect,
                DRF_DEF(OS33, _FLAGS, _MAPPING, _DIRECT), GetBoundGpuSubdevice() ));

    CHECK_RC(m_pSurf->Map());

    // Get cache mode and set to write-back if necessary
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_FB_GET_GPU_CACHE_INFO,
                &m_cacheInfo, sizeof(m_cacheInfo)));

    if (m_cacheInfo.writeMode ==
            LW2080_CTRL_FB_GET_GPU_CACHE_INFO_WRITE_MODE_WRITETHROUGH)
    {
        setParams.writeMode = LW208F_CTRL_FB_CTRL_GPU_CACHE_WRITE_MODE_WRITEBACK;
        CHECK_RC(pLwRm->Control(m_hSubdevDiag,
                    LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                    &setParams, sizeof(setParams)));
    }

    return rc;
}

//! \brief  Run(): Exelwtes the test
//!
//! Does initial pre-test to verify environment and then runs simple tests for
//! both RM and PMU versions of Zero-Cache implementations
//!
//! \return corresponding RC if the test fails
//------------------------------------------------------------------------------
RC ZeroCacheTest::Run()
{
    RC rc = OK;

    if (m_verifyCaching)
    {
        CHECK_RC(preTest());
    }

    CHECK_RC(doZeroCacheTest(ZERO_CACHE_TEST_MODE_RM, ZERO_CACHE_TEST_MODE_RM));

    if (m_pPmu)
    {
        UINT32 ucodeState;
        if ((m_pPmu->GetUcodeState(&ucodeState) == OK) &&
                (ucodeState == LW85B6_CTRL_PMU_UCODE_STATE_READY))
        {
            CHECK_RC(doZeroCacheTest(ZERO_CACHE_TEST_MODE_PMU, ZERO_CACHE_TEST_MODE_PMU));
            CHECK_RC(doZeroCacheTest(ZERO_CACHE_TEST_MODE_PMU, ZERO_CACHE_TEST_MODE_RM));
            CHECK_RC(doZeroCacheTest(ZERO_CACHE_TEST_MODE_RM, ZERO_CACHE_TEST_MODE_PMU));
        }
    }
    return rc;
}

//! \brief  Cleanup(): Cleans up any resources/state associated with this test.
//!
//!
//!
//! \return corresponding RC if the clenaup fails
//------------------------------------------------------------------------------
RC ZeroCacheTest::Cleanup()
{
    LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS setParams = {0};
    RC rc = OK;
    LwRmPtr pLwRm;
    m_Notifier.Free();
    if (m_verifyCaching)
    {
        pLwRm->Free(m_obj3d);
        m_TestConfig.FreeChannel(m_pCh);

        pLwRm->UnmapMemory(m_pSurf->GetMemHandle(), m_pDirect, 0, GetBoundGpuSubdevice());

        m_pSurf->Free();
        delete m_pSurf;

        // Restore cache mode
        if (m_cacheInfo.writeMode ==
                LW2080_CTRL_FB_GET_GPU_CACHE_INFO_WRITE_MODE_WRITETHROUGH)
        {
            setParams.writeMode =
                LW208F_CTRL_FB_CTRL_GPU_CACHE_WRITE_MODE_WRITETHROUGH;

            CHECK_RC(pLwRm->Control(m_hSubdevDiag,
                        LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                        &setParams, sizeof(setParams)));
        }
    }

    return rc;
}

//! \brief  preTest(): Test assumptions made in this test
//!
//! This test relies on certain assumptions about the cache.  We need to be
//! able to induce certain situations such as a certain memory pattern in memory
//! and another in the cache.  This function verifies that the methods chosen to
//! induce these situations are valid.
//!
//! \return corresponding RC if the test fails
//------------------------------------------------------------------------------
RC
ZeroCacheTest::preTest()
{
    LwRmPtr pLwRm;
    RC rc = OK;

    LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS flushParams;
    vector<UINT32>  fillPatterlwec(1, ZERO_CACHE_TEST_FILL_PATTERN);

    // Evict everything from the cache prior to the test
    memset(&flushParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

    flushParams.flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
        DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
            LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
            (void*) &flushParams,
            sizeof(flushParams)));

    // Fills memory NOT cache (reflected writes don't allocate in cache).
    CHECK_RC(m_pSurf->Fill(ZERO_CACHE_TEST_FILL_PATTERN));

    // Fills cache, shouldn't touch memory
    CHECK_RC(doHwClear());

    // surface should still have initial pattern
    CHECK_RC(Memory::ComparePattern(m_pDirect, &fillPatterlwec, (UINT32) m_pSurf->GetAllocSize()));

    return rc;
}

//! \brief  doZeroCacheTest(): Tests implementation of Zero-Cache mode
//!
//! Fills the cache and memory with different pattersn, then transitions into
//! zero-cache mode and ensures that memory now has the pattern that was in the
//! cache.  Then transitions out of zero-cache mode, checks for any corruption,
//! and then checks that cache is enabled by filling memory and cache with
//! different patterns.
//!
//! \return corresponding RC if the test fails
//------------------------------------------------------------------------------
RC ZeroCacheTest::doZeroCacheTest(UINT32 enterZeroCache, UINT32 leaveZeroCache)
{
    LwRmPtr pLwRm;
    RC rc = OK;
    LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS params;
    vector<UINT32>  fillPatterlwec(1, ZERO_CACHE_TEST_FILL_PATTERN);
    vector<UINT32>  clearPatterlwec(1, ZERO_CACHE_TEST_CLEAR_PATTERN);

    if (m_verifyCaching)
    {
        LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS flushParams;

        // Evict everything from the cache prior to the test
        memset(&flushParams, 0, sizeof(LW2080_CTRL_FB_FLUSH_GPU_CACHE_PARAMS));

        flushParams.flags = DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _FLUSH_MODE, _FULL_CACHE) |
            DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _WRITE_BACK, _YES) |
            DRF_DEF(2080, _CTRL_FB_FLUSH_GPU_CACHE_FLAGS, _ILWALIDATE, _YES);

        CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE,
                (void*) &flushParams,
                sizeof(flushParams)));

        // Fill surfaces
        CHECK_RC(m_pSurf->Fill(ZERO_CACHE_TEST_FILL_PATTERN));
        CHECK_RC(doHwClear());
    }

    // Do zero-cache transition
    if (enterZeroCache == ZERO_CACHE_TEST_MODE_RM)
    {
        Printf(Tee::PriLow, "Begin RM Zero-Cache transition to zero cache\n");
        memset(&params, 0, sizeof(params));
        params.rcmState = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_ZERO_CACHE;
        params.flags = DRF_DEF(208F, _CTRL_FB_CTRL_GPU_CACHE_FLAGS, _MODE, _RM);

        CHECK_RC(pLwRm->Control(m_hSubdevDiag,
                               LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                               (void*)&params,
                               sizeof(LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS)));

        Printf(Tee::PriLow, "End RM Zero-Cache transition to zero cache\n");
    }
    else if (enterZeroCache == ZERO_CACHE_TEST_MODE_PMU)
    {
        Printf(Tee::PriLow, "Begin PMU Zero-Cache transition to zero cache\n");
        memset(&params, 0, sizeof(params));
        params.rcmState = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_ZERO_CACHE;
        params.flags = DRF_DEF(208F, _CTRL_FB_CTRL_GPU_CACHE_FLAGS, _MODE, _PMU);

        CHECK_RC(pLwRm->Control(m_hSubdevDiag,
                               LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                               (void*)&params,
                               sizeof(LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS)));

        Printf(Tee::PriHigh, "Failed to start PMU task to enter zero-cache mode "
                "(LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE should have caught this).\n");
        CHECK_RC(RC::SOFTWARE_ERROR);

        Printf(Tee::PriLow, "End PMU Zero-Cache transition to zero cache\n");
    }
    else
    {
        MASSERT(!"INVALID cache-transition mode (must be RM || PMU)");
    }

    if (m_verifyCaching)
    {
        // Makes sure data was flushed to memory and both mappings are the same.
        CHECK_RC(Memory::ComparePattern(m_pSurf->GetAddress(), &clearPatterlwec, (UINT32) m_pSurf->GetAllocSize()));
        CHECK_RC(Memory::ComparePattern(m_pDirect, &clearPatterlwec, (UINT32) m_pSurf->GetAllocSize()));

        //
        // Verify cache is disabled by doing another mem2mem and verifying directed
        // and reflected match.  If cache were enabled we'd expect a mismatch.
        //
        CHECK_RC(m_pSurf->Fill(ZERO_CACHE_TEST_FILL_PATTERN));
        CHECK_RC(doHwClear());

        CHECK_RC(Memory::Compare(m_pDirect, m_pSurf->GetAddress(), (UINT32) m_pSurf->GetAllocSize()));
    }

    // Do full-cache transition
    if (leaveZeroCache == ZERO_CACHE_TEST_MODE_RM)
    {
        Printf(Tee::PriLow, "Begin RM Zero-Cache transition to full cache\n");

        memset(&params, 0, sizeof(params));
        params.rcmState = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_FULL;
        params.flags = DRF_DEF(208F, _CTRL_FB_CTRL_GPU_CACHE_FLAGS, _MODE, _RM);
        CHECK_RC(pLwRm->Control(m_hSubdevDiag,
                               LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                               (void*)&params,
                               sizeof(LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS)));
        Printf(Tee::PriLow, "End RM Zero-Cache transition to full cache\n");
    }
    else if (leaveZeroCache == ZERO_CACHE_TEST_MODE_PMU)
    {
        Printf(Tee::PriLow, "Begin PMU Zero-Cache transition to full cache\n");
        memset(&params, 0, sizeof(params));
        params.rcmState = LW208F_CTRL_FB_CTRL_GPU_CACHE_RCM_STATE_FULL;
        params.flags = DRF_DEF(208F, _CTRL_FB_CTRL_GPU_CACHE_FLAGS, _MODE, _PMU);

        CHECK_RC(pLwRm->Control(m_hSubdevDiag,
                               LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE,
                               (void*)&params,
                               sizeof(LW208F_CTRL_FB_CTRL_GPU_CACHE_PARAMS)));

        Printf(Tee::PriHigh, "Failed to start PMU task to enter zero-cache mode "
                "(LW208F_CTRL_CMD_FB_CTRL_GPU_CACHE should have caught this).\n");
        CHECK_RC(RC::SOFTWARE_ERROR);

        Printf(Tee::PriLow, "End PMU Zero-Cache transition to full cache\n");
    }
    else
    {
        MASSERT(!"INVALID cache-transition mode (must be RM || PMU)");
    }

    if (m_verifyCaching)
    {
        // Both mappings should still be equal.
        CHECK_RC(Memory::ComparePattern(m_pSurf->GetAddress(), &clearPatterlwec, (UINT32) m_pSurf->GetAllocSize()));
        CHECK_RC(Memory::Compare(m_pDirect, m_pSurf->GetAddress(), (UINT32) m_pSurf->GetAllocSize()));

        //
        // Finally, verify that the cache is actually enabled with another mem2mem
        // that should go to the cache and not memory.
        //
        CHECK_RC(m_pSurf->Fill(ZERO_CACHE_TEST_FILL_PATTERN));
        CHECK_RC(doHwClear());

        // surface should still have initial pattern
        CHECK_RC(Memory::ComparePattern(m_pDirect, &fillPatterlwec, (UINT32) m_pSurf->GetAllocSize()));

    }

    return rc;
}
//! \brief  Helper function which uses 3d class to clear surface in order to allocate in cache
//!
//!
//! \return true if task is completed, false otherwise
//------------------------------------------------------------------------------
RC
ZeroCacheTest::doHwClear()
{
    RC rc = OK;

    m_pCh->Write(m_subCh, LW8697_SET_OBJECT, m_obj3d);
    m_pCh->Write(m_subCh, LW8697_SET_SURFACE_CLIP_HORIZONTAL,
            DRF_NUM(8697, _SET_SURFACE_CLIP_HORIZONTAL, _WIDTH, 128));
    m_pCh->Write(m_subCh, LW8697_SET_SURFACE_CLIP_VERTICAL,
            DRF_NUM(8697, _SET_SURFACE_CLIP_VERTICAL, _HEIGHT, 128));
    m_pCh->Write(m_subCh, LW8697_SET_VIEWPORT_CLIP_HORIZONTAL(0),
            DRF_NUM(8697, _SET_VIEWPORT_CLIP_HORIZONTAL, _WIDTH, 128));
    m_pCh->Write(m_subCh, LW8697_SET_VIEWPORT_CLIP_VERTICAL(0),
            DRF_NUM(8697, _SET_VIEWPORT_CLIP_VERTICAL, _HEIGHT, 128));
    m_pCh->Write(m_subCh, LW8697_SET_CLIP_ID_TEST,0);
    m_pCh->Write(m_subCh, LW8697_SET_WINDOW_OFFSET_X, 0);
    m_pCh->Write(m_subCh, LW8697_SET_WINDOW_OFFSET_Y, 0);
    m_pCh->Write(m_subCh, LW8697_SET_STENCIL_MASK, 0xff);

    m_pCh->Write(m_subCh, LW8697_SET_CTX_DMA_COLOR(0), m_pSurf->GetCtxDmaHandleGpu());
    m_pCh->Write(m_subCh, LW8697_SET_CT_A(0), LwU64_HI32(m_pSurf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_B(0), LwU64_LO32(m_pSurf->GetCtxDmaOffsetGpu()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_FORMAT(0), LW8697_SET_CT_FORMAT_V_A8R8G8B8);
    m_pCh->Write(m_subCh, LW8697_SET_CT_ARRAY_PITCH(0), (UINT32) (m_pSurf->GetArrayPitch() >> 2));
    m_pCh->Write(m_subCh, LW8697_SET_CT_SIZE_A(0),
        DRF_DEF(8697, _SET_CT_SIZE_A, _LAYOUT_IN_MEMORY, _PITCH) |
        DRF_NUM(8697, _SET_CT_SIZE_A, _WIDTH, m_pSurf->GetPitch()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_SIZE_B(0),
        DRF_NUM(8697, _SET_CT_SIZE_B, _HEIGHT, m_pSurf->GetHeight()));
    m_pCh->Write(m_subCh, LW8697_SET_CT_SIZE_C,
            DRF_NUM(8697, _SET_CT_SIZE_C, _THIRD_DIMENSION, 1) |
            DRF_DEF(8697, _SET_CT_SIZE_C, _CONTROL, _THIRD_DIMENSION_DEFINES_ARRAY_SIZE));

    m_pCh->Write(m_subCh, LW8697_SET_CLEAR_CONTROL,
            DRF_DEF(8697, _SET_CLEAR_CONTROL, _RESPECT_STENCIL_MASK, _FALSE) |
            DRF_DEF(8697, _SET_CLEAR_CONTROL, _USE_CLEAR_RECT, _FALSE));

    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(0), 0);  // R
    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(1), 0);  // G
    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(2), 0);  // B
    m_pCh->Write(m_subCh, LW8697_SET_COLOR_CLEAR_VALUE(3), 0);  // A
    m_pCh->Write(m_subCh, LW8697_SET_CT_MARK(0), DRF_DEF(8697, _SET_CT_MARK, _IEEE_CLEAN, _TRUE));

    m_pCh->Write(m_subCh, LW8697_SET_CT_WRITE(0),
        DRF_DEF(8697, _SET_CT_WRITE, _R_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _G_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _B_ENABLE, _TRUE) |
        DRF_DEF(8697, _SET_CT_WRITE, _A_ENABLE, _TRUE));

    m_pCh->Write(m_subCh, LW8697_SET_RENDER_ENABLE_A, 0);
    m_pCh->Write(m_subCh, LW8697_SET_RENDER_ENABLE_B, 0);
    m_pCh->Write(m_subCh, LW8697_SET_RENDER_ENABLE_C, 1);

     // Assumes no 3D RT arrays
     UINT32 MaxZ = max(m_pSurf->GetDepth(),
         m_pSurf->GetArraySize());

    for (UINT32 z = 0; z < MaxZ; z++)
    {
        // Enable all the color planes
        UINT32 enables =
            DRF_NUM(8697, _CLEAR_SURFACE, _MRT_SELECT, 0) |
            DRF_NUM(8697, _CLEAR_SURFACE, _RT_ARRAY_INDEX, z) |
            DRF_DEF(8697, _CLEAR_SURFACE, _R_ENABLE, _TRUE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _G_ENABLE, _TRUE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _B_ENABLE, _TRUE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _STENCIL_ENABLE, _FALSE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _Z_ENABLE, _FALSE) |
            DRF_DEF(8697, _CLEAR_SURFACE, _A_ENABLE, _TRUE);

        // Trigger the clear
        m_pCh->Write(m_subCh,LW8697_CLEAR_SURFACE, enables);
    }
    CHECK_RC(m_Notifier.Instantiate(m_subCh, LW8697_SET_CONTEXT_DMA_NOTIFY));
    m_Notifier.Clear(LW8697_NOTIFIERS_NOTIFY);
    CHECK_RC(m_Notifier.Write(m_subCh));
    CHECK_RC(m_pCh->Write(m_subCh, LW8697_NO_OPERATION, 0));
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_Notifier.Wait(LW8697_NOTIFIERS_NOTIFY, m_TestConfig.TimeoutMs()));

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ZeroCacheTest object
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ZeroCacheTest, RmTest,
    "ZeroCacheTest RMTEST that tests Zero Cache Mode, lwrrently specific to iGT21A");

