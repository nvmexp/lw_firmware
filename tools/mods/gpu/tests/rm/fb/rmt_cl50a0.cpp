/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl50a0cpp
//! \brief LW50_MEMORY_VIRTUAL Test
//!

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl50a0.h" // LW50_MEMORY_VIRTUAL
#include "ctrl/ctrl0080.h"
#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/rmtestutils.h"

#include "core/include/memcheck.h"

class Class50a0Test: public RmTest
{
public:
    Class50a0Test();
    virtual ~Class50a0Test();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    UINT64           m_gpuAddr;
    UINT32 *         m_cpuAddr;
    UINT32           loc_attr;

    LwRm::Handle     m_hVirtMem;
    LwRm::Handle     m_hPhysMem;
    FLOAT64          m_TimeoutMs;
};

//! \brief Class50a0Test constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class50a0Test::Class50a0Test() :
    m_gpuAddr(0LL),
    m_cpuAddr(NULL),
    loc_attr(0),
    m_hVirtMem(0),
    m_hPhysMem(0),
    m_TimeoutMs(Tasker::NO_TIMEOUT)
{
    SetName("Class50a0Test");
}

//! \brief Class50a0Test destructor.
//!
//! \sa Cleanup -----------------------------------------------------------------------------
Class50a0Test::~Class50a0Test()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if LW50_MEMORY_VIRTUAL is supported on the current chip -----------------------------------------------------------------------------
string Class50a0Test::IsTestSupported()
{
    if ( !IsClassSupported(LW50_MEMORY_VIRTUAL) ) return "LW50_MEMORY_VIRTUAL class is not supported on current platform";

    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported -----------------------------------------------------------------------------
RC Class50a0Test::Setup()
{
    // To make test supported for S/R
    SuspendResumeSupported();

    const UINT32 memSize = 0x1000;
    LwRmPtr      pLwRm;
    UINT32       attr;
    UINT32       map_attr;
    UINT32       attr2;
    RC           rc;

    CHECK_RC(GoToSuspendAndResume(1));

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    if (Memory::Fb == Surface2D::GetActualLocation(Memory::Fb, GetBoundGpuSubdevice()))
    {
        loc_attr = LWOS32_ATTR_NONE;
        map_attr = DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                   DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE);
    }
    else
    {
        loc_attr  = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI);
        map_attr = DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                   DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _DISABLE);
    }

    //
    // Allocate the GPU virtual address
    //
    attr = loc_attr | (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT));
    attr2 = LWOS32_ATTR2_NONE;
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_VIRTUAL,
                                       &attr,
                                       &attr2, // pAttr2
                                       memSize,
                                       1,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       &m_hVirtMem,
                                       NULL, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice()));

    //
    // Allocate the physical address
    //
    attr = (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
            DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
            DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
            DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
            DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
            DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
            DRF_DEF(OS32, _ATTR, _LOCATION, _PCI) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _UNCACHED) |
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS));
    attr2 = LWOS32_ATTR2_NONE;
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                       LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                       &attr,
                                       &attr2, //pAttr2
                                       memSize,
                                       1,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       &m_hPhysMem,
                                       NULL, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice()));

    //
    // Now map the GPU virtual address to the physical address
    //
    CHECK_RC(pLwRm->MapMemoryDma(m_hVirtMem, m_hPhysMem, 0, memSize,
                                 map_attr,
                                 &m_gpuAddr, GetBoundGpuDevice()));

    //
    // Get a CPU mapping as well
    //
    CHECK_RC(pLwRm->MapMemory(m_hPhysMem, 0, memSize, (void **)&m_cpuAddr, 0, GetBoundGpuSubdevice()));
    MEM_WR32(m_cpuAddr, 0);

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Class50a0Test::Run()
{
    const       LwU32 relVal = 0x12345678;
    RC          rc;
    LwRmPtr     pLwRm;
    PollArguments args;
    GpuPtr      pGpu;

    CHECK_RC(m_pCh->ScheduleChannel(true));
    CHECK_RC(m_pCh->SetSemaphoreOffset(m_gpuAddr));
    CHECK_RC(m_pCh->SemaphoreRelease(relVal));
    CHECK_RC(m_pCh->Flush());

    args.value  = 0x12345678;
    args.pAddr  = m_cpuAddr;
    CHECK_RC(PollVal(&args, m_TimeoutMs));

    //
    // Quick test of 32b short pointers with grown down
    //
    if (Memory::Fb == Surface2D::GetActualLocation(Memory::Fb, GetBoundGpuSubdevice()))
    {

        if (rc == OK)
        {
            LwRm::Handle m_hVirtMem2;
            UINT32 attr, attr2;
            LwU64 voffset;

            attr =  loc_attr |
                    (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                    DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                    DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                    DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT));
            attr2 = LWOS32_ATTR2_NONE;
            CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                               LWOS32_ALLOC_FLAGS_VIRTUAL |
                                               LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN,
                                               &attr,
                                               &attr2, // pAttr2
                                               1024 * 1024,
                                               1,    // alignment
                                               NULL, // pFormat
                                               NULL, // pCoverage
                                               NULL, // pPartitionStride
                                               &m_hVirtMem2,
                                               &voffset,
                                               NULL, // pLimit
                                               NULL, // pRtnFree
                                               NULL, // pRtnTotal
                                               0,    // width
                                               0,    // height
                                               GetBoundGpuDevice()));
            if (voffset < (1ull << 32))
            {
                Printf(Tee::PriHigh, "Error GROWS_DOWN address is too low! %llx\n",
                      voffset);
                rc = RC::SOFTWARE_ERROR;
            }
            pLwRm->Free(m_hVirtMem2);
        }
        if (rc == OK)
        {
            LwRm::Handle m_hVirtMem2;
            UINT32 attr, attr2;
            LwU64 voffset;

            Printf(Tee::PriNormal, "Running 32BIT_POINTER_ENABLE test.\n");

            attr =  loc_attr |
                    (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                    DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                    DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                    DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT));
            attr2 = DRF_DEF(OS32, _ATTR2, _32BIT_POINTER, _ENABLE);
            CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                               LWOS32_ALLOC_FLAGS_VIRTUAL |
                                               LWOS32_ALLOC_FLAGS_FORCE_MEM_GROWS_DOWN,
                                               &attr,
                                               &attr2, // pAttr2
                                               1024 * 1024,
                                               1,    // alignment
                                               NULL, // pFormat
                                               NULL, // pCoverage
                                               NULL, // pPartitionStride
                                               &m_hVirtMem2,
                                               &voffset,
                                               NULL, // pLimit
                                               NULL, // pRtnFree
                                               NULL, // pRtnTotal
                                               0,    // width
                                               0,    // height
                                               GetBoundGpuDevice()));
            if (voffset >= (1ull << 32))
            {
                Printf(Tee::PriHigh, "Error 32-bit address is too high! %llx\n",
                       voffset);
                rc = RC::SOFTWARE_ERROR;
            }
            pLwRm->Free(m_hVirtMem2);
        }
    }
    if (rc == OK)
    {
        LwRm::Handle m_hVirtMem2;
        UINT32 attr, attr2;
        LwU64 voffset;
        LW0080_CTRL_DMA_GET_CAPS_PARAMS caps;

        caps.capsTblSize = LW0080_CTRL_DMA_CAPS_TBL_SIZE;

        CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                LW0080_CTRL_CMD_DMA_GET_CAPS,
                                &caps, sizeof(caps)));
        if (LW0080_CTRL_DMA_GET_CAP(caps.capsTbl, LW0080_CTRL_DMA_CAPS_32BIT_POINTER_ENFORCED))
        {
            Printf(Tee::PriNormal, "Running 32BIT_POINTER_DISABLE test.\n");

            attr =  loc_attr |
                    (DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                    DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                    DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                    DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _DEFAULT));
            attr2 = DRF_DEF(OS32, _ATTR2, _32BIT_POINTER, _DISABLE);
            CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                               LWOS32_ALLOC_FLAGS_VIRTUAL,
                                               &attr,
                                               &attr2, // pAttr2
                                               1024 * 1024,
                                               1,    // alignment
                                               NULL, // pFormat
                                               NULL, // pCoverage
                                               NULL, // pPartitionStride
                                               &m_hVirtMem2,
                                               &voffset,
                                               NULL, // pLimit
                                               NULL, // pRtnFree
                                               NULL, // pRtnTotal
                                               0,    // width
                                               0,    // height
                                               GetBoundGpuDevice()));
            // This should be > 32b
            if (voffset < (1ull << 32))
            {
                Printf(Tee::PriHigh, "Error non-32-bit address is too low! %llx\n",
                       voffset);
                rc = RC::SOFTWARE_ERROR;
            }
            pLwRm->Free(m_hVirtMem2);
        }
        else
        {
            Printf(Tee::PriNormal, "Skipping 32BIT_POINTER_DISABLE test.\n");
        }
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Class50a0Test::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    if (m_cpuAddr)
        pLwRm->UnmapMemory(m_hPhysMem, m_cpuAddr, 0, GetBoundGpuSubdevice());
    if (m_gpuAddr)
        pLwRm->UnmapMemoryDma(m_hVirtMem, m_hPhysMem,
                              LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());

    pLwRm->Free(m_hVirtMem);
    pLwRm->Free(m_hPhysMem);

    if (m_pCh)
    {
        FIRST_RC(m_TestConfig.FreeChannel(m_pCh));
    }

    return firstRc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class50a0Test
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class50a0Test, RmTest,
                 "Class50a0 RM test.");
