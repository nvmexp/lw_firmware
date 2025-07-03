/*
* Lwidia_COPYRIGHT_BEGIN
*
* Copyright 2007,2009-2011,2013,2019 by LWPU Corporation. All rights reserved.
* All information contained herein is proprietary and confidential
* to LWPU Corporation. Any use, reproduction, or disclosure without
* the written permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file rmt_cl9072.cpp
//! \brief Fermi GF100_DISP_SW class test
//!
//! This tests 9072 class interfaces used to set/release
//! semaphores and notifiers upon VBLANK.
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "lwos.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "core/include/channel.h"
#include "core/include/tasker.h"
#include "gpu/include/gpudev.h"
#include "core/include/display.h"

#include "class/cl9072.h"  // GF100_DISP_SW
#include "class/cl0070.h"  // LW01_MEMORY_VIRTUAL
#include "core/include/memcheck.h"

#define SW_OBJ_SUBCH 5

class Class9072Test : public RmTest
{
public:
    Class9072Test();
    virtual ~Class9072Test();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC TestNotifiers(LwNotification *pAddrCPU, UINT64 pAddrGPU);
    RC TestSemaphore(UINT32 *pAddrCPU, UINT64 addrGPU);

    RC AllocVidMemSemaphore();
    RC ClealwidMemSemaphore();

    RC AllocSysMemSemaphore();
    RC CleanSysMemSemaphore();

    RC AllocSysMemNotifiers();
    RC CleanSysMemNotifiers();

    RC AllocVidMemNotifiers();
    RC ClealwidMemNotifiers();

    LwRmPtr         pLwRm;
    LwRm::Handle    m_hVA                 = 0;

    // System Memory Semaphore handles and data
    LwRm::Handle    m_hSemSysMem         = 0;
    UINT64          semGpuAddrSysMem     = 0;
    UINT32          *pSemCpuAddrSysMem   = nullptr;

    // Video Memory Semaphore handles and data
    LwRm::Handle    m_hSemVidMem         = 0;
    UINT64          semGpuAddrVidMem     = 0;
    UINT32          *pSemCpuAddrVidMem   = nullptr;

    // System Memory Notifiers handles and data
    LwRm::Handle    m_hNotifSysMem       = 0;
    UINT64          notifGpuAddrSysMem   = 0;
    LwNotification  *pNotifCpuAddrSysMem = nullptr;

    // Video Memory Notifiers handles and data
    LwRm::Handle    m_hNotifVidMem       = 0;
    UINT64          notifGpuAddrVidMem   = 0;
    LwNotification  *pNotifCpuAddrVidMem = nullptr;

    FLOAT64         m_TimeoutMs          = Tasker::NO_TIMEOUT;

    LwRm::Handle    m_hClient            = 0;
};

const UINT32 SET_SEM_VALUE       = 0xbeef;
const UINT32 DEFAULT_SEM_VALUE   = 0xabba;
const UINT32 SLEEP_FOR_VBLANKS   = 40;

//! \brief Class Constructor
//!
//! Indicate the name of the test.
//!
//! \sa Setup
//-----------------------------------------------------------------
Class9072Test::Class9072Test()
{
    SetName("Class9072Test");
}

//! \brief Class Destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------
Class9072Test::~Class9072Test()
{

}

//! \brief Whether the test is supported in current environment
//!
//! Since the interrupt is very specific to fermi, we check for
//! for GPU version.
//!
//! \return True if the test can be run in current environment
//!         False otherwise
//-------------------------------------------------------------------
string Class9072Test::IsTestSupported()
{
    if (IsClassSupported(GF100_DISP_SW))
    {
        return RUN_RMTEST_TRUE;
    }
    else
    {
        return "Class GF100_DISP_SW not supported on current platform";
    }
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup internal state for the test.
//!
//! \return OK if there are no errors, or an error code.
//-------------------------------------------------------------------
RC Class9072Test::Setup()
{
    RC          rc;

    CHECK_RC(InitFromJs());

    // Allocate client handle
    m_hClient = pLwRm->GetClientHandle();

    m_TimeoutMs = m_TestConfig.TimeoutMs();

    //
    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    //
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    // Allocate mappable hVA
    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    AllocVidMemSemaphore();
    AllocSysMemSemaphore();

    AllocVidMemNotifiers();
    AllocSysMemNotifiers();

    return rc;
}

//! \brief Run the test for 9072
//!
//!
//! Creates 9072 class with CRT on head 0 as alloc parameter.
//! Runs semaphore and notifiers tests.
//! This is MODS test and default display values are used.
//! Otherwise - get connectedMask from supportedMask.
//! To obtain supported and connected Mask use LW04_DISPLAY_COMMON.
//!
//-----------------------------------------------------------------------
RC Class9072Test::Run()
{
    LwRm::Handle hObject = 0;
    UINT32 headId = 0;
    Display *pDisplay = GetBoundGpuDevice()->GetDisplay();
    RC rc;

    Printf(Tee::PriLow,
           "%s: 9072 test starts!\n", __FUNCTION__);

    LW9072_ALLOCATION_PARAMETERS allocParams = {0};

    //
    // Set alloc params default for CRT display on head 0
    //
    allocParams.displayMask = pDisplay->Selected();

    // Find the configuration for only one given display from selected displays
    UINT32 i;
    for (i = 0; i < 32; ++i)
    {
        if (allocParams.displayMask & (1 << i))
        {
            allocParams.displayMask = 1 << i;
            break;
        }
    }

    pDisplay->GetHead(allocParams.displayMask, &headId);
    allocParams.logicalHeadId = (LwU32)headId;

    CHECK_RC(pLwRm->Alloc(m_hCh, &hObject, GF100_DISP_SW, &allocParams));
    CHECK_RC(m_pCh->SetObject(SW_OBJ_SUBCH, hObject));
    m_pCh->Flush();

    // Sleep a little while host is working.
    Tasker::Sleep(SLEEP_FOR_VBLANKS);
    Printf(Tee::PriLow,
           "%s: set object passed!\n", __FUNCTION__);

    CHECK_RC(TestSemaphore(pSemCpuAddrSysMem, semGpuAddrSysMem));
    CHECK_RC(TestSemaphore(pSemCpuAddrVidMem, semGpuAddrVidMem));
    Printf(Tee::PriLow,
           "%s: Semaphore testing passed!\n", __FUNCTION__);

    CHECK_RC(TestNotifiers(pNotifCpuAddrVidMem, notifGpuAddrVidMem));
    CHECK_RC(TestNotifiers(pNotifCpuAddrSysMem, notifGpuAddrSysMem));
    Printf(Tee::PriLow,
               "%s: Notifier testing passed!\n", __FUNCTION__);

    pLwRm->Free(hObject);

    return rc;
}

//! \brief Cleans up after the test.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//---------------------------------------------------------------------------
RC Class9072Test::Cleanup()
{
    RC rc, firstRc;
    FIRST_RC(m_TestConfig.FreeChannel(m_pCh));

    ClealwidMemSemaphore();
    CleanSysMemSemaphore();

    ClealwidMemNotifiers();
    CleanSysMemNotifiers();

    pLwRm->Free(m_hVA);

    return firstRc;
}

//! \brief SemPollFunc: Static function
//!
//! This function is a static one used for the poll and timeout.
//! Polling on the semaphore release condition, timeout condition
//! in case the sema isn't released.
//!
//! \return TRUE if the sema released else FALSE.
//!
//! \sa Run
//-----------------------------------------------------------------------------
static bool SemPollFunc(void *pArg)
{
    UINT32 data = MEM_RD32(pArg);

    if(data == SET_SEM_VALUE)
    {
        Printf(Tee::PriLow, "%s: Sema exit Success \n", __FUNCTION__);
        return true;
    }
    else
    {
        Printf(Tee::PriDebug, "%s: Sema exit  not Success \n", __FUNCTION__);
        return false;
    }
}

//! \brief NotifPollFunc: Static function
//!
//! This function is a static one used for the poll and timeout.
//! Polling on the semaphore release condition, timeout condition
//! in case the sema isn't released.
//!
//! \return TRUE if the notifier status is set to  released else FALSE.
//!
//! \sa Run
//-----------------------------------------------------------------------------
static bool NotifPollFunc(void *pArg)
{
    UINT16 data = MEM_RD16(pArg);

    if (data == LW9072_NOTIFICATION_STATUS_DONE_SUCCESS)
    {
        Printf(Tee::PriLow, "%s: Notifier exit Success\n", __FUNCTION__);
        return true;
    }
    else
    {
        Printf(Tee::PriLow, "%s, Notifier exit not Success \n", __FUNCTION__);
        return false;
    }
}

//! \brief Runs test for semaphore - ensures that it is properly set and released.
//!
//! Sends methods to set Semaphore GPU VA, Release Value, Present Interval and Schedule.
//! Waits for host idle and than waits 40 ms (SLEEP_FOR_VBLANKS) (about 2 VBLANKS for 60HZ display) for VBLANK.
//! Reads value from cpu mapped Semaphore Address and compares it to ReleaseValue
//!
RC Class9072Test::TestSemaphore(UINT32 *pAddrCPU, UINT64 addrGPU)
{
    RC rc;
    UINT32 Hi, Lo;

    MEM_WR32(pAddrCPU, DEFAULT_SEM_VALUE);

    Hi = (UINT32)(addrGPU >> 32LL);
    Lo = (UINT32)(addrGPU & 0xffffffffLL);

    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_SET_REPORT_SEMAPHORE_HI, Hi));
    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_SET_REPORT_SEMAPHORE_LO, Lo));
    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_SET_REPORT_SEMAPHORE_RELEASE, SET_SEM_VALUE));
    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_SET_PRESENT_INTERVAL, 1));
    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_SET_REPORT_SEMAPHORE_SCHED, 0));

    m_pCh->Flush();

    // wait a little for VBLANK to happen
    Tasker::Sleep(SLEEP_FOR_VBLANKS);

    rc = POLLWRAP(&SemPollFunc, pAddrCPU, m_TestConfig.TimeoutMs());

    // Return error on timeout
    if (RC::TIMEOUT_ERROR == rc)
    {
        rc = RC::NOTIFIER_TIMEOUT;
        Printf(Tee::PriLow, "%s: Timeout error: semaphore didn't change value\n", __FUNCTION__);
    }
    else if (OK == rc)
    {
        Printf(Tee::PriLow, "%s: passed\n", __FUNCTION__);
        rc = m_pCh->CheckError();
    }

    return rc;
}

//! \brief Runs test for notifiers - ensures that notifier is properly released.
//!
//! Sends methods to set Notifiers GPU VA and sets VBLANK info.
//! Waits for host idle and than waits 40 ms (SLEEP_FOR_VBLANKS) (about 2 VBLANKS for 60HZ display) for VBLANK.
//! Reads value from cpu mapped Notifier Address and compares it to initial value.
//!
RC Class9072Test::TestNotifiers(LwNotification *pAddrCPU, UINT64 pAddrGPU)
{
    RC rc;
    UINT32 Hi, Lo;
    LwNotification *Notif = &pAddrCPU[LW9072_NOTIFIERS_NOTIFY_ON_VBLANK];

    MEM_WR16(&(Notif->status),
            LW9072_NOTIFICATION_STATUS_ERROR_STATE_IN_USE);

    Hi = (UINT32)(pAddrGPU >> 32LL);
    Lo = (UINT32)(pAddrGPU & 0xffffffffLL);
    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_SET_NOTIFY_HI, Hi));
    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_SET_NOTIFY_LO, Lo));
    CHECK_RC(m_pCh->Write(SW_OBJ_SUBCH, LW9072_NOTIFY_ON_VBLANK, 0));

    m_pCh->Flush();

    // wait a little for VBLANK to happen
    Tasker::Sleep(SLEEP_FOR_VBLANKS);

    rc = POLLWRAP(&NotifPollFunc, &(Notif->status), m_TestConfig.TimeoutMs());

    // Return error on timeout
    if (RC::TIMEOUT_ERROR == rc)
    {
        rc = RC::NOTIFIER_TIMEOUT;
        Printf(Tee::PriLow, "%s: Timeout error: notifier didn't change value\n", __FUNCTION__);
    }
    else if (OK == rc)
    {
        Printf(Tee::PriLow, "%s: passed!\n", __FUNCTION__);
        rc = m_pCh->CheckError();
    }

    return rc;
}

//! \brief Allocates handler and pointers for Semaphore in Video Memory
//!
//! Allocates memory for Semaphore in Video Memory.
//! Provides GPU address and CPU address for later use in TestSemaphore.
//---------------------------------------------------------------------------
RC Class9072Test::AllocVidMemSemaphore()
{
    RC rc;
    UINT64      Offset;

    UINT32 size = sizeof(LwGpuSemaphore);
    const UINT32 flags = (DRF_DEF(OS32, _ATTR, _LOCATION,  _VIDMEM ) |
                          DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB));

    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE, flags,
                                     LWOS32_ATTR2_NONE, size,
                                     &m_hSemVidMem, &Offset,
                                     nullptr, nullptr, nullptr,
                                     GetBoundGpuDevice()));

    // Map memory
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSemVidMem,
                                 0, size,
                                 DRF_DEF(OS46, _FLAGS, _KERNEL_MAPPING, _ENABLE),
                                 &semGpuAddrVidMem, GetBoundGpuDevice()));

    // map the Semaphore CPU address to memory.
    CHECK_RC(pLwRm->MapMemory(m_hSemVidMem, 0, size, (void **)&pSemCpuAddrVidMem, 0, GetBoundGpuSubdevice()));

    return rc;
}

//! \brief Unmaps memory and frees handlers for Semaphore in Video Memory
//!
//! Cleans up memory mappings and handlers allocations in reversed order to their allocation
//---------------------------------------------------------------------------
RC Class9072Test::ClealwidMemSemaphore()
{
    RC rc;

    pLwRm->UnmapMemory(m_hSemVidMem, pSemCpuAddrVidMem, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA, m_hSemVidMem,
            LW04_MAP_MEMORY_FLAGS_NONE, semGpuAddrVidMem, GetBoundGpuDevice());
    pLwRm->Free(m_hSemVidMem);

    return rc;
}

//! \brief Allocates handler and pointers for Semaphore in System Memory
//!
//! Allocates memory for Semaphore in System Memory.
//! Provides GPU address and CPU address for later use in TestSemaphore.
//---------------------------------------------------------------------------
RC Class9072Test::AllocSysMemSemaphore()
{
    RC rc;
    UINT32 size = sizeof(LwGpuSemaphore);

    const UINT32 flags = (DRF_DEF(OS02, _FLAGS, _LOCATION,    _PCI) |
                          DRF_DEF(OS02, _FLAGS, _COHERENCY,   _CACHED) |
                          DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS));

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemSysMem, flags, size, GetBoundGpuDevice()));

    // Map memory
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSemSysMem,
                                 0, size,
                                 DRF_DEF(OS46, _FLAGS, _KERNEL_MAPPING, _ENABLE),
                                 &semGpuAddrSysMem, GetBoundGpuDevice()));

    // map the Semaphore CPU address to memory.
     CHECK_RC(pLwRm->MapMemory(m_hSemSysMem, 0, size, (void **)&pSemCpuAddrSysMem, 0, GetBoundGpuSubdevice()));

     return rc;
}

//! \brief Unmaps memory and frees handlers for Semaphore in System Memory
//!
//! Cleans up memory mappings and handlers allocations in reversed order to their allocation
//---------------------------------------------------------------------------
RC Class9072Test::CleanSysMemSemaphore()
{
    RC rc;

    pLwRm->UnmapMemory(m_hSemSysMem, pSemCpuAddrSysMem, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA, m_hSemSysMem,
            LW04_MAP_MEMORY_FLAGS_NONE, semGpuAddrSysMem, GetBoundGpuDevice());
    pLwRm->Free(m_hSemSysMem);

    return rc;
}

//! \brief Allocates handler and pointers for Notifiers in Video Memory
//!
//! Allocates memory for Notifiers in Video Memory.
//! Provides GPU address and CPU address for later use in TestNotifiers.
//---------------------------------------------------------------------------
RC Class9072Test::AllocVidMemNotifiers()
{
    RC rc;
    UINT64      Offset;

    UINT32 size = sizeof(LwNotification) * LW9072_NOTIFIERS_MAXCOUNT;
    const UINT32 flags = DRF_DEF(OS32, _ATTR, _LOCATION, _VIDMEM) |
                         DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS) |
                         DRF_DEF(OS32, _ATTR, _PAGE_SIZE, _4KB) |
                         DRF_DEF(OS32, _ATTR, _COHERENCY, _CACHED);

    // for buffers it is important to have CONTIGUOUS memory
    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE, flags,
                                     LWOS32_ATTR2_NONE,
                                     size, &m_hNotifVidMem, &Offset,
                                     nullptr, nullptr, nullptr,
                                     GetBoundGpuDevice()));

    // Map memory to context Dma
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hNotifVidMem,
                                 0,
                                 size,
                                 DRF_DEF(OS46, _FLAGS, _KERNEL_MAPPING, _ENABLE),
                                 &notifGpuAddrVidMem, GetBoundGpuDevice()));

    // map the Notifiers CPU address to memory.
    CHECK_RC(pLwRm->MapMemory(m_hNotifVidMem, 0, size, (void **)&pNotifCpuAddrVidMem, 0, GetBoundGpuSubdevice()));

    return rc;
}
//! \brief Unmaps memory and frees handlers for Notifiers in Video Memory
//!
//! Cleans up memory mappings and handlers allocations in reversed order to their allocation
//---------------------------------------------------------------------------
RC Class9072Test::ClealwidMemNotifiers()
{
    RC rc;

    pLwRm->UnmapMemory(m_hNotifVidMem, pNotifCpuAddrVidMem, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA, m_hNotifVidMem,
            LW04_MAP_MEMORY_FLAGS_NONE, notifGpuAddrVidMem, GetBoundGpuDevice());
    pLwRm->Free(m_hNotifVidMem);

    return rc;
}

//! \brief Allocates handler and pointers for Notifiers in System Memory
//!
//! Allocates memory for Notifiers in System Memory.
//! Provides GPU address and CPU address for later use in TestNotifiers.
//---------------------------------------------------------------------------
RC Class9072Test::AllocSysMemNotifiers()
{
    RC rc;

    UINT32 size = sizeof(LwNotification) * LW9072_NOTIFIERS_MAXCOUNT;

    // for buffers it is important to have CONTIGUOUS memory
    const UINT32 flags = (DRF_DEF(OS02, _FLAGS, _LOCATION,    _PCI       ) |
                          DRF_DEF(OS02, _FLAGS, _COHERENCY,   _CACHED) |
                          DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _CONTIGUOUS));

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hNotifSysMem, flags, size, GetBoundGpuDevice()));

    // Map memory to context Dma
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hNotifSysMem,
                                 0,
                                 size,
                                 DRF_DEF(OS46, _FLAGS, _KERNEL_MAPPING, _ENABLE),
                                 &notifGpuAddrSysMem, GetBoundGpuDevice()));

    // map the Notifiers CPU address to memory.
    CHECK_RC(pLwRm->MapMemory(m_hNotifSysMem, 0, size, (void **)&pNotifCpuAddrSysMem, 0, GetBoundGpuSubdevice()));

    return rc;
}

//! \brief Unmaps memory and frees handlers for Notifiers in System Memory
//!
//! Cleans up memory mappings and handlers allocations in reversed order to their allocation
//---------------------------------------------------------------------------
RC Class9072Test::CleanSysMemNotifiers()
{
    RC rc;

    pLwRm->UnmapMemory(m_hNotifSysMem, pNotifCpuAddrSysMem, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemoryDma(m_hVA, m_hNotifSysMem,
            LW04_MAP_MEMORY_FLAGS_NONE, notifGpuAddrSysMem, GetBoundGpuDevice());
    pLwRm->Free(m_hNotifSysMem);

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
JS_CLASS_INHERIT(Class9072Test, RmTest,
                 "Class9072Test RMTest");

