/* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file rmt_multiva.cpp
 * @brief Test the feature of having multiple independent virtual address
 * spaces under a single device. Channels can share a vaspace or own their
 * private vaspaces. This test is consisted of two sub-tests. One tests the
 * private vaspaces allocation and binding to channels. Another tests the
 * shared vaspaces among channels.
 *
 **/

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"
#include "lwos.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl0080.h"
#include "class/cl9097.h"  // FERMI_A
#include "class/cla097.h"  // KEPLER_A
#include "class/cla197.h"  // KEPLER_B
#include "class/cla297.h"  // KEPLER_C
#include "class/cl90f1.h"  // FERMI_VASPACE_A
#include "class/cl007d.h"  // LW04_SOFTWARE_TEST
#include "core/include/memcheck.h"
#include "gpu/tests/rm/utility/rmtestutils.h"

typedef struct __semaphore_test {
    UINT64  gpuAddr;
    UINT32 *cpuAddr;
    UINT32  semExp;
    LwRm::Handle hVidMem;
} SEMAPHORE_TEST;

typedef struct __channel_test {
    LwRm::Handle  hVASpace;
    LwRm::Handle  hVirtMem;
    LwRm::Handle  hCh;
    Channel *     pCh;

    LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS getPdeInfoParams;
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getPteInfoParams;

    // To support AMODEL
    UINT32 vasId;

    SEMAPHORE_TEST sem;
} CHANNEL_TEST;

// Each channel has its own private VA space.
static CHANNEL_TEST m_privateCh[] = {
    {0, 0, 0, NULL, {0}, {0}, 0xFFFFFFFF, {0,NULL,0xDEADBEEF,0}},
    {0, 0, 0, NULL, {0}, {0}, 0xFFFFFFFF, {0,NULL,0x8BADF00D,0}}
};

// Channels sharing a single VA space
static CHANNEL_TEST m_shareCh[] = {
    {0, 0, 0, NULL, {0}, {0}, 0xFFFFFFFF, {0,NULL,0xCAFEBABE,0}},
    {0, 0, 0, NULL, {0}, {0}, 0xFFFFFFFF, {0,NULL,0xCAFED00D,0}}
};

static CHANNEL_TEST m_externalCh[] = {
    {0, 0, 0, NULL, {0}, {0}, 0xFFFFFFFF, {0,NULL,0xFACEFEED,0}}
};

typedef struct __pollArgs {
    UINT32 *cpuAddr;
    UINT32  semExp;
} POLLARGS;

static bool PollFunc_MultiVATest(void *pArg)
{
    POLLARGS *pPollArg = (POLLARGS *)pArg;
    UINT32 data = MEM_RD32(pPollArg->cpuAddr);

    if(data == pPollArg->semExp)
    {
        Printf(Tee::PriHigh, "Polling: Sema exit Success match on: 0x%x\n",data);
        return true;
    }
    else
    {
        Printf(Tee::PriHigh, "Polling: Value not matching data:0x%x expecting:0x%x\n",data,pPollArg->semExp);
        return false;
    }
}

class MultiVATest: public RmTest
{
public:
    MultiVATest();
    virtual ~MultiVATest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC PrivateChannelTest();
    RC PrivateAllocVATest();
    RC PrivateControlTest();
    RC PrivateMapMemoryTest();

    RC DoubleAllocVA(UINT64  vAddr, UINT32  size, UINT32  attr, UINT32  attr2, UINT32  hVASpace, UINT32 *pVirtMemHandle);
    RC CheckExpectedFailure(const RC& failure, const RC& rc, const char* unexpectedPassMsg);

    RC ShareChannelTest();
    RC ShareAllocVATest();
    RC ShareControlTest();
    RC ShareMapMemoryTest();

    RC ExtAllocVATest();

    UINT64  m_PrivTestVAddr;
    FLOAT64 m_timeoutMs;
    Platform::DisableBreakPoints disableBreaks;
};

/**
 * @brief MultiVATest constructor.
 *
 * The test virtual address is picked arbitrarily.
 **/
MultiVATest::MultiVATest() :
    m_PrivTestVAddr(0x40000000),
    m_timeoutMs(Tasker::NO_TIMEOUT)
{
    SetName("MultiVATest");
}

/**
 * @brief MultiVATest destructor.
 **/
MultiVATest::~MultiVATest()
{
}

/**
 * @brief Free all the channels
 *
 **/
RC MultiVATest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;
    CHANNEL_TEST   *pChTest = NULL;

    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_privateCh[i];
        FIRST_RC(m_TestConfig.FreeChannel(pChTest->pCh));
        pLwRm->Free(pChTest->hVASpace);
    }
    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_shareCh[i];
        FIRST_RC(m_TestConfig.FreeChannel(pChTest->pCh));
    }
    pLwRm->Free(m_shareCh[0].hVASpace);
    pLwRm->Free(m_externalCh[0].hVASpace);

    Printf(Tee::PriHigh, "Finish Cleaning up channels\n");
    return OK;
}

/**
 * @brief CheckExpectedFailure : utility class that checks whether an anticipated error condition was returned
 *
 * @return OK if the expected error condition was recieved
 * @return failure If no error condition was recieved
 * @return rc Of an unexpected error condition was recieved
 **/
RC MultiVATest::CheckExpectedFailure(const RC& failure, const RC& rc, const char* unexpectedPassMsg)
{
    if (rc == failure)
    {
        // failed as expected, so continue the test
        return OK;
    }
    else if ( rc == OK )
    {
        Printf(Tee::PriHigh, "%s", unexpectedPassMsg);
        return failure;
    }
    else
    {
        // failed due to some other error... Print error and return
        Printf(Tee::PriHigh, "Unexpected failure due to error = %s\n", rc.Message());
        return rc;
    }
}

/**
 * @brief Setup all necessary state before running the test.
 *
 * 1. Allocate 2 separate vaspaces and bind each of them to a different channel.
 * 2. Allocate 1 vaspace and bind it to two different channels.
 *
 **/
RC MultiVATest::Setup()
{
    LwRmPtr pLwRm;
    RC rc = OK;
    LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
    CHANNEL_TEST *pChTest = NULL;
    CHECK_RC(InitFromJs());
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    m_TestConfig.SetAllowMultipleChannels(true);
    m_timeoutMs = GetTestConfiguration()->TimeoutMs();
    LwRm::Handle hVASpace;
    LwRm::Handle hExtVASpace;

    /* Setup 1 */
    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_privateCh[i];
        Printf(Tee::PriHigh, "MultiVATest: Private channel: %d\n",i);
        CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                &(pChTest->hVASpace),
                FERMI_VASPACE_A,
                &vaParams));
        Printf(Tee::PriHigh, "MultiVATest: VASpace Handle:0x%x "
                             "error = %s\n", pChTest->hVASpace, rc.Message());
        CHECK_RC(m_TestConfig.AllocateChannel(&(pChTest->pCh),
                                              &(pChTest->hCh),
                                              nullptr,
                                              nullptr,
                                              pChTest->hVASpace,
                                              0,
                                              LW2080_ENGINE_TYPE_GR(0)));
        Printf(Tee::PriHigh, "MultiVATest: chHandle : 0x%x "
                             "error = %s\n", pChTest->hCh, rc.Message());
        //
        // Disable WaitIdle which can cause page fault because it's assuming
        // legacy default vaspace, which could be different from a channel's
        // vaspace.
        //
        (pChTest->pCh)->SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(true);
    }

    /* Setup 2 */
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
            &hVASpace,
            FERMI_VASPACE_A,
            &vaParams));
    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_shareCh[i];
        pChTest->hVASpace = hVASpace;
        CHECK_RC(m_TestConfig.AllocateChannel(&(pChTest->pCh),
                                              &(pChTest->hCh),
                                              nullptr,
                                              nullptr,
                                              pChTest->hVASpace,
                                              0,
                                              LW2080_ENGINE_TYPE_GR(0)));
        Printf(Tee::PriHigh, "MultiVATest: share chHandle : 0x%x "
                             "error = %s\n", pChTest->hCh, rc.Message());
        (pChTest->pCh)->SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(true);
    }

    /* Setup 3 */
    vaParams.flags |= LW_VASPACE_ALLOCATION_FLAGS_IS_EXTERNALLY_OWNED;
    rc = pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
            &hExtVASpace,
            FERMI_VASPACE_A,
            &vaParams);

    if (rc == OK)
    {
        m_externalCh[0].hVASpace = hExtVASpace;
    }
    else if (rc == RC::LWRM_NOT_SUPPORTED)
    {
         // AMODEL doesn't support external vas
         if(Platform::GetSimulationMode() == Platform::Amodel)
         {
             rc = OK;
         }
         else
         {
            MASSERT(0);
            CHECK_RC(rc);
         }
    }
    else
    {
        CHECK_RC(rc);
    }

    Printf(Tee::PriHigh, "Channel VA test SETUP done!\n");
    return OK;
}

/**
 * @brief Whether or not the test is supported in the current environment
 *
 * @return true if FERMI_VASPACE_A is enabled
 **/
string MultiVATest::IsTestSupported()
{
    LwRmPtr                         pLwRm;
    LwRm::Handle                    hGpuDev    = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    LW0080_CTRL_DMA_GET_CAPS_PARAMS paramsCaps = {0};

    if (!IsClassSupported(FERMI_VASPACE_A))
    {
        return "Multiple VASpaces or platform not supported";
    }

    paramsCaps.capsTblSize = LW0080_CTRL_DMA_CAPS_TBL_SIZE;
    RC rc = pLwRm->Control(hGpuDev,
                           LW0080_CTRL_CMD_DMA_GET_CAPS,
                          &paramsCaps,
                           sizeof(paramsCaps));
    MASSERT(rc == OK);
    if (rc != OK)
    {
        return "LW0080_CTRL_CMD_DMA_GET_CAPS failed! Cannot determine whether the test can run.";
    }

    if(!(LW0080_CTRL_GR_GET_CAP(paramsCaps.capsTbl,
                                LW0080_CTRL_DMA_CAPS_MULTIPLE_VA_SPACES_SUPPORTED)))
    {
        return "Multiple virtual address ranges are not supported. So skip test";
    }

    return RUN_RMTEST_TRUE;
}

/**
 * @brief Run the tests!
 *
 * @return OK if the test passed, test-specific RC if it failed
 **/
RC MultiVATest::Run()
{
    RC rc;

    CHECK_RC(PrivateChannelTest());
    CHECK_RC(ShareChannelTest());
    CHECK_RC(ExtAllocVATest());

    Printf(Tee::PriHigh, "Channel VA test COMPLETE!\n");
    return rc;
}

/**
 * @brief Tests control calls on explicit vaspaces
 *
 * @return
 **/
RC MultiVATest::PrivateControlTest()
{
    RC            rc;
    LwRmPtr       pLwRm;
    CHANNEL_TEST *pChTest = NULL;
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS *pGetPte;
    LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS *pGetPde;
    LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS params = {0};
    UINT64 pdbAddr;

    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_privateCh[i];

        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            //
            // AModel doesn't have PDs/PTs
            // Compare the vas Ids instead
            //
            params.hVASpace = pChTest->hVASpace;
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS,
                                    &params,
                                    sizeof(params)));
            pChTest->vasId = params.vaSpaceId;
        }
        else
        {
            // Get PTE info
            pGetPte = &(pChTest->getPteInfoParams);
            pGetPte->gpuAddr  = m_PrivTestVAddr;
            pGetPte->hVASpace = pChTest->hVASpace;
            pGetPte->skipVASpaceInit = true;
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                                    pGetPte,
                                    sizeof(LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS)));
            // Get PDE info
            pGetPde = &(pChTest->getPdeInfoParams);
            pGetPde->gpuAddr  = m_PrivTestVAddr;
            pGetPde->hVASpace = pChTest->hVASpace;
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_GET_PDE_INFO,
                                    pGetPde,
                                    sizeof(LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS)));
        }
    }

    // Private address spaces need to have separate PDB
    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pdbAddr = m_privateCh[i].getPdeInfoParams.pdbAddr;
        Printf(Tee::PriHigh, "Private Channel %d: PDB addr 0x%llx\n",i,pdbAddr);
        for (UINT32 k = 0; k < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); k++)
        {
            if (k==i)
                continue;
            if (Platform::GetSimulationMode() == Platform::Amodel)
            {
                MASSERT(m_privateCh[i].vasId != m_privateCh[k].vasId);
            }
            else
            {
                MASSERT(pdbAddr != m_privateCh[k].getPdeInfoParams.pdbAddr);
            }
        }
    }

    return rc;
}

/**
 * @brief Map physical memory to the new allocated private vaspaces
 *
 **/
RC MultiVATest::PrivateMapMemoryTest()
{
    RC      rc;
    LwRmPtr pLwRm;
    UINT32 semGot;
    UINT32 vidMemSize    = 4*1024;
    const  UINT64 mpAttr = DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                           DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _4KB);
    const  UINT32 phAttr = DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                           DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                           DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                           DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
                           DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
                           DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
                           DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
                           DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE) |
                           DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT) |
                           DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);
    UINT32 attr          = phAttr;
    UINT32 attr2         = LWOS32_ATTR2_NONE;
    CHANNEL_TEST   *pChTest = NULL;
    SEMAPHORE_TEST *pSem    = NULL;
    Channel * pCh;
    LwRm::Handle  hSwObj;

    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        Printf(Tee::PriHigh, "MultiVATest: **Private channel %d**\n",i);
        pChTest = &m_privateCh[i];
        pSem    = &(pChTest->sem);

        //
        // Allocate 4KB of physical memory. Map it to private vaspace.
        // Then write a unique value to the starting region of the physical memory.
        // Each private channel should read different values.
        //
        CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                           LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                           LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                           &attr,
                                           &attr2, // pAttr2
                                           vidMemSize,
                                           1,    // alignment
                                           NULL, // pFormat
                                           NULL, // pCoverage
                                           NULL, // pPartitionStride
                                           &(pSem->hVidMem),
                                           NULL, // poffset,
                                           NULL, // pLimit
                                           NULL, // pRtnFree
                                           NULL, // pRtnTotal
                                           0,    // width
                                           0,    // height
                                           GetBoundGpuDevice()));

        // Map vid mem to virtmem
        Printf(Tee::PriHigh, "MultiVATest: Map DMA begins\n");
        CHECK_RC(pLwRm->MapMemoryDma(pChTest->hVirtMem,
                                     pSem->hVidMem,
                                     0,
                                     vidMemSize,
                                     mpAttr,
                                     &(pSem->gpuAddr),
                                     GetBoundGpuDevice()));

        // MapMemory to obtain CPU pointer
        Printf(Tee::PriHigh, "MultiVATest: MapMemory to get cpu addr\n");
        CHECK_RC(pLwRm->MapMemory(pSem->hVidMem, 0, vidMemSize, (void **)&(pSem->cpuAddr), 0, GetBoundGpuSubdevice()));
        Printf(Tee::PriHigh, "MultiVATest: Cpuaddr: %p\n",pSem->cpuAddr);
        Printf(Tee::PriHigh, "MultiVATest: Gpuaddr: 0x%llx\n",pSem->gpuAddr);

        // Test vaspace by using semaphores
        // Need to have a sw object and schedule the channel.
        CHECK_RC(pLwRm->Alloc(pChTest->hCh, &hSwObj, LW04_SOFTWARE_TEST, NULL));
        pCh = pChTest->pCh;
        pCh->ScheduleChannel(true);

        MEM_WR32(pSem->cpuAddr,0);
        rc = Platform::FlushCpuWriteCombineBuffer();
        if (rc == RC::UNSUPPORTED_FUNCTION)
        {
            Printf(Tee::PriHigh, "MultiVATest: MODS Returned Unsupported function\
                                  when Platform::FlushCpuWriteCombineBuffer() was\
                                  called..\n");
            return rc;
        }

        CHECK_RC(pCh->SetSemaphoreOffset(pSem->gpuAddr));
        CHECK_RC(pCh->SemaphoreRelease(pSem->semExp));
        CHECK_RC(pCh->Flush());
        //pCh->WaitIdle(m_timeoutMs);
        //
        // Have to use POLLWRAP instead of waitidle b/c it will allocate a new
        // semaphore in a wrong vaspace which causes page fault.
        //
        POLLARGS arg = {pSem->cpuAddr,pSem->semExp};
        POLLWRAP(&PollFunc_MultiVATest, (void *)&arg, m_TestConfig.TimeoutMs());
        semGot = MEM_RD32(pSem->cpuAddr);

        if (semGot != pSem->semExp)
        {
            Printf(Tee::PriHigh, "MultiVATest: semaphore value incorrect\n");
            Printf(Tee::PriHigh, "               expected 0x%x, got 0x%x\n",
                   pSem->semExp, semGot);
            return RC::DATA_MISMATCH;
        }
        Printf(Tee::PriHigh, "MultiVATest: semaphore test done\n");
    }

    return rc;
}

/**
 * @brief Allocate a given vaspace at the same fixed address twice.
 *
 * The second alloc should fail if first alloc succeeds.
 *
 * @param vAddr the fixed virtual address to be allocated
 * @param size  the size of the vaspace to be allocated
 * @param attr  OS32_ATTR of allocation
 * @param attr2 OS32_ATTR2 of allocation
 * @param hVASpace the handle of a vaspace that will be allocated
 * @param pVirtMemHandle returned memory handle to the new allocated vaspace
 *
 * @return
 **/
RC MultiVATest::DoubleAllocVA
(
    UINT64  vAddr,
    UINT32  size,
    UINT32  attr,
    UINT32  attr2,
    UINT32  hVASpace,
    UINT32 *pVirtMemHandle
)
{
    RC      rc;
    LwRmPtr pLwRm;
    UINT32  dummyHandle = 0;

    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                   LWOS32_ALLOC_FLAGS_VIRTUAL |
                                   LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE,
                                   &attr,
                                   &attr2, //pAttr2
                                   size, // size
                                   1,    // alignment
                                   NULL, // pFormat
                                   NULL, // pCoverage
                                   NULL, // pPartitionStride
                                   pVirtMemHandle,
                                   &vAddr, // poffset(fixed address)
                                   NULL, // pLimit
                                   NULL, // pRtnFree
                                   NULL, // pRtnTotal
                                   0,    // width
                                   0,    // height
                                   GetBoundGpuDevice(),
                                   hVASpace));
    //
    // Try allocating at the same address in the same vaspace. Expected to
    // FAIL!
    //
    DISABLE_BREAK_COND(disableBreaks, true);
    rc = pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                   LWOS32_ALLOC_FLAGS_VIRTUAL |
                                   LWOS32_ALLOC_FLAGS_FIXED_ADDRESS_ALLOCATE,
                                   &attr,
                                   &attr2, //pAttr2
                                   size, // size
                                   1,    // alignment
                                   NULL, // pFormat
                                   NULL, // pCoverage
                                   NULL, // pPartitionStride
                                   &dummyHandle,
                                   &vAddr, // poffset,
                                   NULL, // pLimit
                                   NULL, // pRtnFree
                                   NULL, // pRtnTotal
                                   0,    // width
                                   0,    // height
                                   GetBoundGpuDevice(),
                                   hVASpace);
    DISABLE_BREAK_END(disableBreaks);

    CHECK_RC_CLEANUP(CheckExpectedFailure(RC::LWRM_INSUFFICIENT_RESOURCES, rc,
                     "DoubleAllocVA: trying to allocate virtual memory at the"
                     "virtual address that has alredy been allocated."
                     "This should cause FAILURE!\n"));
    if (rc == OK)
        return rc;
Cleanup:
    pLwRm->Free(*pVirtMemHandle);
    return rc;
}

/**
 * @brief Tests if 2 vaspace objects truly have separate vaspaces.
 *
 * If they do have 2 separate vaspaces, then allocation at the same fixed
 * address should succeed for different vaspace handle.
 *
 * @return
 **/
RC MultiVATest::PrivateAllocVATest()
{
    Printf(Tee::PriHigh, "Private Alloc Test\n");
    RC      rc;
    // Allocate 4KB of virtual memory at the test address.
    UINT32 size   = 4*1024;
    UINT32 attr   = DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB) |
                    DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    UINT32 attr2  = LWOS32_ATTR2_NONE;
    CHANNEL_TEST *pChTest = NULL;

    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_privateCh[i];
        CHECK_RC(DoubleAllocVA(m_PrivTestVAddr, size, attr, attr2,
                               pChTest->hVASpace,
                               &(pChTest->hVirtMem)));
        Printf(Tee::PriHigh, "Private Channel %d: virtmem handle 0x%x\n",i,pChTest->hVirtMem);
    }

    return rc;
}

RC MultiVATest::PrivateChannelTest()
{
    RC rc;
    LwRmPtr pLwRm;
    CHANNEL_TEST   *pChTest = NULL;
    SEMAPHORE_TEST *pSem    = NULL;

    CHECK_RC_CLEANUP(PrivateAllocVATest());
    CHECK_RC_CLEANUP(PrivateMapMemoryTest());
    CHECK_RC_CLEANUP(PrivateControlTest());

Cleanup:
    Printf(Tee::PriHigh, "Cleaning allocated resources\n");
    // Clean up mapping
    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_privateCh[i];
        pSem    = &(pChTest->sem);

        if (pSem != NULL)
        {
            Printf(Tee::PriHigh, "ID: %d. Sem Unmap Memory and MemoryDma\n",i);
            if (pSem->cpuAddr !=NULL)
                pLwRm->UnmapMemory(pSem->hVidMem, pSem->cpuAddr, 0, GetBoundGpuSubdevice());
            if (pSem->gpuAddr != 0)
            {
                pLwRm->UnmapMemoryDma(pChTest->hVirtMem, pSem->hVidMem,
                                      LW04_MAP_MEMORY_FLAGS_NONE, pSem->gpuAddr, GetBoundGpuDevice());
            }
        }
    }

    // Free allocated memory and objects
    for (UINT32 i = 0; i < (sizeof(m_privateCh)/sizeof(CHANNEL_TEST)); i++)
    {
        Printf(Tee::PriHigh, "ID: %d. Unmap Memory and MemoryDma\n",i);
        pChTest = &m_privateCh[i];
        pSem    = &(pChTest->sem);
        if (pSem != NULL)
        {
            if (pSem->hVidMem != 0)
                pLwRm->Free(pSem->hVidMem);
        }
        if (pChTest->hVirtMem != 0)
            pLwRm->Free(pChTest->hVirtMem);
    }
    Printf(Tee::PriHigh, "Private Channel test COMPLETE!\n");
    return rc;
}

/**
 * @brief Map physical memory to the new shared private vaspaces
 *
 **/
RC MultiVATest::ShareMapMemoryTest()
{
    RC      rc;
    LwRmPtr pLwRm;
    UINT32 semGot;
    UINT32 vidMemSize    = 4*1024;
    const  UINT64 mpAttr = DRF_DEF(OS46, _FLAGS, _ACCESS, _READ_WRITE) |
                           DRF_DEF(OS46, _FLAGS, _PAGE_SIZE, _4KB);
    const  UINT32 phAttr = DRF_DEF(OS32, _ATTR, _FORMAT, _PITCH) |
                           DRF_DEF(OS32, _ATTR, _DEPTH, _32) |
                           DRF_DEF(OS32, _ATTR, _AA_SAMPLES, _1) |
                           DRF_DEF(OS32, _ATTR, _TILED, _NONE) |
                           DRF_DEF(OS32, _ATTR, _ZLWLL, _NONE) |
                           DRF_DEF(OS32, _ATTR, _COMPR, _NONE) |
                           DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
                           DRF_DEF(OS32, _ATTR, _COHERENCY, _WRITE_COMBINE) |
                           DRF_DEF(OS32, _ATTR, _PHYSICALITY, _DEFAULT) |
                           DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB);
    UINT32 attr          = phAttr;
    UINT32 attr2         = LWOS32_ATTR2_NONE;
    CHANNEL_TEST   *pChTest = NULL;
    SEMAPHORE_TEST *pSem    = NULL;
    Channel * pCh;
    LwRm::Handle  hSwObj;

    pChTest = &m_shareCh[0];
    pSem    = &(pChTest->sem);

    //
    // Allocate 4KB of physical memory. Map it to the shared vaspace.
    // Then write a unique value to the starting region of the physical memory.
    // The second channel should read the value that the first one wrote.
    //
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(LWOS32_TYPE_IMAGE,
                                       LWOS32_ALLOC_FLAGS_NO_SCANOUT |
                                       LWOS32_ALLOC_FLAGS_MAP_NOT_REQUIRED,
                                       &attr,
                                       &attr2, // pAttr2
                                       vidMemSize,
                                       1,    // alignment
                                       NULL, // pFormat
                                       NULL, // pCoverage
                                       NULL, // pPartitionStride
                                       &(pSem->hVidMem),
                                       NULL, // poffset,
                                       NULL, // pLimit
                                       NULL, // pRtnFree
                                       NULL, // pRtnTotal
                                       0,    // width
                                       0,    // height
                                       GetBoundGpuDevice()));

    // Map vid mem to virtmem
    Printf(Tee::PriHigh, "MultiVATest: Map DMA begins\n");
    CHECK_RC(pLwRm->MapMemoryDma(pChTest->hVirtMem,
                                 pSem->hVidMem,
                                 0,
                                 vidMemSize,
                                 mpAttr,
                                 &(pSem->gpuAddr),
                                 GetBoundGpuDevice()));

    // MapMemory to obtain CPU pointer
    CHECK_RC(pLwRm->MapMemory(pSem->hVidMem, 0, vidMemSize, (void **)&(pSem->cpuAddr), 0, GetBoundGpuSubdevice()));
    Printf(Tee::PriHigh, "MultiVATest: Cpuaddr: %p\n",pSem->cpuAddr);
    Printf(Tee::PriHigh, "MultiVATest: Gpuaddr: 0x%llx\n",pSem->gpuAddr);

    // since share channel 1 has the same hVirtMem, they should also have the
    // same gpu/cpu addr.
    m_shareCh[1].sem.gpuAddr = pSem->gpuAddr;
    m_shareCh[1].sem.cpuAddr = pSem->cpuAddr;

    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_shareCh[i];
        pSem    = &(pChTest->sem);
        // Test vaspace by using semaphores
        // Need to have a sw object and schedule the channel.
        CHECK_RC(pLwRm->Alloc(pChTest->hCh, &hSwObj, LW04_SOFTWARE_TEST, NULL));
        pCh = pChTest->pCh;
        pCh->ScheduleChannel(true);

        MEM_WR32(pSem->cpuAddr,0);
        rc = Platform::FlushCpuWriteCombineBuffer();
        if (rc == RC::UNSUPPORTED_FUNCTION)
        {
            Printf(Tee::PriHigh, "MultiVATest: MODS Returned Unsupported function\
                                  when Platform::FlushCpuWriteCombineBuffer() was\
                                  called..\n");
            return rc;
        }

        CHECK_RC(pCh->SetSemaphoreOffset(pSem->gpuAddr));
        CHECK_RC(pCh->SemaphoreRelease(pSem->semExp));
        CHECK_RC(pCh->Flush());
        //pCh->WaitIdle(m_timeoutMs);
        //
        // Have to use POLLWRAP instead of waitidle b/c it will allocate a new
        // semaphore in a wrong vaspace which causes page fault.
        //
        POLLARGS arg = {pSem->cpuAddr,pSem->semExp};
        POLLWRAP(&PollFunc_MultiVATest, (void *)&arg, m_TestConfig.TimeoutMs());
        semGot = MEM_RD32(pSem->cpuAddr);

        if (semGot != pSem->semExp)
        {
            Printf(Tee::PriHigh, "MultiVATest: semaphore value incorrect\n");
            Printf(Tee::PriHigh, "               expected 0x%x, got 0x%x\n",
                   pSem->semExp, semGot);
            return RC::DATA_MISMATCH;
        }
    }

    //
    // Here comes the part where we really test if both channels can
    // effectively write to the same vaspace. Since channel 1 did the last
    // write, channel 0 should read the same value despite it wrote a different
    // value before.
    //
    semGot = MEM_RD32(m_shareCh[0].sem.cpuAddr);
    if (semGot != m_shareCh[1].sem.semExp)
    {
        Printf(Tee::PriHigh, "MultiVATest: semaphore value incorrect\n");
        Printf(Tee::PriHigh, "               expected 0x%x, got 0x%x\n",
               m_shareCh[1].sem.semExp, semGot);
        return RC::DATA_MISMATCH;
    }

    Printf(Tee::PriHigh, "MultiVATest: semaphore test done\n");

    return rc;
}

/**
 * @brief Tests if channels are sharing a va space
 *
 * If they do have a single vaspace, then allocation at the same fixed
 * address should fail.
 *
 * @return
 **/
RC MultiVATest::ShareAllocVATest()
{
    Printf(Tee::PriHigh, "share Alloc Test\n");
    RC      rc;
    // Allocate 4KB of virtual memory at the test address.
    UINT32 size   = 4*1024;
    UINT32 attr   = DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB) |
                    DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    UINT32 attr2  = LWOS32_ATTR2_NONE;
    CHANNEL_TEST *pChTest = NULL;

    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_shareCh[i];

        //
        // Only the first virtual page allocation at the fixed address should
        // succeed.
        //
        if (i == 0)
        {
            CHECK_RC(DoubleAllocVA(m_PrivTestVAddr, size, attr, attr2,
                                   pChTest->hVASpace,
                                   &(pChTest->hVirtMem)));
        }
        else
        {
            rc = DoubleAllocVA(m_PrivTestVAddr, size, attr, attr2,
                               pChTest->hVASpace,
                               &(pChTest->hVirtMem));
            CHECK_RC(CheckExpectedFailure(RC::LWRM_INSUFFICIENT_RESOURCES, rc,
                     "ShareAllocVATest: trying to allocate virtual memory at the"
                     "virtual address that has alredy been allocated."
                     "This should cause FAILURE!\n"));

            // Share the same virt mem
            pChTest->hVirtMem = m_shareCh[0].hVirtMem;
        }
        Printf(Tee::PriHigh, "share Channel %d: virtmem handle 0x%x\n",i,pChTest->hVirtMem);
    }

    return rc;
}

RC MultiVATest::ExtAllocVATest()
{
    RC            rc;
    CHANNEL_TEST *pChTest = &m_externalCh[0];

    if (pChTest->hVASpace == 0)
    {
        return rc;
    }

    Printf(Tee::PriHigh, "Ext Alloc Test\n");

    // Allocate 4KB of virtual memory at the test address.
    UINT32 size   = 4*1024;
    UINT32 attr   = DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB) |
                    DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM);
    UINT32 attr2  = LWOS32_ATTR2_NONE;

    rc = DoubleAllocVA(m_PrivTestVAddr, size, attr, attr2,
                                   pChTest->hVASpace,
                                   &(pChTest->hVirtMem));

    CHECK_RC(CheckExpectedFailure(RC::LWRM_INSUFFICIENT_RESOURCES, rc,
                     "ExtAllocVATest: trying to allocate virtual memory"
                     "for an externally owned vaspace."
                     "This should cause FAILURE!\n"));

    return rc;
}

/**
 * @brief Tests control calls on explicit vaspaces
 *
 * @return
 **/
RC MultiVATest::ShareControlTest()
{
    RC            rc;
    LwRmPtr       pLwRm;
    CHANNEL_TEST *pChTest = NULL;
    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS *pGetPte;
    LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS *pGetPde;
    LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS params = {0};
    UINT64 pdbAddr;

    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_shareCh[i];

        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            //
            // AModel doesn't have PDs/PTs
            // Compare the vas Ids instead
            //
            params.hVASpace = pChTest->hVASpace;
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS,
                                    &params,
                                    sizeof(params)));
            pChTest->vasId = params.vaSpaceId;
        }
        else
        {
            // Get PTE info
            pGetPte = &(pChTest->getPteInfoParams);
            pGetPte->gpuAddr  = m_PrivTestVAddr;
            pGetPte->hVASpace = pChTest->hVASpace;
            pGetPte->skipVASpaceInit = true;
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                                    pGetPte,
                                    sizeof(LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS)));
            // Get PDE info
            pGetPde = &(pChTest->getPdeInfoParams);
            pGetPde->gpuAddr  = m_PrivTestVAddr;
            pGetPde->hVASpace = pChTest->hVASpace;
            CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                                    LW0080_CTRL_CMD_DMA_GET_PDE_INFO,
                                    pGetPde,
                                    sizeof(LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS)));
        }
    }

    // share address spaces need to have the same PDB
    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pdbAddr = m_shareCh[i].getPdeInfoParams.pdbAddr;
        Printf(Tee::PriHigh, "share Channel %d: PDB addr 0x%llx\n",i,pdbAddr);
        for (UINT32 k = 0; k < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); k++)
        {
            if (k==i)
                continue;

            if (Platform::GetSimulationMode() == Platform::Amodel)
            {
                MASSERT(m_shareCh[i].vasId == m_shareCh[k].vasId);
            }
            else
            {
                MASSERT(pdbAddr == m_shareCh[k].getPdeInfoParams.pdbAddr);
            }
        }
    }

    return rc;
}

RC MultiVATest::ShareChannelTest()
{
    RC rc;
    LwRmPtr pLwRm;
    CHANNEL_TEST   *pChTest = NULL;
    SEMAPHORE_TEST *pSem    = NULL;

    CHECK_RC_CLEANUP(ShareAllocVATest());
    CHECK_RC_CLEANUP(ShareMapMemoryTest());
    CHECK_RC_CLEANUP(ShareControlTest());

Cleanup:
    Printf(Tee::PriHigh, "Cleaning allocated resources\n");
    // Clean up mapping
    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        pChTest = &m_shareCh[i];
        pSem    = &(pChTest->sem);

        if (pSem != NULL && i==0)
        {
            Printf(Tee::PriHigh, "ID: %d. Unmap Memory and MemoryDma\n",i);
            if (pSem->cpuAddr !=NULL)
                pLwRm->UnmapMemory(pSem->hVidMem, pSem->cpuAddr, 0, GetBoundGpuSubdevice());
            if (pSem->gpuAddr != 0)
            {
                pLwRm->UnmapMemoryDma(pChTest->hVirtMem, pSem->hVidMem,
                                      LW04_MAP_MEMORY_FLAGS_NONE, pSem->gpuAddr, GetBoundGpuDevice());
            }
        }
    }

    // Free allocated memory and objects
    for (UINT32 i = 0; i < (sizeof(m_shareCh)/sizeof(CHANNEL_TEST)); i++)
    {
        Printf(Tee::PriHigh, "ID: %d. Unmap Memory and MemoryDma\n",i);
        pChTest = &m_shareCh[i];
        pSem    = &(pChTest->sem);
        if (pSem != NULL)
        {
            if (pSem->hVidMem != 0)
                pLwRm->Free(pSem->hVidMem);
        }
        // Prevent double freeing
        if ((pChTest->hVirtMem != 0 && pChTest->hVirtMem != m_shareCh[0].hVirtMem) || i==0)
            pLwRm->Free(pChTest->hVirtMem);
    }
    Printf(Tee::PriHigh, "share Channel test COMPLETE!\n");
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------
//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ MultiVATest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(MultiVATest, RmTest,
                 "MultiVATest RM test.");

