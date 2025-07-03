/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2011,2014,2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_cl0070.cpp
//! \brief LW01_MEMORY_VIRTUAL Test
//!

#include "gpu/tests/rmtest.h"
#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/include/gpudev.h"

#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "ctrl/ctrl208f.h" // LW208F_CTRL_BUS_IS_BAR1_VIRTUAL
#include "core/include/memcheck.h"

#define SW_SUBCH 5

//!
//!
//! Structure to hold test conditions
//! Memory(CohSysMem, nCohSysMem)
//! Size Memory (4K, 1M, 12M)
//!
typedef struct __semaphore_test {

    bool             isSysMem;
    bool             isCoherentSysMem;
    bool             isValid;
    bool             isContig;

    UINT32           sizeMem;

    UINT64           gpuAddr;
    UINT32 *         cpuAddr;

    Channel *        pChan;
    LwRm::Handle     hChan;
    LwRm::Handle     hDynMem;
    LwRm::Handle     hSemMem;
    LwRm::Handle     hVA;
    LwRm::Handle     hObj;

} SEMAPHORE_TEST;

static SEMAPHORE_TEST   m_semaphore[] = {
    /* coherent sysmem, size = 4K */
    {true, true, false, false, 0x00001000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    {true, true, false, true,  0x00001000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* coherent sysmem, size = 1M */
    {true, true, false, false, 0x00100000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    {true, true, false, true,  0x00100000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* coherent sysmem, size = 12M */
    {true, true, false, false, 0x00C00000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* MODS cannot allocate 12M contig sysmem */

    /* noncoherent sysmem, size = 4K */
    {true, false, false, false, 0x00001000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    {true, false, false, true,  0x00001000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* noncoherent sysmem, size = 1M */
    {true, false, false, false, 0x00100000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    {true, false, false, true,  0x00100000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* noncoherent sysmem, size = 12M */
    {true, false, false, false, 0x00C00000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* MODS cannot allocate 12M contig sysmem */

    /* fb mem, size = 4K */
    {false, false, false, false, 0x00001000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    {false, false, false, true,  0x00001000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* fb mem, size = 1M */
    {false, false, false, false, 0x00100000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    {false, false, false, true,  0x00100000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    /* fb mem, size = 12M */
    {false, false, false, false, 0x00C00000, 0LL, NULL, NULL, 0, 0, 0, 0, 0},
    {false, false, false, true,  0x00C00000, 0LL, NULL, NULL, 0, 0, 0, 0, 0}
};

class Class0070Test: public RmTest
{
public:
    Class0070Test();
    virtual ~Class0070Test();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    RC CreateSemaphoreMemory(SEMAPHORE_TEST *sm, UINT32 legacy);
    RC TestSemaphorePayload (SEMAPHORE_TEST *sm);
    RC FreeSemaphoreMemory(SEMAPHORE_TEST *sm);

    FLOAT64          m_timeoutMs = Tasker::NO_TIMEOUT;
    bool             m_bIsBar1Virtual = false;
};

//! \brief Class0070Test constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
Class0070Test::Class0070Test()
{
    SetName("Class0070Test");
}

//! \brief Class0070Test destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
Class0070Test::~Class0070Test()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if G8x+
//-----------------------------------------------------------------------------
string Class0070Test::IsTestSupported()
{
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_GPFIFO_CHANNEL))
        return RUN_RMTEST_TRUE;
    return "Should have GPUSUB_HAS_GPFIFO_CHANNEL feature";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC Class0070Test::Setup()
{
    LwRmPtr pLwRm;
    RC      rc;
    LW208F_CTRL_BUS_IS_BAR1_VIRTUAL_PARAMS bar1InfoParams;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_timeoutMs = GetTestConfiguration()->TimeoutMs();
    m_TestConfig.SetAllowMultipleChannels(true);

    CHECK_RC(pLwRm->Control(GetBoundGpuSubdevice()->GetSubdeviceDiag(),
                            LW208F_CTRL_CMD_BUS_IS_BAR1_VIRTUAL,
                            &bar1InfoParams,
                            sizeof(bar1InfoParams)));

    m_bIsBar1Virtual = bar1InfoParams.bIsVirtual;

    for ( UINT32 i = 0 ; i < (sizeof(m_semaphore)/sizeof(SEMAPHORE_TEST)); i++ )
    {
        if ((!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_SYSMEM) && m_semaphore[i].isSysMem) ||
            (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_COHERENT_SYSMEM) && m_semaphore[i].isCoherentSysMem))
        {
            // skip configs that aren't supported on this chip.
            continue;
        }

        // If BAR1 is not virtual, RM will refuse all discontig map requests across BAR1 to vidmem
        if (!m_bIsBar1Virtual &&
            !m_semaphore[i].isContig &&
            !m_semaphore[i].isSysMem &&
            !m_semaphore[i].isCoherentSysMem)
        {
            continue; 
        }

        CHECK_RC(CreateSemaphoreMemory(&m_semaphore[i], i & 1));
        m_semaphore[i].isValid = true;
    }
    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC Class0070Test::Run()
{
    RC rc;

    for ( UINT32 i = 0 ; i < (sizeof(m_semaphore)/sizeof(SEMAPHORE_TEST)); i++ ) {

        if (m_semaphore[i].isValid)
        {
            CHECK_RC(TestSemaphorePayload(&m_semaphore[i]));
        }
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC Class0070Test::Cleanup()
{
    RC rc, firstRc;

    for ( UINT32 i = 0 ; i < (sizeof(m_semaphore)/sizeof(SEMAPHORE_TEST)); i++ )
    {
        FIRST_RC(FreeSemaphoreMemory(&m_semaphore[i]));
    }

    return firstRc;
}

//!
//! Allocate system memory and make it visible to cpu(cpuAddr) and gpu(gpuAddr).
//!
//-----------------------------------------------------------------------------
RC Class0070Test::CreateSemaphoreMemory(SEMAPHORE_TEST *sm, UINT32 legacy)
{
    LwRmPtr      pLwRm;
    RC           rc;

    void *       pDummy;
    UINT64       dynMemLimit;
    UINT64       Offset;

    CHECK_RC(m_TestConfig.AllocateChannel(&sm->pChan, &sm->hChan, LW2080_ENGINE_TYPE_GR(0)));

    //
    // Allocate a page of system/Frame buffer memory
    //
    if (sm->isSysMem ) {

        Printf(Tee::PriHigh,"Using System Memory Size : 0x%x\n", sm->sizeMem);

        CHECK_RC(pLwRm->AllocSystemMemory(&sm->hSemMem,
            DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
            (sm->isContig ?
            DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _CONTIGUOUS) :
            DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS)) |
            (sm->isCoherentSysMem ?
            DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED) :
            DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED)),
            sm->sizeMem, GetBoundGpuDevice()));

    } else {

        Printf(Tee::PriHigh,"Using Video Memory Size : 0x%x\n", sm->sizeMem);

        CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
            DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
            (sm->isContig ?
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS) :
            DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS)) |
            DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED),
            LWOS32_ATTR2_NONE,
            sm->sizeMem, &sm->hSemMem, &Offset,
            nullptr, nullptr, nullptr,
            GetBoundGpuDevice()));

    };

    //
    // Create an "dynamic" context DMA to which this memory can be added.  Use of mix
    // of the legacy SYSMEM_DYNAMIC interface and the current interface.
    //
    if (legacy)
    {
        CHECK_RC(pLwRm->AllocMemory(&sm->hDynMem, LW01_MEMORY_SYSTEM_DYNAMIC,
                                    DRF_DEF(OS02, _FLAGS, _ALLOC, _NONE),
                                    &pDummy, &dynMemLimit, GetBoundGpuDevice()));

        CHECK_RC(pLwRm->AllocContextDma(&sm->hVA,
                                        DRF_DEF(OS03, _FLAGS, _ACCESS, _READ_WRITE) |
                                        (sm->isSysMem && sm->isCoherentSysMem ?
                                         DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _ENABLE) :
                                         DRF_DEF(OS03, _FLAGS, _CACHE_SNOOP, _DISABLE)),
                                        sm->hDynMem, 0, dynMemLimit));
    }
    else
    {
        LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };

        sm->hDynMem = 0;

        CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                              &sm->hVA, LW01_MEMORY_VIRTUAL, &vmparams));
    }

    //
    // Add the system/FB memory to the address space.  The "offset" returned
    // is actually the GPU virtual address in this case.
    //
    if (sm->isSysMem && sm->isCoherentSysMem) {
        // For coherent system memory, The snoop is used to make sure the cache coherency
        CHECK_RC(pLwRm->MapMemoryDma(sm->hVA,
                                     sm->hSemMem,
                                     0,
                                     sm->sizeMem,
                                     DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                     &sm->gpuAddr,
                                     GetBoundGpuDevice()));
    } else {
        // Noncoherent system memory, Always coherent FB memory, snooping is not required.
        CHECK_RC(pLwRm->MapMemoryDma(sm->hVA,
                                     sm->hSemMem,
                                     0,
                                     sm->sizeMem,
                                     LW04_MAP_MEMORY_FLAGS_NONE,
                                     &sm->gpuAddr,
                                     GetBoundGpuDevice()));
    }

    //
    // Get a CPU address as well
    //
    CHECK_RC(pLwRm->MapMemory(sm->hSemMem, 0, sm->sizeMem, (void **)&sm->cpuAddr, 0, GetBoundGpuSubdevice()));

    // SW class
    CHECK_RC(pLwRm->Alloc(sm->hChan, &sm->hObj, LW04_SOFTWARE_TEST, NULL));

    return rc;
}

//! Verify the Semaphore release payloads for 4b & 16b cases.
//!
//------------------------------------------------------------------------------
RC Class0070Test::TestSemaphorePayload(SEMAPHORE_TEST *sm)
{
    RC rc = OK;
    const UINT32 semExp = 0x12345678;
    UINT32       semGot;

    MEM_WR32(sm->cpuAddr, 0);

    sm->pChan->SetObject(SW_SUBCH, sm->hObj);

    CHECK_RC(sm->pChan->SetSemaphoreOffset(sm->gpuAddr));
    CHECK_RC(sm->pChan->SemaphoreRelease(semExp));
    CHECK_RC(sm->pChan->Flush());
    sm->pChan->WaitIdle(m_timeoutMs);

    semGot = MEM_RD32(sm->cpuAddr);
    if (semGot != semExp)
    {
        Printf(Tee::PriHigh, "Class0070Test: semaphore value incorrect\n");
        Printf(Tee::PriHigh, "               expected 0x%x, got 0x%x\n",
               semExp, semGot);
        return RC::DATA_MISMATCH;
    }

    return rc;
}

//! Release all used resources of the chip.
//!
//----------------------------------------------------------------------------
RC Class0070Test::FreeSemaphoreMemory(SEMAPHORE_TEST *sm)
{
    RC rc;
    LwRmPtr pLwRm;

    if (sm->cpuAddr) pLwRm->UnmapMemory(sm->hSemMem, sm->cpuAddr, 0, GetBoundGpuSubdevice());
    if (sm->gpuAddr) pLwRm->UnmapMemoryDma(sm->hVA,
                          sm->hSemMem,
                          DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                          sm->gpuAddr,
                          GetBoundGpuDevice());
    if (sm->hVA)    pLwRm->Free(sm->hVA);
    if (sm->hDynMem) pLwRm->Free(sm->hDynMem);
    if (sm->hSemMem) pLwRm->Free(sm->hSemMem);
    if (sm->hObj)    pLwRm->Free(sm->hObj);

    if (sm->pChan)   CHECK_RC(m_TestConfig.FreeChannel(sm->pChan));
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ Class0070Test
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(Class0070Test, RmTest,
                 "Class0070 RM test.");
