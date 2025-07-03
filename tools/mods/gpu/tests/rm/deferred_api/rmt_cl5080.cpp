/*
* Lwidia_COPYRIGHT_BEGIN
*
* Copyright 2013-2014,2019 by LWPU Corporation. All rights reserved.
* All information contained herein is proprietary and confidential
* to LWPU Corporation. Any use, reproduction, or disclosure without
* the written permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_cl5080.cpp
//! \brief Test for deferred api
//!
//! This tests is used to perform a TLB ilwalidate via the deferred api call.
//! Deferred api mechanism allows to register a task, which can be triggered later via a SW channel method.
//

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/tests/rmtest.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tasker.h"
#include "gpu/tests/rm/utility/rmtestutils.h"

#include "ctrl/ctrl5080.h"
#include "class/cl5080.h"
#include "class/cl90f1.h"

#include "core/include/memcheck.h"

#define DEFERRED_API_TEST_API_HANDLE 0xBEEF5080
#define DEFERRED_API_TEST_SUBCH_DEFERAPI 0x5

class Class5080Test : public RmTest
{
public:
    Class5080Test();
    virtual ~Class5080Test();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC allocPhysicalBuffer(UINT64);
    RC allocVirtualBuffer(UINT64);
    RC TestTlbIlwalidate();

    LwRmPtr                  pLwRm;
    LwRm::Handle             m_hDeferredApiHandle, m_hNotifyEvent, m_hVirtMem, m_hPhysMem;
    ModsEvent               *m_pModsEvent;
    Platform::SimulationMode simMode;
    FLOAT64                  m_timeoutMs;
};

//! \brief Class Constructor
//!
//! Indicate the name of the test.
//!
//! \sa Setup
//-----------------------------------------------------------------
Class5080Test::Class5080Test() :
    m_hDeferredApiHandle(0),
    m_hNotifyEvent(0),
    m_hVirtMem(0),
    m_hPhysMem(0),
    m_pModsEvent(nullptr),
    simMode(Platform::Hardware),
    m_timeoutMs(Tasker::NO_TIMEOUT)
{
    SetName("Class5080Test");
}

//! \brief Class Destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------
Class5080Test::~Class5080Test()
{
}

//! \brief Whether the test is supported in current environment
//!
//! Test should run only on GPUs with MMU, thus a check for PAGING model.
//!
//! \return True if the test can be run in current environment
//!         False otherwise
//-------------------------------------------------------------------
string Class5080Test::IsTestSupported()
{
    if (IsClassSupported(FERMI_VASPACE_A))
    {
        if (IsClassSupported(LW50_DEFERRED_API_CLASS))
        {
            return RUN_RMTEST_TRUE;
        }
        else
        {
            return "LW50_DEFERRED_API_CLASS is not supported on the current platform";
        }
    }
    return "Class5080Test is not supported on the current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup internal state for the test.
//!
//! \return OK if there are no errors, or an error code.
//-------------------------------------------------------------------
RC Class5080Test::Setup()
{
    RC                  rc;
    void               *pEventAddr;
    LW5080_ALLOC_PARAMS allocParams = {LW_TRUE};

    CHECK_RC(InitFromJs());

    m_timeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));
    m_pCh->SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(true);
    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hDeferredApiHandle, LW50_DEFERRED_API_CLASS, &allocParams));
    CHECK_RC(m_pCh->SetObject(DEFERRED_API_TEST_SUBCH_DEFERAPI, m_hDeferredApiHandle));

    // Caches & TLBs are modelled only on Silicon & RTL.
    // So we verify that the TLB ilwalidations happened correctly only on those platforms.
    simMode = Platform::GetSimulationMode();

    if (simMode != Platform::Hardware)
    {
        m_pModsEvent = Tasker::AllocEvent();
        pEventAddr = Tasker::GetOsEvent(
                m_pModsEvent,
                pLwRm->GetClientHandle(),
                pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

        //Associcate Event to Object
        CHECK_RC(pLwRm->AllocEvent(m_hDeferredApiHandle,
                                   &m_hNotifyEvent,
                                   LW01_EVENT_OS_EVENT,
                                   0,
                                   pEventAddr));
   }

    if (simMode == Platform::Hardware || simMode == Platform::RTL)
    {
        CHECK_RC(allocPhysicalBuffer(4096*2));
        CHECK_RC(allocVirtualBuffer(4096));
    }

    return rc;
}

//! \brief Run the TLB ilwalidates
//!
//!
//!
//-----------------------------------------------------------------------
RC Class5080Test::Run()
{
    RC rc;
    CHECK_RC(TestTlbIlwalidate());
    return rc;
}

RC Class5080Test::allocPhysicalBuffer(UINT64 size)
{
    RC           rc;
    UINT32       attr, attr2;

    attr = DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH)              |
           DRF_DEF(OS32, _ATTR, _DEPTH, _32)                  |
           DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1)              |
           DRF_DEF(OS32, _ATTR, _TILED, _NONE)                |
           DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE)                |
           DRF_DEF(OS32, _ATTR, _COMPR, _NONE)                |
           DRF_DEF(OS32, _ATTR, _LOCATION, _PCI)              |
           DRF_DEF(OS32, _ATTR, _PHYSICALITY, _NONCONTIGUOUS) |
           DRF_DEF(OS32, _ATTR, _COHERENCY, _UNCACHED)        |
           DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);

    attr2 = DRF_DEF(OS32, _ATTR2, _GPU_CACHEABLE, _NO);

    CHECK_RC(pLwRm->VidHeapAllocSizeEx( LWOS32_TYPE_IMAGE,
                                        LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                        LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                        &attr,
                                        &attr2, // pAttr2
                                        size,
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
    return rc;
}

RC Class5080Test::allocVirtualBuffer(UINT64 size)
{
    RC           rc;
    UINT32       attr, attr2;

    attr  = DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);
    attr2 = LWOS32_ATTR2_NONE;

    CHECK_RC(pLwRm->VidHeapAllocSizeEx( LWOS32_TYPE_IMAGE,
                                        LWOS32_ALLOC_FLAGS_VIRTUAL,
                                        &attr,
                                        &attr2, //pAttr2
                                        size, // size
                                        1,    // alignment
                                        NULL, // pFormat
                                        NULL, // pCoverage
                                        NULL, // pPartitionStride
                                        &m_hVirtMem,
                                        NULL, // poffset(fixed address)
                                        NULL, // pLimit
                                        NULL, // pRtnFree
                                        NULL, // pRtnTotal
                                        0,    // width
                                        0,    // height
                                        GetBoundGpuDevice()));

    return rc;
}

RC Class5080Test::TestTlbIlwalidate()
{
    RC                                 rc;
    UINT64                             gpuAddr   = 0;
    UINT32                            *cpuAddr   = NULL;
    LW5080_CTRL_DEFERRED_API_V2_PARAMS cmdParams = {0};

    cmdParams.hApiHandle = DEFERRED_API_TEST_API_HANDLE;
    cmdParams.cmd        = LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB;
    cmdParams.hClientVA  = pLwRm->GetClientHandle();
    cmdParams.hDeviceVA  = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    CHECK_RC(pLwRm->Control(m_hDeferredApiHandle, LW5080_CTRL_CMD_DEFERRED_API_V2,
                            &cmdParams, sizeof (cmdParams)));
    CHECK_RC(m_pCh->ScheduleChannel(true));

    if (simMode == Platform::Hardware || simMode == Platform::RTL)
    {
        CHECK_RC(pLwRm->MapMemory(m_hPhysMem, 0, 4096*2, (void **)&cpuAddr, 0, GetBoundGpuSubdevice()));
        CHECK_RC(pLwRm->MapMemoryDma(m_hVirtMem, m_hPhysMem, 0, 4096, 0, &gpuAddr, GetBoundGpuDevice()));

        Printf(Tee::PriHigh, "%s: Cpuaddr: %p\n", __FUNCTION__, cpuAddr);
        Printf(Tee::PriHigh, "%s: Gpuaddr: 0x%llx\n", __FUNCTION__, gpuAddr);

        MEM_WR32(cpuAddr, 0x0);

        CHECK_RC(m_pCh->SetSemaphoreOffset(gpuAddr));
        CHECK_RC(m_pCh->SemaphoreRelease(0x11111111));
        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_pCh->WaitIdle(m_timeoutMs));

        if (MEM_RD32(cpuAddr) != 0x11111111)
        {
            pLwRm->UnmapMemory(m_hPhysMem, cpuAddr, 0, GetBoundGpuSubdevice());
            pLwRm->UnmapMemoryDma(m_hVirtMem, m_hPhysMem, 0, gpuAddr, GetBoundGpuDevice());
            return RC::SOFTWARE_ERROR;
        }

        pLwRm->UnmapMemoryDma(m_hVirtMem, m_hPhysMem, DRF_DEF(OS47, _FLAGS, _DEFER_TLB_ILWALIDATION, _TRUE), gpuAddr, GetBoundGpuDevice());
        CHECK_RC(pLwRm->MapMemoryDma(m_hVirtMem, m_hPhysMem, 4096, 4096, DRF_DEF(OS46, _FLAGS, _DMA_OFFSET_FIXED, _TRUE) |
                                                                         DRF_DEF(OS46, _FLAGS, _DEFER_TLB_ILWALIDATION, _TRUE), &gpuAddr,
                                                                         GetBoundGpuDevice()));

        MEM_WR32(cpuAddr + 4096/sizeof(UINT32), 0x22223333);

        CHECK_RC(m_pCh->SetSemaphoreOffset(gpuAddr));
        CHECK_RC(m_pCh->SemaphoreRelease(0x33334444));
        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_pCh->WaitIdle(m_timeoutMs));

        // We mapped the memory to GPU without flushing the GMMU TLB.
        // So GPU should be using the old translation to update the the GMMU VA.
        if ((MEM_RD32(cpuAddr + 4096/sizeof(UINT32)) == 0x33334444) ||
            (MEM_RD32(cpuAddr) != 0x33334444))
        {
            pLwRm->UnmapMemory(m_hPhysMem, cpuAddr, 0, GetBoundGpuSubdevice());
            pLwRm->UnmapMemoryDma(m_hVirtMem, m_hPhysMem, 0, gpuAddr, GetBoundGpuDevice());
            return RC::SOFTWARE_ERROR;
        }
    }

    // Launch deferred api to flush GMMU TLB
    CHECK_RC(m_pCh->Write(DEFERRED_API_TEST_SUBCH_DEFERAPI, LW5080_DEFERRED_API, DEFERRED_API_TEST_API_HANDLE));
    CHECK_RC(m_pCh->Flush());
    // Wait for the deferred api completion event.
    if (simMode == Platform::Hardware)
    {
        // On Mfg MODS, interrupts are diabled while running RM Test. So rely on WaitIdle().
        CHECK_RC(m_pCh->WaitIdle(m_timeoutMs));
    }
    else
    {
        // On simulation, we rely on the deferred api event.
        CHECK_RC(Tasker::WaitOnEvent(m_pModsEvent, m_timeoutMs));
    }

    if (simMode == Platform::Hardware || simMode == Platform::RTL)
    {
        CHECK_RC(m_pCh->SetSemaphoreOffset(gpuAddr));
        CHECK_RC(m_pCh->SemaphoreRelease(0x5555666));
        CHECK_RC(m_pCh->Flush());
        CHECK_RC(m_pCh->WaitIdle(m_timeoutMs));

        // Since we flushed the TLB via deferred api, GPU should update the correct memory now.
        if (MEM_RD32(cpuAddr + 4096/sizeof(UINT32)) != 0x5555666)
        {
            rc = RC::SOFTWARE_ERROR;
        }

        pLwRm->UnmapMemory(m_hPhysMem, cpuAddr, 0, GetBoundGpuSubdevice());
        pLwRm->UnmapMemoryDma(m_hVirtMem, m_hPhysMem, 0, gpuAddr, GetBoundGpuDevice());
    }

    return rc;
}

//! \brief Cleans up after the test.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//---------------------------------------------------------------------------
RC Class5080Test::Cleanup()
{
    RC rc;

    if (m_hNotifyEvent)
        pLwRm->Free(m_hNotifyEvent);

    if (m_pModsEvent)
        Tasker::FreeEvent(m_pModsEvent);

    if (m_hDeferredApiHandle)
        pLwRm->Free(m_hDeferredApiHandle);

    if (m_hVirtMem)
        pLwRm->Free(m_hVirtMem);

    if (m_hPhysMem)
        pLwRm->Free(m_hPhysMem);

    m_TestConfig.FreeChannel(m_pCh);

    return rc;
}

//---------------------------------------------------------------------------
// JS Linkage
//---------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//!
//---------------------------------------------------------------------------
JS_CLASS_INHERIT(Class5080Test, RmTest,
                 "Class5080Test RMTest");
