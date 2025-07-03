/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// RM test to test OFA class
//
//
//!
//! \file rmt_ofa.cpp
//! \brief RM test to test OFA class

//!
//! This file tests OFA. The test allocates an object of the class, and tries to
//! write a value to a memory and read it back to make sure it went through. It
//! also checks that ofa was able to notify the CPU about the semaphore release
//! using a nonstall interrupt
//!

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/clc6fa.h" // LWC6FA_VIDEO_OFA
#include "class/clc7fa.h" // LWC7FA_VIDEO_OFA
#include "class/clb8fa.h" // LWB8FA_VIDEO_OFA

struct PollArgs
{
    UINT32 *pollCpuAddr;
    UINT32 value;
};

//! \brief Poll Function. Explicit function for polling
//!
//! Designed so as to remove the need for waitidle
//-----------------------------------------------------------------------------
static bool Poll(void *pArgs)
{
    UINT32 val;
    PollArgs Args = *(PollArgs *)pArgs;

    val = MEM_RD32(Args.pollCpuAddr);

    if (Args.value == val)
    {
        return true;
    }
    return false;
}

class OfaTest: public RmTest
{
public:
    OfaTest();
    RC B8FATest(UINT32 &);
    virtual ~OfaTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(TestRc,     bool);
private:
    UINT64       m_gpuAddr;
    UINT32 *     m_cpuAddr;
    bool         m_TestRc;

    LwRm::Handle m_hObj;

    LwRm::Handle m_hSemMem;
    LwRm::Handle m_hVA;
    FLOAT64      m_TimeoutMs  = Tasker::NO_TIMEOUT;

    ModsEvent   *pNotifyEvent = NULL;

    GrClassAllocator *m_classAllocator;
};

//! \brief OfaTest constructor.
//!
//! \sa Setup
//------------------------------------------------------------------------------
OfaTest::OfaTest()
{
    m_gpuAddr = 0LL;
    m_cpuAddr = NULL;
    m_hObj    = 0;
    m_hSemMem = 0;
    m_hVA     = 0;
    m_TestRc  = false;

    m_classAllocator = new OfaAlloc();

    SetName("OfaTest");
}

//! \brief OfaTest destructor.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
OfaTest::~OfaTest()
{
    delete m_classAllocator;
}

//! \brief Make sure the class we're testing is supported by RM.
//!
//! \return True
//------------------------------------------------------------------------------
string OfaTest::IsTestSupported()
{
    GpuDevice *pGpuDev = GetBoundGpuDevice();

    if (m_classAllocator->IsSupported(pGpuDev))
    {
        Printf(Tee::PriHigh,"Test is supported on the current GPU/platform, preparing to run...\n");
    }
    else
    {
        switch(m_classAllocator->GetClass())
        {
            case LWB8FA_VIDEO_OFA:
                return "Class LWB8FA_VIDEO_OFA Not supported on GPU";
            case LWC6FA_VIDEO_OFA:
                return "Class LWC6FA_VIDEO_OFA Not supported on GPU";
            case LWC7FA_VIDEO_OFA:
                return "Class LWC7FA_VIDEO_OFA Not supported on GPU"; 
        }
        return "Invalid Class Specified";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//------------------------------------------------------------------------------
RC OfaTest::Setup()
{
    RC                                        rc;
    const UINT32                              memSize = 0x1000;
    LwRmPtr                                   pLwRm;
    GpuDevice                                *pGpuDev = GetBoundGpuDevice();
    LwRm::Handle                              hNotifyEvent = 0;
    void                                     *pEventAddr   = NULL;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams  = {0};

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemMem,
             DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
             memSize, GetBoundGpuDevice()));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    // Allocate channel
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_OFA));

    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSemMem, 0, memSize,
                                 LW04_MAP_MEMORY_FLAGS_NONE, &m_gpuAddr, GetBoundGpuDevice()));
    CHECK_RC(pLwRm->MapMemory(m_hSemMem, 0, memSize, (void **)&m_cpuAddr, 0, GetBoundGpuSubdevice()));

    CHECK_RC(m_classAllocator->Alloc(m_hCh, pGpuDev));
    m_hObj = m_classAllocator->GetHandle();

    // Allocate event
    pNotifyEvent = Tasker::AllocEvent();
    pEventAddr = Tasker::GetOsEvent(pNotifyEvent,
                                    pLwRm->GetClientHandle(),
                                    pLwRm->GetDeviceHandle(pGpuDev));

    // Associate event to object
    CHECK_RC(pLwRm->AllocEvent(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                               &hNotifyEvent,
                               LW01_EVENT_OS_EVENT,
                               LW2080_NOTIFIERS_OFA | LW01_EVENT_NONSTALL_INTR,
                               pEventAddr));

    eventParams.event  = LW2080_NOTIFIERS_OFA;
    eventParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_SINGLE;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
             LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
             &eventParams, sizeof(eventParams)));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief B8FATest test by writing different value to the same memory through
//!        GPU mapping for B8FA class.
//!
//! \return void
//-----------------------------------------------------------------------------
RC OfaTest::B8FATest(UINT32 &subch)
{
    if (m_TestRc)
    {
        RC errorRc;
        RC expectedRc = RC::RM_RCH_FIFO_ERROR_MMU_ERR_FLT;

        // avoid test failure due to MMU fault interrupt
        ErrorLogger::IgnoreErrorsForThisTest();

        // send invalid semaphore address generating MMU fault
        m_pCh->Write(subch, LWB8FA_SEMAPHORE_A,
                        DRF_NUM(B8FA, _SEMAPHORE_A, _UPPER, (UINT32)(0)));
        m_pCh->Write(subch, LWB8FA_SEMAPHORE_B, (UINT32)(0));
        m_pCh->Write(subch, LWB8FA_SEMAPHORE_C,
                        DRF_NUM(B8FA, _SEMAPHORE_C, _PAYLOAD, 0xdeaddead));
        m_pCh->Write(subch, LWB8FA_SEMAPHORE_D,
                        DRF_DEF(B8FA, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                        DRF_DEF(B8FA, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
        m_pCh->Write(subch, LWB8FA_SEMAPHORE_D,
                        DRF_DEF(B8FA, _SEMAPHORE_D, _OPERATION, _TRAP));

        m_pCh->Flush();
        m_pCh->WaitIdle(m_TimeoutMs);

        errorRc = m_pCh->CheckError();

        Printf(Tee::PriHigh,
            "%s: %s RC error oclwrred on OFA in RC test: %s\n",
            __FUNCTION__, (errorRc == expectedRc) ? "Expected" : "Unexpected",
            errorRc.Message());

        if (errorRc == expectedRc)
        {
            m_pCh->ClearError();
        }
        else
        {
            return errorRc;
        }
    }
    m_pCh->Write(subch, LWB8FA_SEMAPHORE_A,
                    DRF_NUM(B8FA, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr >> 32LL)));
    m_pCh->Write(subch, LWB8FA_SEMAPHORE_B, (UINT32)(m_gpuAddr & 0xffffffffLL));
    m_pCh->Write(subch, LWB8FA_SEMAPHORE_C,
                    DRF_NUM(B8FA, _SEMAPHORE_C, _PAYLOAD, 0x12345678));
    m_pCh->Write(subch, LWB8FA_SEMAPHORE_D,
                    DRF_DEF(B8FA, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                    DRF_DEF(B8FA, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
    m_pCh->Write(subch, LWB8FA_SEMAPHORE_D,
                    DRF_DEF(B8FA, _SEMAPHORE_D, _OPERATION, _TRAP));

    m_pCh->Flush();
    return RC::OK;
}

//! \brief Run test by writing to a memory and reading it back to confirm.
//!
//! \return OK if the test passed, test-specific RC if it failed
//------------------------------------------------------------------------
RC OfaTest::Run()
{
    RC     rc;
    UINT32 semVal;
    PollArgs args;
    UINT32 subch = 0;

    ErrorLogger::StartingTest();

    //
    // Write a value to memory through CPU mapping
    //
    MEM_WR32(m_cpuAddr, 0x87654321);

    m_pCh->SetObject(subch, m_hObj);

    switch(m_classAllocator->GetClass())
    {
        case LWB8FA_VIDEO_OFA:
        case LWC6FA_VIDEO_OFA:
        case LWC7FA_VIDEO_OFA:
        {
            CHECK_RC(B8FATest(subch));
        }
        break;

        default:
        {
            Printf(Tee::PriHigh,"Class 0x%x not supported.. skipping the  Semaphore Exelwtion\n",
                        m_classAllocator->GetClass());
            return RC::LWRM_ILWALID_CLASS;
        }
    }

    args.value = (0x12345678);
    args.pollCpuAddr = m_cpuAddr;

    Printf(Tee::PriHigh, "Waiting for semaphore release...\n");
    rc = POLLWRAP(&Poll, &args, m_TimeoutMs);
    Printf(Tee::PriHigh, "Semaphore released\n");

    //
    // Read a value from memory through CPU mapping
    //
    semVal = MEM_RD32(m_cpuAddr);

    if (rc != OK)
    {
        Printf(Tee::PriHigh,
              "OfaTest: GOT: 0x%x\n", semVal);
        Printf(Tee::PriHigh,
              "                EXPECTED: 0x%x\n", 0x12345678);
        return rc;
    }
    else
    {
        Printf(Tee::PriHigh,
              "OfaTest: Read/Write matched as expected.\n");
    }

    Printf(Tee::PriHigh, "Waiting on SEMAPHORE_D trap notification...\n");
    CHECK_RC(Tasker::WaitOnEvent(pNotifyEvent, m_TimeoutMs));
    Printf(Tee::PriHigh, "Received SEMAPHORE_D trap notification\n");

    ErrorLogger::TestCompleted();
    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC OfaTest::Cleanup()
{
    LwRmPtr pLwRm;

    pLwRm->UnmapMemory(m_hSemMem, m_cpuAddr, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA, m_hSemMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());

    if (m_hObj)
    {
        pLwRm->Free(m_hObj);
    }

    m_TestConfig.FreeChannel(m_pCh);

    pLwRm->Free(m_hVA);
    pLwRm->Free(m_hSemMem);

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ OfaTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(OfaTest, RmTest, "OFA RM test.");
CLASS_PROP_READWRITE(OfaTest, TestRc, bool,
                     "RC recovery test");
