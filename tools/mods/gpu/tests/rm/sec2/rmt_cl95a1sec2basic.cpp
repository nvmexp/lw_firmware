/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class95a1Sec2Basic test.
// Tests Sec2 using basic semaphore methods
//

#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl95a1.h" // LW95A1_TSEC

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "core/include/memcheck.h"

#define SEC2_SEMAPHORE_INITIAL_VALUE 0x8765ABCD
#define SEC2_SEMAPHORE_RELEASE_VALUE 0x1234FEDC

class Class95a1Sec2BasicTest: public RmTest
{
public:
    Class95a1Sec2BasicTest();
    virtual ~Class95a1Sec2BasicTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    UINT64       m_gpuAddr;
    UINT32 *     m_cpuAddr;

    LwRm::Handle m_hObj;
    LwRm::Handle m_hVA;

    LwRm::Handle m_hSemMem;
    FLOAT64      m_TimeoutMs  = Tasker::NO_TIMEOUT;
    ModsEvent   *pNotifyEvent = NULL;
};

//------------------------------------------------------------------------------
Class95a1Sec2BasicTest::Class95a1Sec2BasicTest() :
    m_gpuAddr(0LL),
    m_cpuAddr(NULL),
    m_hObj(0),
    m_hVA(0),
    m_hSemMem(0)
{
    SetName("Class95a1Sec2BasicTest");
}

//------------------------------------------------------------------------------
Class95a1Sec2BasicTest::~Class95a1Sec2BasicTest()
{
}

//------------------------------------------------------------------------
string Class95a1Sec2BasicTest::IsTestSupported()
{
    if( IsClassSupported(LW95A1_TSEC) )
        return RUN_RMTEST_TRUE;
    return "LW95A1 class is not supported on current platform";
}

//------------------------------------------------------------------------------
RC Class95a1Sec2BasicTest::Setup()
{

    const UINT32                              memSize      = 0x1000;
    LwRmPtr                                   pLwRm;
    RC                                        rc;
    LwRm::Handle                              hNotifyEvent = 0;
    void                                     *pEventAddr   = NULL;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams  = {0};

    CHECK_RC(InitFromJs());

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_SEC2));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemMem,
             DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
             memSize, GetBoundGpuDevice()));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSemMem, 0, memSize,
                                 LW04_MAP_MEMORY_FLAGS_NONE, &m_gpuAddr, GetBoundGpuDevice()));
    CHECK_RC(pLwRm->MapMemory(m_hSemMem, 0, memSize, (void **)&m_cpuAddr, 0, GetBoundGpuSubdevice()));

    pNotifyEvent = Tasker::AllocEvent();
    pEventAddr = Tasker::GetOsEvent(pNotifyEvent,
                                    pLwRm->GetClientHandle(),
                                    pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    // Associate event to object
    CHECK_RC(pLwRm->AllocEvent(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                               &hNotifyEvent,
                               LW01_EVENT_OS_EVENT,
                               LW2080_NOTIFIERS_SEC2 | LW01_EVENT_NONSTALL_INTR,
                               pEventAddr));

    eventParams.event = LW2080_NOTIFIERS_SEC2;
    eventParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_SINGLE;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
             LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
             &eventParams, sizeof(eventParams)));

    return rc;
}

//------------------------------------------------------------------------
RC Class95a1Sec2BasicTest::Run()
{
    RC            rc;
    PollArguments args;

    // Initialize semaphore
    MEM_WR32(m_cpuAddr, SEC2_SEMAPHORE_INITIAL_VALUE);

    m_pCh->SetObject(0, m_hObj);

    m_pCh->Write(0, LW95A1_SEMAPHORE_A,
        DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr >> 32LL)));
    m_pCh->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(m_gpuAddr & 0xffffffffLL));
    m_pCh->Write(0, LW95A1_SEMAPHORE_C,
        DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, SEC2_SEMAPHORE_RELEASE_VALUE));
    m_pCh->Write(0, LW95A1_SEMAPHORE_D,
        DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
        DRF_DEF(95A1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
    m_pCh->Write(0, LW95A1_SEMAPHORE_D,
        DRF_DEF(95A1, _SEMAPHORE_D, _OPERATION, _TRAP));
    m_pCh->Flush();

    // Poll on the semaphore release
    args.value  = SEC2_SEMAPHORE_RELEASE_VALUE;
    args.pAddr  = m_cpuAddr;
    Printf(Tee::PriHigh, "Waiting for semaphore release...\n");
    CHECK_RC(PollVal(&args, m_TimeoutMs));
    Printf(Tee::PriHigh, "Semaphore released\n");

    // Wait to make sure SEC2 sent the SEMAPHORE_D trap notification
    Printf(Tee::PriHigh, "Waiting on SEMAPHORE_D trap notification...\n");
    CHECK_RC(Tasker::WaitOnEvent(pNotifyEvent, m_TimeoutMs));
    Printf(Tee::PriHigh, "Received SEMAPHORE_D trap notification\n");

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2BasicTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    pLwRm->UnmapMemory(m_hSemMem, m_cpuAddr, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA, m_hSemMem,
                          LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());

    pLwRm->Free(m_hVA);
    pLwRm->Free(m_hSemMem);
    pLwRm->Free(m_hObj);
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    return firstRc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2BasicTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2BasicTest, RmTest,
                 "Class95a1Sec2Basic RM test.");
