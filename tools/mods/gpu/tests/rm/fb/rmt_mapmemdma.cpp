/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011,2019 by LWPU Corporation.  All rights reserved.   All
 * information contained herein  is proprietary and confidential to LWPU
 * Corporation.   Any use, reproduction, or disclosure without the written
 * permission of LWPU  Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file rmt_mapmemdma.cpp
//! \brief RmMapMemorydma functionality test
//!
//! Idea is to allocate video memory map it to context dma.
//! Test it by polling through a notifier mapped to it.

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "class/cl0040.h"    // LW01_MEMORY_LOCAL_USER
#include "class/cl5097.h"    // LW50_TESLA
#include "class/cl5097sw.h"  // LW50_TESLA
#include "class/cl8297.h"
#include "class/cl8397.h"
#include "class/cl8597.h"
#include "class/cl8697.h"
#include "class/cl9097.h"    // FERMI_A
#include "class/cl9097sw.h"  // FERMI_A
#include "class/cl0070.h"
#include "class/cla06fsubch.h"
#include "gpu/include/gralloc.h"

#include "ctrl/ctrl0080.h"
#include "core/include/memcheck.h"

#define RM_PAGE_SIZE    (1024 * 4)

#define SEMA_INITIAL_VALUE  0xdead
#define SEMA_WAKE_VALUE     0xbeef

#define GR_OBJ_SUBCH LWA06F_SUBCHANNEL_3D

//! \brief NotifyBufferPollFunc: Static function
//!
//! This function is a static one used for the poll and timeout.
//! Polling on the semaphore release condition, timeout condition
//! in case the sema isn't released.
//!
//! \return TRUE if the notifier status is set to  released else FALSE.
//!
//! \sa Run
//-----------------------------------------------------------------------------
static bool NotifyBufferPollFunc(void *pArg)
{
    UINT32 data = MEM_RD32(pArg);

    if (data == SEMA_WAKE_VALUE)
    {
        Printf(Tee::PriLow, "%s: Notifier exit Success\n", __FUNCTION__);
        return true;
    }
    else
    {
        return false;
    }
}

class MapMemoryDmaTest: public RmTest
{
public:
    MapMemoryDmaTest();
    virtual ~MapMemoryDmaTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC CheckShaderAccess(UINT64 gpuAddr, UINT32 index, UINT32 flags46, UINT32 expectedSa);

    //@{
    //! Test specific variables
    UINT64           m_gpuAddr;

    UINT32           *pSema;            // CPU pointer to semaphore

    LwRm::Handle     m_hVidMem;
    LwRm::Handle     m_hVA;
    ThreeDAlloc      m_3dAlloc_First;
    ThreeDAlloc      m_3dAlloc_Second;

    //@}
};

//! \brief Constructor for MapMemoryDmaTest
//!
//! Just does nothing except setting a name for the test
//! and initializing the test specific variables
//! actual functionality is done by Setup()
//!
//! \sa Setup
//-----------------------------------------------------------------------------
MapMemoryDmaTest::MapMemoryDmaTest()
{
    SetName("MapMemoryDmaTest");

    m_hVidMem = 0;
    m_hVA     = 0;
    m_gpuAddr = 0;
    pSema     = NULL;
    m_3dAlloc_First.SetOldestSupportedClass(LW50_TESLA);
    m_3dAlloc_First.SetNewestSupportedClass(GT21A_TESLA);
    m_3dAlloc_Second.SetOldestSupportedClass(FERMI_A);
}

//! \brief MapMemoryDmaTest destructor
//!
//! All resources should be freed in the Cleanup member function, so
//! destructors shouldn't do much.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
MapMemoryDmaTest::~MapMemoryDmaTest()
{
}

//! \brief IsTestSupported()
//!
//! Checks whether the hardware class(es) is/are supported or not
//-----------------------------------------------------------------------------
string MapMemoryDmaTest::IsTestSupported()
{
    //
    // Run this test only on G8x and above.
    // Removing the support for Fermi(for time being)
    // because of the Bug:346336
    //
    if (m_3dAlloc_First.IsSupported(GetBoundGpuDevice()) || m_3dAlloc_Second.IsSupported(GetBoundGpuDevice()))
    {
        return RUN_RMTEST_TRUE;
    }

    return "Test is supported only on G8x and above, for time being unsupported for Fermi(bug:346336)";
}
//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC MapMemoryDmaTest::Setup()
{
    LwRmPtr      pLwRm;
    RC           rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    //
    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    //
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));
    if (m_3dAlloc_First.IsSupported(GetBoundGpuDevice()))
    {
        CHECK_RC(m_3dAlloc_First.Alloc(m_hCh, GetBoundGpuDevice()));
    }
    else
    {
        CHECK_RC(m_3dAlloc_Second.Alloc(m_hCh, GetBoundGpuDevice()));
    }

    // Allocate mappable hVA
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    // Allocate RM_PAGE_SIZE video memory
    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
                                     LWOS32_ATTR_NONE,
                                     LWOS32_ATTR2_NONE,
                                     RM_PAGE_SIZE,
                                     &m_hVidMem,
                                     0,
                                     nullptr, nullptr, nullptr,
                                     GetBoundGpuDevice()));

    // Map video memory to context Dma
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem,
                                 0, RM_PAGE_SIZE - 1,
                                 LW04_MAP_MEMORY_FLAGS_NONE,
                                 &m_gpuAddr, GetBoundGpuDevice()));

    // Map the video memory to notifier
    CHECK_RC(pLwRm->MapMemory(m_hVidMem,
                              0,
                              RM_PAGE_SIZE - 1,
                              (void **)&pSema,
                              0,
                              GetBoundGpuSubdevice()));

    return rc;
}

//! \brief Check shader access bits
RC MapMemoryDmaTest::CheckShaderAccess(UINT64 gpuAddr, UINT32 index, UINT32 flags46, UINT32 saExpected)
{
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS pteInfo = {0};
    LwRmPtr pLwRm;
    LwU32 sa;
    LwU32 i;
    RC rc;

    pteInfo.gpuAddr = gpuAddr;
    pteInfo.subDeviceId = 0;
    pteInfo.skipVASpaceInit = 0;

    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                            &pteInfo, sizeof(pteInfo)));

    for (i = 0 ; i < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; i++)
    {
        if (pteInfo.pteBlocks[i].pageSize == 0)
            continue;

        sa = DRF_VAL(0080_CTRL, _DMA_PTE_INFO, _PARAMS_FLAGS_SHADER_ACCESS, pteInfo.pteBlocks[i].pteFlags);

        if (sa == saExpected)
            return OK;
    }

    Printf(Tee::PriHigh, "Error: No match for index %d flags LWOS46 %x to SHADER_ACCESS %x\n",
           index, flags46, saExpected);

    return RC::SOFTWARE_ERROR;
}

//! \brief Run the test!
//!
//! \return OK if the test passed,i.e when the "notification" is done
//!         test-specific RC if it failed
//! \sa Setup
//-----------------------------------------------------------------------------

RC MapMemoryDmaTest::Run()
{
    LwRmPtr      pLwRm;
    RC           rc = OK;

    // write the current status to notifier
    MEM_WR32(pSema, SEMA_INITIAL_VALUE);
    if (m_3dAlloc_First.IsSupported(GetBoundGpuDevice()))
    {
        // writing commands into the channel
        m_pCh->Write(0, LW5097_SET_OBJECT, m_3dAlloc_First.GetHandle());

        m_pCh->Write(0, LW5097_SET_CTX_DMA_SEMAPHORE , m_hVA);
        m_pCh->Write(0, LW5097_SET_REPORT_SEMAPHORE_A , (UINT32)(m_gpuAddr >> 32LL));
        m_pCh->Write(0, LW5097_SET_REPORT_SEMAPHORE_B , (UINT32)(m_gpuAddr & 0xffffffffLL));
        m_pCh->Write(0, LW5097_SET_REPORT_SEMAPHORE_C , SEMA_WAKE_VALUE);
        m_pCh->Write(0, LW5097_SET_REPORT_SEMAPHORE_D ,
                     DRF_DEF(5097, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                     DRF_DEF(5097, _SET_REPORT_SEMAPHORE_D, _PIPELINE_LOCATION, _ALL));
        m_pCh->Write(0, LW5097_NO_OPERATION, 0);
    }
    else
    {
        CHECK_RC(m_pCh->SetObject(GR_OBJ_SUBCH, m_3dAlloc_Second.GetHandle()));

        m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_REPORT_SEMAPHORE_A , (UINT32)(m_gpuAddr >> 32LL));
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_REPORT_SEMAPHORE_B , (UINT32)(m_gpuAddr & 0xffffffffLL));
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_REPORT_SEMAPHORE_C , SEMA_WAKE_VALUE);
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_SET_REPORT_SEMAPHORE_D ,
                     DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                     DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D, _PIPELINE_LOCATION, _ALL));
        m_pCh->Write(GR_OBJ_SUBCH, LW9097_NO_OPERATION, 0);
    }

    m_pCh->Flush();

    //
    // poll the notifier and check status of notifier if it is success or not
    //
    POLLWRAP(&NotifyBufferPollFunc, pSema, m_TestConfig.TimeoutMs());

    if (MEM_RD32(pSema) != SEMA_WAKE_VALUE)
    {
        Printf(Tee::PriHigh, "Notification Error \nStatus : %x\n",
               MEM_RD32(&pSema));
        rc  =  RC::SOFTWARE_ERROR;
    }

    //
    // unmap the Dma and notifier
    // This may not fit into mods convention but made an exception
    // in view of future enhancements
    //

    pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());
    m_gpuAddr = 0;

    pLwRm->UnmapMemory(m_hVidMem, pSema, 0, GetBoundGpuSubdevice());
    pSema = NULL;

    //
    // Quick test of 32b short pointers with grown down
    //
    if (rc == OK)
    {
        CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem,
                                     0, RM_PAGE_SIZE - 1,
                                     DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_GROWS, _DOWN),
                                     &m_gpuAddr, GetBoundGpuDevice()));
        // This should be > 32b
        if (m_gpuAddr < (1ull << 32))
        {
            Printf(Tee::PriHigh, "Error GROWS_DOWN address is too low! %llx\n",
                   m_gpuAddr);
            rc = RC::SOFTWARE_ERROR;
        }
        pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());
        m_gpuAddr = 0;
    }

    //
    // Check that a 32b pointer request is < 4GB with grows down
    //
    if (rc == OK)
    {
        Printf(Tee::PriNormal, "Run 32BIT_POINTER_ENABLE test\n");

        CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem,
                                     0, RM_PAGE_SIZE - 1,
                                     DRF_DEF(OS46, _FLAGS, _32BIT_POINTER, _ENABLE) |
                                     DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_GROWS, _DOWN),
                                     &m_gpuAddr, GetBoundGpuDevice()));
        // This should be < 32b
        if (m_gpuAddr >= (1ull << 32))
        {
            Printf(Tee::PriHigh, "Error 32-bit address is too high! %llx\n",
                   m_gpuAddr);
            rc = RC::SOFTWARE_ERROR;
        }
        pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());
        m_gpuAddr = 0;
    }

    //
    // Check that a non-32b pointer request is above 4GB on GPUs that requie this mode
    //
    if (rc == OK)
    {
        LW0080_CTRL_DMA_GET_CAPS_PARAMS caps;

        caps.capsTblSize = LW0080_CTRL_DMA_CAPS_TBL_SIZE;

        CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                LW0080_CTRL_CMD_DMA_GET_CAPS,
                                &caps, sizeof(caps)));
        if (LW0080_CTRL_DMA_GET_CAP(caps.capsTbl, LW0080_CTRL_DMA_CAPS_32BIT_POINTER_ENFORCED))
        {
            Printf(Tee::PriNormal, "Run 32BIT_POINTER_DISABLE test\n");

            CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem,
                                         0, RM_PAGE_SIZE - 1,
                                         DRF_DEF(OS46, _FLAGS, _32BIT_POINTER, _DISABLE) |
                                         DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_GROWS, _UP),
                                         &m_gpuAddr, GetBoundGpuDevice()));
            // This should be > 32b
            if (m_gpuAddr < (1ull << 32))
            {
                Printf(Tee::PriHigh, "Error non-32-bit address is too low! %llx\n",
                       m_gpuAddr);
                rc = RC::SOFTWARE_ERROR;
            }
            pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem,
                                  LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());
            m_gpuAddr = 0;
        }
        else
        {
            Printf(Tee::PriNormal, "Skip 32BIT_POINTER_DISABLE test\n");
        }

        if (rc == OK)
        {
            if (LW0080_CTRL_DMA_GET_CAP(caps.capsTbl, LW0080_CTRL_DMA_CAPS_SHADER_ACCESS_SUPPORTED))
            {
                static const struct saTests
                {
                    UINT32 flags46;
                    UINT32 flagsSA;
                } tests[] = {
                    { DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _READ_ONLY),
                      LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_READ_ONLY },
                    { DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _WRITE_ONLY),
                      LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_WRITE_ONLY },
                    { DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _READ_WRITE),
                      LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_READ_WRITE },
                    { DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _DEFAULT) | DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_ONLY),
                      LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_READ_ONLY },
                    { DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _DEFAULT) | DRF_DEF(OS46, _FLAGS, _ACCESS, _WRITE_ONLY),
                      LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_WRITE_ONLY },
                    { DRF_DEF(OS46, _FLAGS, _SHADER_ACCESS, _DEFAULT) | DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE),
                      LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_SHADER_ACCESS_READ_WRITE }
                };
                UINT32 count = sizeof(tests) / sizeof(struct saTests);
                UINT32 i;

                //
                // Test shader read only and write only, both direclty set and in compatibility
                // mode.
                //
                Printf(Tee::PriNormal, "Run SHADER_ACCESS test - %d instances\n", count);

                for (i = 0 ; i < count; i ++)
                {
                    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hVidMem,
                                                 0, RM_PAGE_SIZE - 1,
                                                 tests[i].flags46,
                                                 &m_gpuAddr, GetBoundGpuDevice()));
                    CHECK_RC(CheckShaderAccess(m_gpuAddr, i, tests[i].flags46, tests[i].flagsSA));
                    pLwRm->UnmapMemoryDma(m_hVA, m_hVidMem,
                                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());
                    m_gpuAddr = 0;
                }
            }
            else
            {
                Printf(Tee::PriNormal, "Skip SHADER_ACCESS test\n");
            }
        }
    }

    return rc;
}

//! \brief Free any resources that this test selwred during Setup.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails,
//! so cleaning up the allocated context DMA, notifier,object and the channel

//!
//! \sa Setup

RC MapMemoryDmaTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    pLwRm->Free(m_hVA);
    m_hVA = 0;

    pLwRm->Free(m_hVidMem);
    m_hVidMem = 0;

    m_TestConfig.IdleChannel(m_pCh);
    m_3dAlloc_First.Free();
    m_3dAlloc_Second.Free();

    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//--------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ MapMemoryDmaTest
//!object.
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(MapMemoryDmaTest, RmTest,
                 "MapMemoryDma functionality test.");
