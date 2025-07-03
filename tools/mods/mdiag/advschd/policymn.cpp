/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file
//! \brief Implements the singleton that manages the PolicyManager module

#include "policymn.h"
#include <algorithm>
#include "gpu/utility/chanwmgr.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "lwRmReg.h"
#include "core/include/platform.h"
#include "pmchan.h"
#include "pmvaspace.h"
#include "pmvftest.h"
#include "pmchwrap.h"
#include "pmevent.h"
#include "pmevntmn.h"
#include "pmgilder.h"
#include "pmparser.h"
#include "pmsurf.h"
#include "mdiag/utils/mdiagsurf.h"
#include "pmtest.h"
#include "pmutils.h"
#include "core/include/regexp.h"
#include "core/include/registry.h"
#include "core/include/utility.h"
#include "class/cl90f1.h"       // FERMI_VASPACE_A
#include "ctrl/ctrl90f1.h"      // LW90F1_CTRL_VASPACE_GET_GMMU_FORMAT_PARAMS
#include "core/include/tee.h"
#include "core/include/regexp.h"
#include "core/include/argread.h"
#include "mdiag/sysspec.h"
#include "pmsmcengine.h"
#include "mdiag/lwgpu.h"
#include "mdiag/tests/conlwrrentTest.h"
#include "mdiag/utils/vaspaceutils.h"

PolicyManager *PolicyManager::m_Instance = NULL;
const FLOAT64 PolicyManager::PollWaitTimeLimit = 1000;
PolicyManagerMessageDebugger * PolicyManagerMessageDebugger::m_Instance = NULL;

//--------------------------------------------------------------------
//! \brief constructor
//!
PolicyManager::PolicyManager() :
    m_pParser(NULL),
    m_pEventMgr(NULL),
    m_pGilder(NULL),
    m_TestNum(0),
    m_RobustChannelEnabled(false),
    m_GpuResetEnabled(false),
    m_bBlockingCtxswInt(true),
    m_Mutex(NULL),
    m_DisableMutexInActions(0),
    m_RandomSeed(0),
    m_bRandomSeedSet(false),
    m_StrictModeEnabled(false),
    m_FaultBufferNeeded(false),
    m_NonReplayableFaultBufferNeeded(false),
    m_AccessCounterBufferNeeded(false),
    m_AccessCounterNotificationWaitMs(Tasker::GetDefaultTimeoutMs()),
    m_PoisonErrorNeeded(false),
    m_UpdateFaultBufferGetPointer(true),
    m_ChannelLogging(false),
    m_EnableCEFlush(true),
    m_IsInband(false),
    m_StartVfTestInPolicyMgr(false)
{
    m_pParser   = new PmParser(this);
    m_pEventMgr = new PmEventManager(this);
    m_pGilder   = new PmGilder(this);
    m_Mutex     = Tasker::AllocMutex("PolicyManager::m_Mutex",
                                     Tasker::mtxUnchecked);

    // The test mutex is needed for functions that manipulate the
    // active and inactive test lists.  Normally m_Mutex covers those
    // functions as well, but EndOrAbortTest will temoporarily release
    // m_Mutex while the event manager EndTest function is called,
    // and this can lead to issues if functions like AddTest or StartTest
    // are called during this time period.  (See bug 3325052.)
    m_TestMutex = Tasker::AllocMutex("PolicyManager::m_TestMutex",
                                     Tasker::mtxUnchecked);

    ChannelWrapperMgr::Instance()->UsePmWrapper();

}

//--------------------------------------------------------------------
//! \brief destructor
//!
PolicyManager::~PolicyManager()
{
    FreeTestData();
    delete m_pGilder;
    delete m_pEventMgr;
    delete m_pParser;
    Tasker::FreeMutex(m_Mutex);
    m_Mutex = nullptr;
    Tasker::FreeMutex(m_TestMutex);
    m_TestMutex = nullptr;
}

//--------------------------------------------------------------------
//! \brief Instance method for the PolicyManager singleton
//!
PolicyManager *PolicyManager::Instance()
{
    if (m_Instance == NULL)
    {
        m_Instance = new PolicyManager();
    }
    return m_Instance;
}

void PolicyManager::RestoreRegisters()
{
    for (auto& restore:m_RegRestoreList)
    {
        restore();
    }
}

//--------------------------------------------------------------------
//! \brief Delete the PolicyManager singleton, if it was instantiated.
//!
void PolicyManager::ShutDown()
{
    if (m_Instance != NULL)
    {
        m_Instance->RestoreRegisters();
        delete m_Instance;
        m_Instance = NULL;
    }
}
//--------------------------------------------------------------------
bool PolicyManager::HasInstance()
{
    return (NULL != m_Instance);
}
//--------------------------------------------------------------------
//! \brief Return all active channels on a GpuDevice
//!
PmChannels PolicyManager::GetActiveChannels
(
    const GpuDevice *pGpuDevice
) const
{
    PmChannels channels;
    for (PmChannels::const_iterator ppChannel = m_ActiveChannels.begin();
         ppChannel != m_ActiveChannels.end(); ++ppChannel)
    {
        if ((*ppChannel)->GetGpuDevice() == pGpuDevice)
        {
            channels.push_back(*ppChannel);
        }
    }
    return channels;
}

//--------------------------------------------------------------------
//! \brief Return the active or inactive channel with the specified handle.
//!
PmChannel * PolicyManager::GetChannel(LwRm::Handle hCh) const
{
    for (PmChannels::const_iterator ppChannel = m_ActiveChannels.begin();
         ppChannel != m_ActiveChannels.end(); ++ppChannel)
    {
        if ((*ppChannel)->GetHandle() == hCh)
        {
            return *ppChannel;
        }
    }

    for (PmChannels::const_iterator ppChannel = m_InactiveChannels.begin();
         ppChannel != m_InactiveChannels.end(); ++ppChannel)
    {
        if ((*ppChannel)->GetHandle() == hCh)
        {
            return *ppChannel;
        }
    }

    return NULL;
}

//--------------------------------------------------------------------
//! \brief Return all active subsurfaces
//!
PmSubsurfaces PolicyManager::GetActiveSubsurfaces() const
{
    PmSubsurfaces allSubsurfaces;
    for (PmSurfaces::const_iterator ppSurface = m_ActiveSurfaces.begin();
         ppSurface != m_ActiveSurfaces.end(); ++ppSurface)
    {
        PmSubsurfaces subsurfacesInSurf = (*ppSurface)->GetSubsurfaces();
        for (PmSubsurfaces::iterator ppSubsurface = subsurfacesInSurf.begin();
             ppSubsurface != subsurfacesInSurf.end(); ++ppSubsurface)
        {
            allSubsurfaces.push_back(*ppSubsurface);
        }
    }
    return allSubsurfaces;
}

//--------------------------------------------------------------------
//! \brief Return the active or inactive test that has the specified UniquePtr.
//!
//! \param pTestUniquePtr An opaque value used to uniquely identify the
//!     test.  Must be the same value returned by PmTest::GetUniquePtr().
//!
PmTest * PolicyManager::GetTestFromUniquePtr(void *pTestUniquePtr) const
{
    for (PmTests::const_iterator ppTest = m_ActiveTests.begin();
         ppTest != m_ActiveTests.end(); ++ppTest)
    {
        if ((*ppTest)->GetUniquePtr() == pTestUniquePtr)
        {
            return (*ppTest);
        }
    }
    for (PmTests::const_iterator ppTest = m_InactiveTests.begin();
         ppTest != m_InactiveTests.end(); ++ppTest)
    {
        if ((*ppTest)->GetUniquePtr() == pTestUniquePtr)
        {
            return (*ppTest);
        }
    }
    return NULL;
}

//--------------------------------------------------------------------
//! \brief Return all GpuSubdevices being tested, sorted by instance
//!
GpuSubdevices PolicyManager::GetGpuSubdevices() const
{
    //! Internally, PolicyManager keeps a list of GpuDevices, not
    //! GpuSubdevices.  This method returns all subdevices of the
    //! devices.
    //!

    GpuSubdevices gpuSubdevices;     // Return value
    for (size_t ii = 0; ii < m_GpuDevices.size(); ++ii)
    {
        GpuDevice *pGpuDevice = m_GpuDevices[ii];
        for (UINT32 jj = 0; jj < pGpuDevice->GetNumSubdevices(); ++jj)
        {
            gpuSubdevices.push_back(pGpuDevice->GetSubdevice(jj));
        }
    }
    return gpuSubdevices;
}

//--------------------------------------------------------------------
//! \brief Return true if the PolicyManager was initialized.
//!
//! "Initialized" means that the policyfile was parsed, and contained
//! at least one trigger.  If the PolicyManager isn't initialized,
//! then there's no point in anyone calling AddChannel(),
//! AddSurface(), StartTest(), or EndTest().
//!
bool PolicyManager::IsInitialized() const
{
    return m_pEventMgr->GetNumTriggers() > 0;
}

//--------------------------------------------------------------------
//! \brief Disable the mutex when actions are run
//!
//! This method causes the mutex mutex to be disabled while actions
//! are run, allowing other triggers to interrupt a running action.
//!
//! Used to implement ActionBlock.AllowOverlappingTriggers.
//!
//! This is a dangerous operation that should be used conservatively.
//!
//! \sa MutexDisabledInActions()
//!
//! \param Disable True to disable the mutex, false to re-enable it.
//!     Calls can be nested; if this method is called N times with
//!     Disable=true, it must be called N times with Disable=false in
//!     order to re-enable the mutex.
//!
void PolicyManager::DisableMutexInActions(bool Disable)
{
    if (Disable)
    {
        ++m_DisableMutexInActions;
    }
    else
    {
        MASSERT(m_DisableMutexInActions > 0);
        --m_DisableMutexInActions;
    }
}

//--------------------------------------------------------------------
//! \brief Return true if the mutex has been disabled by
//! DisableMutexInActions()
//!
bool PolicyManager::MutexDisabledInActions() const
{
    return m_DisableMutexInActions > 0;
}

//--------------------------------------------------------------------
//! \brief Starts (optionally creating) the indicated timer
//!
//! This method is used by Action.StartTimer and Trigger.OnTimerUS.
//! It merely records the time that the method was called, which can
//! be retrieved by GetTimerStartUS().  It is up to the caller to
//! compare the start-time to Platform::GetTimeUS() to determine how
//! much time has elapsed for the timer.
//!
void PolicyManager::StartTimer(const string &TimerName)
{
    m_TimerStartUS[TimerName] = Platform::GetTimeUS();
}

//--------------------------------------------------------------------
//! \brief Returns the time that StartTimer() was called
//!
UINT64 PolicyManager::GetTimerStartUS(const string &TimerName) const
{
    map<string, UINT64>::const_iterator pTimerStart =
        m_TimerStartUS.find(TimerName);
    MASSERT(pTimerStart != m_TimerStartUS.end());

    return pTimerStart->second;
}

//--------------------------------------------------------------------
//! \brief Called by tests to register themselves with PolicyManager
//!
//! PolicyManager assumes ownership of the PmTest, and deletes it when
//! EndTest() or AbortTest() is called on the last conlwrrent test.
//!
//! Due to diffilwlties with trace3d code that's called several times,
//! this routine does not consider it an error to try to add the same
//! test several times.  It simply deletes the duplicate.  Thus, the
//! caller should use GetTestFromUniquePtr() if it wants to get a
//! pointer to its PmTest.
//!
RC PolicyManager::AddTest(PmTest *pTest)
{
    MASSERT(pTest != NULL);
    RC rc;

    Tasker::MutexHolder mth(m_TestMutex);
    Tasker::MutexHolder mh(m_Mutex);

    if (GetTestFromUniquePtr(pTest->GetUniquePtr()) != NULL)
    {
        WarnPrintf("Already added test %p to PolicyManager. Discarding duplicate.\n",
                   pTest->GetUniquePtr());
        delete pTest;
        return rc;
    }
    m_InactiveTests.insert(pTest);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Called by tests to register their channels with PolicyManager
//!
//! PolicyManager assumes ownership of the PmChannel, and deletes it
//! when EndTest() or AbortTest() is called on the last conlwrrent
//! test.
//!
//! Due to diffilwlties with trace3d code that's called several times,
//! this routine does not consider it an error to try to add the same
//! channel several times.  It simply deletes the duplicate.
//!
RC PolicyManager::AddChannel(PmChannel *pChannel)
{
    MASSERT(pChannel != NULL);
    RC rc;

    Tasker::MutexHolder mh(m_Mutex);

    // Delete pChannel if it's a duplicate
    //
    if (pChannel->GetTest() && GetChannel(pChannel->GetHandle()) != NULL)
    {
        WarnPrintf("Already added channel 0x%x to PolicyManager. Discarding duplicate.\n",
                   pChannel->GetHandle());
        delete pChannel;
        return rc;
    }

    // Add the channel.
    //
    if (pChannel->GetTest())
    {
        DebugPrintf("PolicyManager: Add Test created channel '%s', handle 0x%08x, chid %u\n",
                    pChannel->GetName().c_str(), pChannel->GetHandle(),
                    pChannel->GetModsChannel()->GetChannelId());
    }
    // If PolicyManager created channel then it is possible that the
    // channel creation is deferred hence ModsChannel can be nullptr
    else
    {
        DebugPrintf("PolicyManager: Add Policy Manager created channel '%s'\n",
                    pChannel->GetName().c_str());
    }


    if (InTest(pChannel->GetTest()))
    {
        m_ActiveChannels.push_back(pChannel);
    }
    else
    {
        m_InactiveChannels.push_back(pChannel);
    }
    CHECK_RC(AddGpuDevice(pChannel->GetGpuDevice()));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Called by tests to register their surfaces with PolicyManager
//!
//! PolicyManager creates a PmSurface object for the surface, and
//! deletes it when EndTest() or AbortTest() is called on the last
//! conlwrrent test.
//!
//! Due to diffilwlties with trace3d code that's called several times,
//! this routine does not consider it an error to try to add the same
//! surface several times.
//!
RC PolicyManager::AddSurface
(
    PmTest *pTest,
    MdiagSurf *pMdiagSurf,
    bool ExternalSurf2D,
    PmSurface **ppNewSurface
)
{
    MASSERT(pMdiagSurf != NULL);
    RC rc;

    Tasker::MutexHolder mh(m_Mutex);

    // Do nothing if we've already called Add[Sub]Surface() on pMdiagSurf
    //
    for (PmSurfaces::iterator ppSurface = m_ActiveSurfaces.begin();
         ppSurface != m_ActiveSurfaces.end(); ++ppSurface)
    {
        if ((*ppSurface)->GetMdiagSurf() == pMdiagSurf)
        {
            MASSERT((*ppSurface)->GetTest() == pTest);
            if (ppNewSurface != 0)
            {
                // No pre-allocated PmSurface passed in
                MASSERT(*ppNewSurface == 0);
                *ppNewSurface = *ppSurface;
            }
            return rc;
        }
    }
    for (PmSurfaces::iterator ppSurface = m_InactiveSurfaces.begin();
         ppSurface != m_InactiveSurfaces.end(); ++ppSurface)
    {
        if ((*ppSurface)->GetMdiagSurf() == pMdiagSurf)
        {
            MASSERT((*ppSurface)->GetTest() == pTest);
            if (ppNewSurface != 0)
            {
                // No pre-allocated PmSurface passed in
                MASSERT(*ppNewSurface == 0);
                *ppNewSurface = *ppSurface;
            }
            return rc;
        }
    }

    PmSurface *pPmSurface = 0;
    if ((ppNewSurface != 0) && (*ppNewSurface != 0))
    {
        // A pre-allocated PmSurface passed in
        pPmSurface = *ppNewSurface;
    }
    else
    {
        // Allocate a new PmSurface
        pPmSurface = new PmSurface(this, pTest, pMdiagSurf, ExternalSurf2D);
        if (ppNewSurface != 0)
        {
            *ppNewSurface = pPmSurface;
        }
    }

    // Add the new PmSurface.
    if (InTest(pTest))
    {
        m_ActiveSurfaces.push_back(pPmSurface);
    }
    else
    {
        m_InactiveSurfaces.push_back(pPmSurface);
    }

    if (pPmSurface->GetMdiagSurf() &&
        pPmSurface->GetMdiagSurf()->GetMemHandle() != 0)
    {
        CHECK_RC(AddGpuDevice(pPmSurface->GetMdiagSurf()->GetGpuDev()));
    }

    // Surface created by policy manager does not belong any test;
    // It's shared among tests. It does not have local name, so no need to call
    // Trace3DTest::DeclareGlobalSharedSurface to register local name.
    // Add the shared surface to LwGpuResource which is visible to both
    // trace3d tests and plugins.
    //
    // Shared surface is identified by name
    // Empty surface can't be referenced - skipping register process
    const string& surfName = pPmSurface->GetMdiagSurf()->GetName();
    if (!ExternalSurf2D && !surfName.empty())
    {
        PmTest *pAnyPmTest = NULL;
        if (!m_ActiveTests.empty())
        {
            pAnyPmTest = *m_ActiveTests.begin();
        }
        else if (!m_InactiveTests.empty())
        {
            pAnyPmTest = *m_InactiveTests.begin();
        }

        if (pAnyPmTest)
        {
            // Warning if failed - plugin will not see the surface by name.
            // There are some existed policy_file which is creating surface
            // with same name twice. Return failure before fixing those tests
            // can't be accepted.
            if (OK != pAnyPmTest->RegisterPmSurface(
                surfName.c_str(), pPmSurface))
            {
                WarnPrintf("%s: Register surface %s to LwGpuResource failed. It might be caused by duplicated surface name.\n",
                           __FUNCTION__, pMdiagSurf->GetName().c_str());
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Called by tests to register their subsurfaces with PolicyManager
//!
RC PolicyManager::AddSubsurface
(
    PmTest *pTest,
    MdiagSurf *pMdiagSurf,
    UINT64 Offset,
    UINT64 Size,
    const string &Name,
    const string &TypeName
)
{
    RC rc;
    Tasker::MutexHolder mh(m_Mutex);

    // Make sure a PmSurface exists for pMdiagSurf
    //
    CHECK_RC(AddSurface(pTest, pMdiagSurf, true));

    // Find the PmSurface
    //
    PmSurface *pPmSurface = NULL;

    for (PmSurfaces::iterator ppSurface = m_ActiveSurfaces.begin();
         ppSurface != m_ActiveSurfaces.end(); ++ppSurface)
    {
        if ((*ppSurface)->GetMdiagSurf() == pMdiagSurf)
        {
            pPmSurface = *ppSurface;
            break;
        }
    }
    for (PmSurfaces::iterator ppSurface = m_InactiveSurfaces.begin();
         ppSurface != m_InactiveSurfaces.end(); ++ppSurface)
    {
        if ((*ppSurface)->GetMdiagSurf() == pMdiagSurf)
        {
            pPmSurface = *ppSurface;
            break;
        }
    }
    MASSERT(pPmSurface != NULL);

    // Add the subsurface to pPmSurface
    //
    CHECK_RC(pPmSurface->AddSubsurface(Offset, Size, Name, TypeName));
    return rc;
}

RC PolicyManager::AddAvailableCEEngineType(UINT32 ceEngineType, bool isGrCopy)
{
    RC rc;

    m_CEEngines.push_back(CEEngine(ceEngineType, isGrCopy));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager that a test is starting
//!
//! This method calls the StartTest() method on all PolicyManager
//! objects that have to do any start-of-test processing, and issues
//! the Start event.
//!
//! \sa EndTest()
//! \sa AbortTest()
//! \sa InTest()
//!
RC PolicyManager::StartTest(PmTest *pTest)
{
    RC rc;
    PmEvent_Start startEvent(pTest);

    MASSERT(IsInitialized());
    MASSERT(pTest != NULL);

    Tasker::MutexHolder mth(m_TestMutex);
    Tasker::MutexHolder mh(m_Mutex);

    // Abort if the test has already started
    //
    if (m_ActiveTests.count(pTest) > 0)
    {
        return rc;
    }
    DebugPrintf("PolicyManager: StartTest\n");

    // Move the test from m_InactiveTests to m_ActiveTests first so
    // that if anything fails, the test can be cleaned up by AbortTest().
    //
    MASSERT(m_InactiveTests.count(pTest) > 0);
    MASSERT(m_ActiveTests.count(pTest) == 0);
    m_InactiveTests.erase(pTest);
    m_ActiveTests.insert(pTest);

    // Add the GPU device here in case there were no surfaces or channels (ex. the test is run with -no_trace).
    CHECK_RC(AddGpuDevice(pTest->GetGpuDevice()));

    // Activate all channels, vaspaces, vfs, smcengines and surfaces for the test
    //
    for (UINT32 ii = 0; ii < m_InactiveVfs.size(); /* nop */)
    {
        PmVfTest *pVf = m_InactiveVfs[ii];
        if (InTest(pVf->GetTest()) || pVf->GetTest() == nullptr)
        {
            m_InactiveVfs.erase(m_InactiveVfs.begin() + ii);
            m_ActiveVfs.push_back(pVf);
        }
        else
        {
            ++ii;
        }
    }
    for (UINT32 ii = 0; ii < m_InactiveVaSpaces.size(); /* nop */)
    {
        PmVaSpace *pVaSpace = m_InactiveVaSpaces[ii];
        if (pVaSpace->IsGlobal() || InTest(pVaSpace->GetTest()))
        {
            m_InactiveVaSpaces.erase(m_InactiveVaSpaces.begin() + ii);
            m_ActiveVaSpaces.push_back(pVaSpace);
        }
        else
        {
            ++ii;
        }
    }
    for (UINT32 ii = 0; ii < m_InactiveSmcEngines.size(); /* nop */)
    {
        PmSmcEngine *pSmcEngine = m_InactiveSmcEngines[ii];
        if (InTest(pSmcEngine->GetTest()) || pSmcEngine->GetTest() == nullptr)
        {
            m_InactiveSmcEngines.erase(m_InactiveSmcEngines.begin() + ii);
            m_ActiveSmcEngines.push_back(pSmcEngine);
        }
        else
        {
            ++ii;
        }
    }
    for (UINT32 ii = 0; ii < m_InactiveChannels.size(); /* nop */)
    {
        PmChannel *pChannel = m_InactiveChannels[ii];
        if (InTest(pChannel->GetTest()))
        {
            m_InactiveChannels.erase(m_InactiveChannels.begin() + ii);
            m_ActiveChannels.push_back(pChannel);
        }
        else
        {
            ++ii;
        }
    }
    for (UINT32 ii = 0; ii < m_InactiveSurfaces.size(); /* nop */)
    {
        PmSurface *pSurface = m_InactiveSurfaces[ii];
        if (InTest(pSurface->GetTest()))
        {
            m_InactiveSurfaces.erase(m_InactiveSurfaces.begin() + ii);
            m_ActiveSurfaces.push_back(pSurface);
        }
        else
        {
            ++ii;
        }
    }

    // If there are several conlwrrent tests, we only need to
    // initialize the gilder and event manager for the first one.
    //
    if (Platform::IsPhysFunMode())
    {
        // skip this wait for RM support
        CHECK_RC(GetPreAllocSurface(pTest->GetLwRmPtr())->CreatePreAllocSurface());
    }

    if (m_ActiveTests.size() == 1)
    {
        CHECK_RC(m_pGilder->StartTest());
        CHECK_RC(m_pEventMgr->StartTest());

        // If fault buffer objects are needed, create those also
        // when the first test starts.
        if (m_FaultBufferNeeded)
        {
            CHECK_RC(CreateReplayableInts());
        }

        if (m_NonReplayableFaultBufferNeeded)
        {
            CHECK_RC(CreateNonReplayableInts());
        }

        CHECK_RC(CreatePhysicalPageFaultInts());

        if (m_AccessCounterBufferNeeded)
        {
            CHECK_RC(CreateAccessCounterInts());
        }

        if (!m_ErrorLoggerInterruptNames.empty())
        {
            CHECK_RC(CreateErrorLoggerInts(&m_ErrorLoggerInterruptNames));
        }

        if (m_PoisonErrorNeeded)
        {
            CHECK_RC(CreatePoisonErrorInts());
        }
    }

    // Each test in multiple conlwrrent tests will receive multiple
    // StartTest events.  These can be handled with ActionBlocks
    // using OnTriggerCount to differentiate between the tests
    //
    CHECK_RC(m_pEventMgr->HandleEvent(&startEvent));

    DebugPrintf("PolicyManager: StartTest Done\n");
    return rc;
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager that a test is ending
//!
//! This method calls the EndTest() method on all PolicyManager
//! objects that have to do any end-of-test processing, including
//! gilding and freeing any test-related resources.
//!
//! \sa StartTest()
//! \sa AbortTest()
//! \sa InTest()
//!
RC PolicyManager::EndTest(PmTest *pTest)
{
    return EndOrAbortTest(pTest, false);
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager that a test is being aborted
//!
//! \sa StartTest()
//! \sa EndTest()
//! \sa InTest()
//!
RC PolicyManager::AbortTest(PmTest *pTest)
{
    return EndOrAbortTest(pTest, true);
}

//--------------------------------------------------------------------
//! \brief Body of EndTest() and AbortTest()
//!
//! This method is called from EndTest() or AbortTest() whene a test
//! is ending, whether naturally or due to a premature abort
//! respectively.
//!
//! This method calls the EndTest() method on all PolicyManager
//! objects that have to do any end-of-test processing, including
//! gilding and freeing any test-related resources.
//!
//! \sa EndTest()
//! \sa AbortTest()
//!
//! \param pTest The test that is ending.  Do nothing if this
//!     parameter is NULL.
//! \param Aborted False if this method was called from EndTest(),
//!     true if this method was called from AbortTest().
//!
RC PolicyManager::EndOrAbortTest(PmTest *pTest, bool Aborted)
{
    StickyRC firstRc;
    MASSERT(IsInitialized());

    if (pTest == NULL)
        return firstRc;

    PmEvent_End endEvent(pTest);
    firstRc = m_pEventMgr->HandleEvent(&endEvent);

    // Before shutting down the last test, wait for any required events
    // to occur
    //
    if (!Aborted && (m_ActiveTests.size() == 1) &&
        (m_ActiveTests.count(pTest) > 0))
    {
        firstRc = m_pGilder->WaitForRequiredEvents();
    }

    Tasker::MutexHolder mth(m_TestMutex);
    Tasker::MutexHolder mh(m_Mutex);

    // Do nothing if the test has already ended
    //
    if (m_ActiveTests.count(pTest) == 0)
        return firstRc;
    DebugPrintf("PolicyManager: %s\n", Aborted ? "AbortTest" : "EndTest");

    // Move the test to the inactive list
    //
    m_ActiveTests.erase(pTest);
    m_InactiveTests.insert(pTest);

    // If this was the last conlwrrent test, shut down the
    // EventManager before deactivating surfaces and channels.
    // While shutting down Event manager, there might be triggered events
    // needing active surfaces/channels. Bug 1613792
    if (m_ActiveTests.empty())
    {
        // The policy manager mutex should not be held when shutting
        // down the event manager.  If a subtask is blocked on the
        // mutex during EndTest() then a deadlock results since the
        // subtask cannot be shutdown until it acquires the mutex
        //
        Tasker::ReleaseMutex(m_Mutex);
        firstRc = m_pEventMgr->EndTest();
        Tasker::AcquireMutex(m_Mutex);
    }

    // Call EndTest on any channels, vaspaces or surfaces that are owned by the
    // test, and move them to the inactive list.  If this is the last
    // conlwrrent test, then do the same thing to all the channels and
    // surfaces that aren't owned by any tests.
    //
    for (UINT32 ii = 0; ii < m_ActiveChannels.size(); /* nop */)
    {
        PmChannel *pChannel = m_ActiveChannels[ii];
        if (!InTest(pChannel->GetTest()))
        {
            m_ActiveChannels.erase(m_ActiveChannels.begin() + ii);
            m_InactiveChannels.push_back(pChannel);
            firstRc = pChannel->EndTest();
        }
        else
        {
            ++ii;
        }
    }

    for (UINT32 ii = 0; ii < m_ActiveSurfaces.size(); /* nop */)
    {
        PmSurface *pSurface = m_ActiveSurfaces[ii];
        if (!InTest(pSurface->GetTest()))
        {
            m_ActiveSurfaces.erase(m_ActiveSurfaces.begin() + ii);
            m_InactiveSurfaces.push_back(pSurface);
            firstRc = pSurface->EndTest();
        }
        else
        {
            ++ii;
        }
    }

    // Reset Surface PteKinds
    for (const auto &savedPteKind : m_SavedPteKinds)
    {
        auto ranges = savedPteKind.first;
        vector<LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS> params;
        GpuDevices gpuDevices;
        vector<LwRm*> lwRms;

        if (OK != 
            (firstRc = PmMemRange::CreateSetPteInfoStructs(
                &ranges, &params, &gpuDevices, &lwRms)))
        {
            return firstRc;
        }

        for (UINT32 ii = 0; ii < params.size(); ++ii)
        {
            for (UINT32 jj = 0; jj < LW0080_CTRL_DMA_SET_PTE_INFO_PTE_BLOCKS; ++jj)
            {
                if (params[ii].pteBlocks[jj].pageSize != 0 &&
                    FLD_TEST_DRF(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                        _TRUE, params[ii].pteBlocks[jj].pteFlags))
                {
                    params[ii].pteBlocks[jj].kind = savedPteKind.second;
                }
            }
            if (OK != (firstRc = lwRms[ii]->ControlByDevice(gpuDevices[ii], 
                LW0080_CTRL_CMD_DMA_SET_PTE_INFO, &params[ii], 
                sizeof(params[ii]))))
            {
                return firstRc;
            }
        }
    }
    m_SavedPteKinds.clear();

    for (UINT32 ii = 0; ii < m_ActiveVaSpaces.size(); /* nop */)
    {
        PmVaSpace *pVaSpace = m_ActiveVaSpaces[ii];
        if (!pVaSpace->IsGlobal() && !InTest(pVaSpace->GetTest()))
        {
            m_ActiveVaSpaces.erase(m_ActiveVaSpaces.begin() + ii);
            m_InactiveVaSpaces.push_back(pVaSpace);
            firstRc = pVaSpace->EndTest();
        }
        else
        {
            ++ii;
        }
    }

    // If this was the last conlwrrent test, then run the Gilder,
    // and free the test data.
    //
    if (m_ActiveTests.empty())
    {
        firstRc = m_pGilder->EndTest();

        // Remove surfaces registered to LwGpuResource in AddSurface()
        //
        for (UINT32 ii = 0; ii < m_InactiveSurfaces.size(); ++ii)
        {
            PmSurface *pSurface = m_InactiveSurfaces[ii];
            if (!pSurface->IsExternalSurf2D())
            {
                PmTest *pAnyPmTest = NULL;
                if (!m_ActiveTests.empty())
                {
                    pAnyPmTest = *m_ActiveTests.begin();
                }
                else if (!m_InactiveTests.empty())
                {
                    pAnyPmTest = *m_InactiveTests.begin();
                }

                if (pAnyPmTest)
                {
                    MdiagSurf *pMdiagSurf = pSurface->GetMdiagSurf();
                    if (OK != pAnyPmTest->UnRegisterPmSurface(
                                       pMdiagSurf->GetName().c_str()))
                    {
                        WarnPrintf("%s: Unregister surface %s to LwGpuResource failed. It might be caused by duplicated surface name.\n",
                                   __FUNCTION__, pMdiagSurf->GetName().c_str());
                    }
                }
            }
        }

        // Delete the corresponding PreAllocSurface since the associated
        // PmSurface will be deleted by the test
        m_pPreAllocSurface.erase(pTest->GetLwRmPtr());

        firstRc = FreeTestData();

        // Increment the sequential test number.  Do not insert any
        // CHECK_RC statements before this point; we need to increment
        // this number even on failure.
        //
        ++m_TestNum;
    }

    DebugPrintf("PolicyManager: %s Done\n", Aborted ? "AbortTest" : "EndTest");
    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Return true if the indicated test is running
//!
//! A test is considered to be "running" if StartTest has been called
//! on it, and neither EndTest nor AbortTest have been called on the
//! test.
//!
//! If pTest is NULL, then this function returns true if any test is
//! running (i.e. it becomes identical to InAnyTest).  This makes it
//! colwenient to pass the return value of PmChannel::GetTest() and
//! PmSurface::GetTest(), since those methods return NULL if the
//! channel/surface don't belong to a particular test and should be
//! considered valid until the last EndTest or AbortTest is called.
//!
//! Note: pTest can also be obtained from GetTestFromUniquePtr(),
//! which can return NULL once m_Tests is freed.  But that doesn't
//! happen until the last EndTest or AbortTest is called, so this
//! method should return the expected "false" value in that case.
//!
//! \sa StartTest()
//! \sa EndTest()
//! \sa AbortTest()
//!
bool PolicyManager::InTest(PmTest *pTest) const
{
    if (pTest != NULL)
        return m_ActiveTests.count(pTest) > 0;
    else
        return m_ActiveTests.size() > 0;
}

//--------------------------------------------------------------------
//! \brief Return true if test is running and hasn't called Action.AbortTest
//!
//! This function is similar to PolicyManager::InTest(), except that
//! it also checks whether we've called Action.AbortTest.
//! Action.AbortTest just sets a flag to tell the test to abort.  The
//! test should respond to the flag by shutting down and calling
//! PolicyManager::AbortTest, but the test can continue running for a
//! while before that happens.
//!
//! If pTest is NULL, then this function returns true if any test
//! meets those criteria.  See PolicyManager::InTest for the
//! rationale.
//!
//! \sa InTest()
//!
bool PolicyManager::InUnabortedTest(PmTest *pTest) const
{
    if (pTest != NULL)
    {
        return (m_ActiveTests.count(pTest) > 0) && !pTest->IsAborting();
    }
    else
    {
        for (PmTests::const_iterator ppTest = m_ActiveTests.begin();
             ppTest != m_ActiveTests.end(); ++ppTest)
        {
            if (!(*ppTest)->IsAborting())
                return true;
        }
        return false;
    }
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager that a wait for idle oclwrred on the
//! entire chip
//!
RC PolicyManager::WaitForIdle()
{
    RC rc;

    Tasker::MutexHolder mh(m_Mutex);
    PmChannels::iterator channelIter;

    // If wait-for-idle event has just oclwred, call WaitForIdle
    // on all the channels created via the policy manager.
    //
    // Channel WaitForIdle() leads to thread yield. Another trace_3d thread
    // may enter EndTest to modify m_ActiveChannels. To avoid referencing stale
    // channelIter of m_ActiveChannels, save policymn channels to be wfi before
    // yielding to another thread. Bug 1484542.
    PmChannels policymnChannels;
    for (channelIter = m_ActiveChannels.begin();
         channelIter != m_ActiveChannels.end();
         ++channelIter)
    {
        if ((*channelIter)->CreatedByPM())
        {
            policymnChannels.push_back(*channelIter);
        }
    }

    for (channelIter = policymnChannels.begin();
         channelIter != policymnChannels.end();
         ++channelIter)
    {
        if (InTest((*channelIter)->GetTest()))
        {
            CHECK_RC((*channelIter)->WaitForIdle());
        }
    }

    // Create a WaitForIdle event with no channel
    PmEvent_WaitForIdle waitForIdleEvent(NULL);
    return m_pEventMgr->HandleEvent(&waitForIdleEvent);
}
//--------------------------------------------------------------------
//! \brief tells PolicyManager that
RC PolicyManager::HandleRmEvent(GpuSubdevice* pSubdev, UINT32 EventId)
{
    PmEvent_RmEvent NewRmEvent(pSubdev, EventId);
    return m_pEventMgr->HandleEvent(&NewRmEvent);
}
//--------------------------------------------------------------------
//! \brief Tells PolicyManager to enable robust-channel errors
//!
//! By default, the RM generates a breakpoint when a robust channel
//! error oclwrs during simulation.  When RobustChannels are enabled
//! in PolicyManager, it will tell the RM to skip the breakpoint so
//! that mods can process the robust-channel errors instead.
//!
//! This method also enables the PmChannelWrapper, since that wrapper
//! is needed to generate a PolicyManager event when a robust-channel
//! error oclwrs.
//!
RC PolicyManager::SetEnableRobustChannel(bool val)
{
    RC rc;

    if (val && !m_RobustChannelEnabled)
    {
        // Configure robust-channel support.  These configuration
        // tasks only need to be done once.
        //
        CHECK_RC(Registry::Write("ResourceManager",
                                 LW_REG_STR_RM_BREAK_ON_RC,
                                 LW_REG_STR_RM_BREAK_ON_RC_DISABLE));
        CHECK_RC(Registry::Write("ResourceManager",
                                 LW_REG_STR_RM_ROBUST_CHANNELS,
                                 LW_REG_STR_RM_ROBUST_CHANNELS_ENABLE));
    }

    m_RobustChannelEnabled = val;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager to enable resetting the Gpu
//!
//! Called by the parser if we use any methods that call
//! GpuDevice::Reset.  Must be called before RmInit.
//!
RC PolicyManager::SetEnableGpuReset(bool val)
{
    RC rc;

    if (val && !m_GpuResetEnabled)
    {
        CHECK_RC(Registry::Write("ResourceManager",
                                 LW_REG_STR_FORCE_PCIE_CONFIG_SAVE,
                                 true));
    }

    m_GpuResetEnabled = val;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Called by PmEventManager when an event oclwrred
//!
//! This method calls the OnEvent() method on all PolicyManager
//! objects that have to do any updating.
//!
RC PolicyManager::OnEvent()
{
    RC rc;
    MASSERT(IsInitialized());

    Tasker::MutexHolder mh(m_Mutex);

    for (PmSurfaces::const_iterator ppSurf = m_ActiveSurfaces.begin();
         ppSurf != m_ActiveSurfaces.end(); ++ppSurf)
    {
        CHECK_RC((*ppSurf)->OnEvent());
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Save runlist entries to policymanager
//!
//! Called by Action.PreemptRunlist.
//!
void PolicyManager::AddPreemptRlEntries
(
    Runlist* pRunList,
    const Runlist::Entries &entries
)
{
    MASSERT(pRunList != NULL);

    m_PreemptRlEntries[pRunList] = entries;
}

//--------------------------------------------------------------------
//! \brief Save removed runlist entries to policymanager
//!
//! Action.RestoreEntriesToRunlist() can restore these removed entries.
//! Called by Action.RemoveEntriesFromRunlist() and
//! Action.RemoveChannelFromRunlist()
void PolicyManager::SaveRemovedRlEntries
(
    Runlist* pRunList,
    const Runlist::Entries &entries
)
{
    MASSERT(pRunList != NULL);

    m_RemovedEntries[pRunList] = entries;
}

//--------------------------------------------------------------------
//! \brief Remove runlist entries from policymanager
//!
//! Called by Action.ResubmitRunlist.
//!
void PolicyManager::RemovePreemptRlEntries(Runlist* pRunList)
{
    MASSERT(pRunList != NULL);

    map<Runlist*, Runlist::Entries>::iterator it;
    it = m_PreemptRlEntries.find(pRunList);
    if (it != m_PreemptRlEntries.end())
    {
        m_PreemptRlEntries.erase(it);
    }
}

//--------------------------------------------------------------------
//! \brief Return the runlist entries added by AddPreemptRlEntries()
//!
Runlist::Entries PolicyManager::GetPreemptRlEntries(Runlist* pRunList) const
{
    MASSERT(pRunList != NULL);

    Runlist::Entries entries;
    map<Runlist*, Runlist::Entries>::const_iterator it;
    it = m_PreemptRlEntries.find(pRunList);
    if (it != m_PreemptRlEntries.end())
    {
        entries = it->second;
    }

    return entries;
}

//--------------------------------------------------------------------
//! \brief Get removed entries saved in PolicyManager
//!
//! Called by Action.RestoreEntriesToRunlist() and
//! Action.RestoreChannelToRunlist().
Runlist::Entries PolicyManager::GetRemovedRlEntries(Runlist* pRunList) const
{
    MASSERT(pRunList != NULL);

    Runlist::Entries entries;
    map<Runlist*, Runlist::Entries>::const_iterator it;
    it = m_RemovedEntries.find(pRunList);
    if (it != m_RemovedEntries.end())
    {
        entries = it->second;
    }

    return entries;
}

//--------------------------------------------------------------------
//! \brief Add a GpuDevice to PolicyManager
//!
//! Called by AddChannel() and Add[Sub]Surface().
//!
RC PolicyManager::AddGpuDevice(GpuDevice *pGpuDevice)
{
    MASSERT(pGpuDevice != NULL);
    static CmpGpuDevicesByInst Cmp;
    RC rc;

    if (!binary_search(m_GpuDevices.begin(), m_GpuDevices.end(),
                       pGpuDevice, Cmp))
    {
        InitSysMemBaseAddrCache(pGpuDevice);

        m_GpuDevices.push_back(pGpuDevice);
        sort(m_GpuDevices.begin(), m_GpuDevices.end(), Cmp);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Called by EndOrAbortTest() and destructor to free
//! test-related data
//!
RC PolicyManager::FreeTestData()
{
    RC rc;

    m_DisableMutexInActions = 0;

    m_PreemptRlEntries.clear();
    m_RemovedEntries.clear();

    PmChannels::iterator channelIter;
    for (channelIter = m_ActiveChannels.begin();
         channelIter != m_ActiveChannels.end();
         ++channelIter)
    {
        if ((*channelIter)->CreatedByPM())
        {
            LwRm* pLwRm = (*channelIter)->GetLwRmPtr();

            (*channelIter)->FreeSubchannels();
            pLwRm->Free((*channelIter)->GetHandle());
        }
    }
    for (channelIter = m_InactiveChannels.begin();
         channelIter != m_InactiveChannels.end();
         ++channelIter)
    {
        if ((*channelIter)->CreatedByPM())
        {
            LwRm* pLwRm = (*channelIter)->GetLwRmPtr();

            (*channelIter)->FreeSubchannels();
            pLwRm->Free((*channelIter)->GetHandle());
        }
    }

    // Delete tests, channels, and surfaces.  Note: the destructors
    // can call Tasker::Yield, so move the pointers to temporary
    // storage before deleting them so that the containers won't
    // contain deleted entries if/when we swap tasks.
    //
    DeletePtrContainer(m_ActiveTests);
    DeletePtrContainer(m_InactiveTests);
    DeletePtrContainer(m_ActiveChannels);
    DeletePtrContainer(m_InactiveChannels);
    DeletePtrContainer(m_ActiveVaSpaces);
    DeletePtrContainer(m_InactiveVaSpaces);
    DeletePtrContainer(m_ReplayableIntMap);
    DeletePtrContainer(m_NonReplayableIntMap);
    DeletePtrContainer(m_AccessCounterIntMap);
    DeletePtrContainer(m_ErrorLoggerIntMap);
    PmSurfaces surfaces;
    surfaces.swap(m_ActiveSurfaces);
    for (size_t ii = 0; ii < surfaces.size(); ii++)
    {
        MASSERT(surfaces[ii]->GetRefs() == surfaces[ii]->GetChildRefs() + 1);
        surfaces[ii]->Release();
    }
    surfaces.clear();
    surfaces.swap(m_InactiveSurfaces);
    for (size_t ii = 0; ii < surfaces.size(); ii++)
    {
        MASSERT(surfaces[ii]->GetRefs() == surfaces[ii]->GetChildRefs() + 1);
        surfaces[ii]->Release();
    }

    m_GpuDevices.clear();
    m_TimerStartUS.clear();

    for (const auto &deviceVASpace : VaSpaceUtils::s_GpuDevHandles.hFermiDeviceVASpace)
    {
        LwRm* pLwRm = deviceVASpace.first;
        pLwRm->Free(deviceVASpace.second);
    }
    VaSpaceUtils::s_GpuDevHandles.hFermiDeviceVASpace.clear();

    for (const auto &hostVASpace : VaSpaceUtils::s_GpuDevHandles.hFermiHostVASpaces)
    {
        LwRmPtr()->Free(hostVASpace.second);
    }
    VaSpaceUtils::s_GpuDevHandles.hFermiHostVASpaces.clear();

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set the random seed for policy manager.  If the random seed
//!        is set via this API, it will take precedence over all seed
//!        commands in the policy file
//!
//! \param seed : The random seed to set
//!
void PolicyManager::SetRandomSeed(UINT32 seed)
{
    m_RandomSeed = seed;
    m_bRandomSeedSet = true;
}

//--------------------------------------------------------------------
//! \brief Colwert engine names supported by PolicyManager to names
//!        supported by GPU device
//!
//! Engine names supported by Policy Manager are listed in
//! https://wiki.lwpu.com/engwiki/index.php/MODS/PolicyFileSyntax
//! which are same as engine names of GpuDevice class except those alias.
//!
//! Policy Manager supports these alias name but GpuDevice::GetEngineName()
//! does not. This function is for this colwertion.
//!
string PolicyManager::ColwPm2GpuEngineName(const string &PmEngineName) const
{
    // Alias engine name in sdk/lwpu/inc/class/cl2080.h
    // #define LW2080_ENGINE_TYPE_TSEC    LW2080_ENGINE_TYPE_CIPHER
    // #define LW2080_ENGINE_TYPE_LWENC0  LW2080_ENGINE_TYPE_MSENC
    //
    string engineName = PmEngineName;
    if (0 == Utility::strcasecmp(PmEngineName.c_str(), "TSEC"))
    {
        engineName = "CIPHER";
    }
    else if (0 == Utility::strcasecmp(PmEngineName.c_str(), "LWENC0"))
    {
        engineName = "MSENC";
    }

    return engineName;
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager to create fault buffer objects
//!
//! A fault buffer object is used to access the RM-created fault buffer.
//! The fault buffer contains information about replayable faults.
//! There is one fault buffer per GPU subdevice.
//!
RC PolicyManager::CreateReplayableInts()
{
    RC rc;
    GpuDevices::iterator iter;

    for (iter = m_GpuDevices.begin(); iter != m_GpuDevices.end(); ++iter)
    {
        for (UINT32 i = 0; i < (*iter)->GetNumSubdevices(); ++i)
        {
            PmReplayableInt *replayableInt =
                new PmReplayableInt(this, (*iter)->GetSubdevice(i));
            m_ReplayableIntMap[(*iter)->GetSubdevice(i)] = replayableInt;

            CHECK_RC(replayableInt->PrepareForInterrupt());
        }
    }

    return rc;
}

//----------------------------------------------------------------------------------
//! \brief Tells PolicyManager to create fault buffer objects for non-replable fault
//!
//! A fault buffer object is used to access the fault buffer registed in RM side.
//! The fault buffer contains information about replayable faults.
//! There is one fault buffer per GPU subdevice.
//!
RC PolicyManager::CreateNonReplayableInts()
{
    RC rc;
    GpuDevices::iterator iter;

    for (iter = m_GpuDevices.begin(); iter != m_GpuDevices.end(); ++iter)
    {
        for (UINT32 i = 0; i < (*iter)->GetNumSubdevices(); ++i)
        {
            PmNonReplayableInt *recoverableInt =
                new PmNonReplayableInt(this, (*iter)->GetSubdevice(i));
            m_NonReplayableIntMap[(*iter)->GetSubdevice(i)] = recoverableInt;

            CHECK_RC(recoverableInt->PrepareForInterrupt());
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager to create access counter objects
//!
//! An access counter buffer object is used to access the RM-created
//! access counter buffer.
//! The access counter buffer contains information about access counter
//! notifications.
//! There is one access counter buffer per GPU subdevice.
//!
RC PolicyManager::CreateAccessCounterInts()
{
    RC rc;
    GpuDevices::iterator iter;

    for (iter = m_GpuDevices.begin(); iter != m_GpuDevices.end(); ++iter)
    {
        for (UINT32 i = 0; i < (*iter)->GetNumSubdevices(); ++i)
        {
            PmAccessCounterInt *accessCounterInt =
                new PmAccessCounterInt(this, (*iter)->GetSubdevice(i), m_AccessCounterNotificationWaitMs);
            m_AccessCounterIntMap[(*iter)->GetSubdevice(i)] = accessCounterInt;

            CHECK_RC(accessCounterInt->PrepareForInterrupt());
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager to create error logger interrupt objects
//!
//! An error logger interrupt object is used to track callbacks for error
//! logger. There is one error logger interrupt object per GPU subdevice.
//!
RC PolicyManager::CreateErrorLoggerInts(const InterruptNames *pNames)
{
    RC rc;
    GpuDevices::iterator iter;
    MASSERT(!pNames->empty());

    for (iter = m_GpuDevices.begin(); iter != m_GpuDevices.end(); ++iter)
    {
        for (UINT32 i = 0; i < (*iter)->GetNumSubdevices(); ++i)
        {
            PmErrorLoggerInt *loggedInt =
                new PmErrorLoggerInt(this, (*iter)->GetSubdevice(i), pNames);
            m_ErrorLoggerIntMap[(*iter)->GetSubdevice(i)] = loggedInt;

            CHECK_RC(loggedInt->PrepareForInterrupt());
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Tells PolicyManager to create fault buffer objects
//!
//! A fault buffer object is used to access the RM-created fault buffer.
//! The fault buffer contains information about replayable faults.
//! There is one fault buffer per GPU subdevice.
//!
RC PolicyManager::ClearFaultBuffer(GpuSubdevice *gpuSubdevice)
{
    RC rc;
    map<GpuSubdevice*,PmReplayableInt*>::iterator iter;

    iter = m_ReplayableIntMap.find(gpuSubdevice);

    if (iter == m_ReplayableIntMap.end())
    {
        ErrPrintf("can't find fault buffer for GPU subdevice.\n");
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC((*iter).second->ClearFaultBuffer());
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get registered CE by index
//!
//! \param ceIndex:       Index in CE group
//! \param bAsyncCE:      AsyncCE group or SyncCE(GrCE) group
//! \param pCEEngineType: Returned value
RC PolicyManager::GetCEEngineTypeByIndex
(
    UINT32 ceIndex,
    bool bAsyncCE,
    UINT32 *pCEEngineType
)
{
    if (NULL == pCEEngineType)
    {
        return RC::ILWALID_ARGUMENT;
    }

    vector<UINT32> ceCandidates;
    if (!bAsyncCE)
    {
        for (size_t i = 0; i < m_CEEngines.size(); ++i)
        {
            if (m_CEEngines[i].IsGrCopy)
                ceCandidates.push_back(m_CEEngines[i].CEEngineType);
            else
                continue;
        }
    }
    else
    {
        for (size_t i = 0; i < m_CEEngines.size(); ++i)
        {
            if (m_CEEngines[i].IsGrCopy)
                continue;
            else
                ceCandidates.push_back(m_CEEngines[i].CEEngineType);
        }
    }

    if (ceCandidates.size() == 0)
    {
        // not found
        return RC::SOFTWARE_ERROR;
    }

    UINT32 index = ceIndex % ceCandidates.size();
    *pCEEngineType = ceCandidates[index];

    return OK;
}

//--------------------------------------------------------------------
//! \brief Constructor - save the state of the policy manager mutex
//!
PolicyManager::PmMutexSaver::PmMutexSaver(PolicyManager *pPolicyMan)
:   m_pPolicyManager(pPolicyMan),
    m_LockCount(0)
{
    if (m_pPolicyManager == nullptr)
        return;

    void *pMutex = m_pPolicyManager->GetMutex();
    MASSERT(pMutex);
    if (Tasker::CheckMutexOwner(pMutex))
    {
        while (Tasker::CheckMutex(pMutex))
        {
            Tasker::ReleaseMutex(pMutex);
            m_LockCount++;
        }
    }
}

//--------------------------------------------------------------------
//! \brief Constructor - restore the state of the policy manager mutex
//!
PolicyManager::PmMutexSaver::~PmMutexSaver()
{
    if (m_pPolicyManager == nullptr)
        return;

    void *pMutex = m_pPolicyManager->GetMutex();
    MASSERT(pMutex);
    while (m_LockCount--)
    {
        Tasker::AcquireMutex(pMutex);
    }
}

PolicyManager::PreAllocSurface* PolicyManager::GetPreAllocSurface(LwRm* pLwRm)
{
    if (m_pPreAllocSurface.find(pLwRm) == m_pPreAllocSurface.end())
    {
        m_pPreAllocSurface[pLwRm] = make_unique<PreAllocSurface>(this, pLwRm);
    }
    return m_pPreAllocSurface[pLwRm].get();
}

//--------------------------------------------------------------------
//! \brief Constructor - Manage a big surface to avoid small surface allocation
//!
PolicyManager::PreAllocSurface::PreAllocSurface
(
    PolicyManager *pPolicyMn,
    LwRm* pLwRm
):
    m_PutOffset(0),
    m_LastChunkSize(0),
    m_pPmSurface(NULL),
    m_pPolicyManager(pPolicyMn),
    m_pLwRm(pLwRm)
{
    MASSERT(pLwRm);
}

RC PolicyManager::PreAllocSurface::AllocSmallChunk(UINT64 size)
{
    RC rc;
    if (!m_pPmSurface)
    {
        MASSERT(m_PutOffset == 0);
        MASSERT(m_LastChunkSize == 0);
        CHECK_RC(CreatePreAllocSurface());
    }

    UINT64 alignedSize = ALIGN_UP(size, sizeof(UINT32));

    if (m_PutOffset + alignedSize > m_pPmSurface->GetSize())
    {
        MASSERT(!"PolicyManager::PreAllocSurface: surface is used up!");
        return RC::SOFTWARE_ERROR;
    }

    m_PutOffset += alignedSize; // update to point to next available size
    m_LastChunkSize = alignedSize; // record lastest size to callwlate base

    return OK;
}

UINT64 PolicyManager::PreAllocSurface::GetPhysAddress() const
{
    RC rc;
    MASSERT(m_pPmSurface);

    PmMemMappings memMappings;
    memMappings = m_pPmSurface->GetMemMappingsHelper()->GetMemMappings(NULL);

    if (memMappings.size() == 0)
    {
        RC rc;
        rc = m_pPmSurface->CreateMemMappings();
        if (rc != OK)
        {
            MASSERT(!"Create mem mappings failed for internal surface\n");
        }

        memMappings = m_pPmSurface->GetMemMappingsHelper()->GetMemMappings(NULL);
    }

    MASSERT(memMappings.size() == 1); // The surface must be contiguous

    MASSERT(m_PutOffset >= m_LastChunkSize);
    return memMappings[0]->GetPhysAddr() + m_PutOffset - m_LastChunkSize;
}

UINT64 PolicyManager::PreAllocSurface::GetGpuVirtAddress() const
{
    MASSERT(m_pPmSurface);
    MASSERT(m_PutOffset >= m_LastChunkSize);

    return m_pPmSurface->GetMdiagSurf()->GetCtxDmaOffsetGpu() +
        m_PutOffset - m_LastChunkSize;
}

Memory::Location PolicyManager::PreAllocSurface::GetLocation() const
{
    MASSERT(m_pPmSurface);

    return m_pPmSurface->GetMdiagSurf()->GetLocation();
}

RC PolicyManager::PreAllocSurface::CreatePreAllocSurface()
{
    RC rc;

    if(!m_pPolicyManager->GetInband())
    {
        DebugPrintf("No need to pre create chunk for the inband pte change.\n");
        return rc;
    }

    if (!m_pPmSurface)
    {
        GpuDevices gpuDevices = m_pPolicyManager->GetGpuDevices();
        if (gpuDevices.size() != 1)
        {
            InfoPrintf("%s Multiple gpu deviceis are not supported by this function\n", __FUNCTION__);
            return rc;
        }

        // Not support multiple gpus device yet
        // Interface Get* need to be extended to
        // support multiple gpu devies
        if (gpuDevices[0]->GetNumSubdevices() > 1)
        {
            InfoPrintf("%s Multiple gpu subdeviceis are not supported by this function\n", __FUNCTION__);
            return rc;
        }

        //create surface
        MdiagSurf *pSurf2D = new MdiagSurf();

        pSurf2D->SetArrayPitch(SIZE_LIMIT_IN_BYTES);
        pSurf2D->SetColorFormat(ColorUtils::Y8);
        pSurf2D->SetLocation(Memory::Coherent);
        pSurf2D->SetPhysContig(true);

        CHECK_RC(pSurf2D->Alloc(gpuDevices[0], m_pLwRm));
        CHECK_RC(pSurf2D->Map(0, m_pLwRm));

        CHECK_RC(m_pPolicyManager->AddSurface(NULL, pSurf2D, false, &m_pPmSurface));
        CHECK_RC(m_pPmSurface->CreateMemMappings());
    }

    return rc;
}

RC PolicyManager::PreAllocSurface::WritePreAllocSurface
(
    const void * Buf,
    size_t BufSize,
    UINT32 Subdev,
    Channel * pInbandChannel
)
{
    RC rc;

    if(BufSize > m_LastChunkSize)
    {
        CHECK_RC(pInbandChannel->WaitIdle(pInbandChannel->GetDefaultTimeoutMs()));
        m_PutOffset = 0;
        m_LastChunkSize = 0;
    }

    MdiagSurf *pMdiagSurf = m_pPmSurface->GetMdiagSurf();
    if(!pMdiagSurf->IsMapped())
    {
        CHECK_RC(pMdiagSurf->Map(Subdev, m_pLwRm));
    }

    MASSERT(m_PutOffset >= m_LastChunkSize);
    UINT64 offset = m_PutOffset - m_LastChunkSize;
    UINT08 * VirtualAddr = static_cast<UINT08 *>(pMdiagSurf->GetAddress()) + offset;

    Platform::VirtualWr(VirtualAddr, Buf, BufSize);
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    return OK;
}

RC PolicyManager::CreatePhysicalPageFaultInts()
{
    RC rc;
    GpuDevices::iterator iter;

    for (iter = m_GpuDevices.begin(); iter != m_GpuDevices.end(); ++iter)
    {
        for (UINT32 i = 0; i < (*iter)->GetNumSubdevices(); ++i)
        {
            PmPhysicalPageFaultInt * physPageFaultInt =
                new PmPhysicalPageFaultInt(this, (*iter)->GetSubdevice(i));
            CHECK_RC(physPageFaultInt->PrepareForInterrupt());
        }
    }

    return rc;
}

RC PolicyManager::CreatePoisonErrorInts()
{
    RC rc;
    GpuDevices::iterator iter;

    for (iter = m_GpuDevices.begin(); iter != m_GpuDevices.end(); ++iter)
    {
        for (UINT32 i = 0; i < (*iter)->GetNumSubdevices(); ++i)
        {
            PmNonfatalPoisonErrorInt * nonfatalPoisonErrorInt =
                new PmNonfatalPoisonErrorInt(this, (*iter)->GetSubdevice(i));
            CHECK_RC(nonfatalPoisonErrorInt->PrepareForInterrupt());

            PmFatalPoisonErrorInt * fatalPoisonErrorInt =
                new PmFatalPoisonErrorInt(this, (*iter)->GetSubdevice(i));
            CHECK_RC(fatalPoisonErrorInt->PrepareForInterrupt());
        }
    }

    return rc;
}

void PolicyManager::InitSysMemBaseAddrCache(const GpuDevice* pDev)
{
    pair<SysMemBaseAddrMap::iterator, bool> insertIter;
    insertIter = m_SysMemBaseAddrCache.insert(make_pair(pDev, SysMemBaseAddrArray()));
    SysMemBaseAddrArray& array = insertIter.first->second;

    // Check there's at least one subdevice
    MASSERT(pDev->GetNumSubdevices() > 0);
    array.resize(pDev->GetNumSubdevices());
    UINT64 savedAddr =  ~0ULL; // debug code
    for (UINT32 i = 0; i < pDev->GetNumSubdevices(); ++i)
    {
        UINT64 baseAddr = 0;
        if (!Platform::IsVirtFunMode())
        {
            const GpuSubdevice* subdev = pDev->GetSubdevice(i);

            ModsGpuRegField pfbUpperAddr;
            if (subdev->Regs().IsSupported(MODS_PFB_XV_UPPER_ADDR_ADR_63_47))
            {
                pfbUpperAddr = MODS_PFB_XV_UPPER_ADDR_ADR_63_47;
            }
            else
            {
                pfbUpperAddr = MODS_PFB_XV_UPPER_ADDR_ADR_63_40;
            }
            baseAddr = subdev->Regs().Read32(pfbUpperAddr);
            baseAddr <<= (sizeof(baseAddr) * 8) -  subdev->Regs().LookupMaskSize(pfbUpperAddr);
        }
        else
        {
            // Base XV_UPPER_ADDR is not virtualized and needs to issue a RPC to read this register
            baseAddr = 0;
        }

        DebugPrintf("PolicyManager: Init sysmem base addr - caching sysmem base address 0x%llx for device %p:%u.\n",
                    baseAddr, pDev, i);
        if (savedAddr != ~0ULL && baseAddr != savedAddr)
        {
            // debug code
            ErrPrintf("PolicyManager: Init sysmem base addr - subdevices' sysmem base addresses are different from each other: 0x%llx vs. 0x%llx.\n",
                      savedAddr, baseAddr);
            // Remove this assertion and the debugging code after fixing GetSysMemBaseAddrPerDev()
            // caller code by specifying specific subdevice id once there's a real case
            // in which subdevices base addresses are different from each other within a
            // single gpudevice.
            MASSERT(0);
        }

        array[i] = baseAddr;
        savedAddr = baseAddr; // debug code
    }
}

RC PolicyManager::GetSysMemBaseAddrPerDev(const GpuDevice* pDev, UINT64 *pAddrOut) const
{
    SysMemBaseAddrMap::const_iterator it = m_SysMemBaseAddrCache.find(pDev);
    if (it != m_SysMemBaseAddrCache.end())
    {
        const SysMemBaseAddrArray& array = it->second;

        // Temporary coding of subdevice ID.
        // Refer to comments on GetSysMemBaseAddrPerDev() in header file.
        const UINT32 subdevIdx = 0;

        MASSERT(subdevIdx < array.size());
        MASSERT(pAddrOut != 0);
        *pAddrOut = array[subdevIdx];

        return OK;
    }

    ErrPrintf("PolicyManager/GetSysMemBaseAddrPerDev: can't find sysmem base addr cached info for GPU device %p.\n",
              pDev);

    return RC::ILWALID_ARGUMENT;
}

RC PolicyManager::HandleChannelReset(LwRm::Handle hObject, LwRm* pLwRm)
{
    PmEvent_OnChannelReset rmChannelCallbackEvent(hObject, pLwRm);
    return GetEventManager()->HandleEvent(&rmChannelCallbackEvent);
}

RC PolicyManager::AddVf(PmVfTest * pVf)
{
    RC rc;
    MASSERT(pVf);

    Tasker::MutexHolder mh(m_Mutex);

    // Add the Vf.
    //
    DebugPrintf("PolicyManager: Add Vf '%d'\n", pVf->GetGFID());
    if (InTest(pVf->GetTest()) && pVf->GetTest() == nullptr)
    {
        m_ActiveVfs.push_back(pVf);
    }
    else
    {
        m_InactiveVfs.push_back(pVf);
    }

    // ToDo:: Need to add gpudevice at VmiopMdiagElw later
    // CHECK_RC(AddGpuDevice(pVf->GetGpuDevice()));
    return rc;
}

RC PolicyManager::AddSmcEngine(PmSmcEngine * pSmcEngine)
{
    RC rc;
    MASSERT(pSmcEngine);

    Tasker::MutexHolder mh(m_Mutex);

    // Add the SmcEngine
    DebugPrintf("PolicyManager: Add SmcEngine '%d'\n", pSmcEngine->GetSysPipe());
    if (InTest(pSmcEngine->GetTest()) && pSmcEngine->GetTest() == nullptr)
    {
        m_ActiveSmcEngines.push_back(pSmcEngine);
    }
    else
    {
        m_InactiveSmcEngines.push_back(pSmcEngine);
    }

    return rc;
}

RC PolicyManager::RemoveSmcEngine(LwRm * pLwRm)
{
    RC rc;

    Tasker::MutexHolder mh(m_Mutex);

    // Policy manager only register those smcEngine which have traces running on them
    for (auto pSmcEngine : m_ActiveSmcEngines)
    {
        if (pSmcEngine->GetLwRmPtr() == pLwRm)
        {
            for (auto test : m_ActiveTests)
            {
                if ((test->GetSmcEngine() == pSmcEngine) &&
                        (test->GetDeviceStatus() != ConlwrrentTest::FINISHED))
                {
                    ErrPrintf("The SMC engine sys%d is still busy at test %s.\n",
                              pSmcEngine->GetSysPipe(), test->GetName().c_str());
                    return RC::SOFTWARE_ERROR;
                }
            }

            m_ActiveSmcEngines.erase(
                    remove(m_ActiveSmcEngines.begin(), m_ActiveSmcEngines.end(), pSmcEngine),
                    m_ActiveSmcEngines.end());
            m_InactiveSmcEngines.push_back(pSmcEngine);
            InfoPrintf("Remove smcEngine sys%d at policy Manager to inactive queue.\n",
                       pSmcEngine->GetSysPipe());
        }
    }

    return rc;
}

RC PolicyManager::AddLWGpuResource(LWGpuResource* lwgpu)
{
    RC rc;
    MASSERT(lwgpu);

    Tasker::MutexHolder mh(m_Mutex);

    m_LwGpuResourceSet.insert(lwgpu);
    m_Dev2Res[lwgpu->GetGpuDevice()] = lwgpu;

    return rc;
}

LWGpuResources PolicyManager::GetLWGpuResources()
{
    return m_LwGpuResourceSet;
}

LWGpuResource* PolicyManager::GetLWGpuResourceBySubdev(GpuSubdevice *pSubdev)
{
    return m_Dev2Res[pSubdev->GetParentDevice()];
}

UINT32 PolicyManager::GetMaxSubCtxForSmcEngine(LwRm* pLwRm, GpuSubdevice *pSubdev)
{
    MASSERT(pLwRm != LwRmPtr().Get()); // Cannot be here if using default RM client
    LWGpuResource *pLWGpuRes = GetLWGpuResourceBySubdev(pSubdev);
    return pLWGpuRes->GetMaxSubCtxForSmcEngine(pLwRm);
}

bool PolicyManager::MultipleGpuPartitionsExist()
{
    for (const auto & lwgpu : m_LwGpuResourceSet)
    {
        if (lwgpu->MultipleGpuPartitionsExist())
            return true;
    }
    return false;
}

bool PolicyManager::IsSmcEngineAdded(PmSmcEngine* pSmcEngine)
{
    MASSERT(pSmcEngine);
    Tasker::MutexHolder mh(m_Mutex);

    for (auto const & activeSmcEngine : m_ActiveSmcEngines)
    {
        if (pSmcEngine->GetSysPipe() == activeSmcEngine->GetSysPipe())
            return true;
    }

    for (auto const & inactiveSmcEngine : m_InactiveSmcEngines)
    {
        if (pSmcEngine->GetSysPipe() == inactiveSmcEngine->GetSysPipe())
            return true;
    }

    return false;
}

PmSmcEngine* PolicyManager::GetMatchingSmcEngine(UINT32 engineId, LwRm* pLwRm)
{
    MASSERT(MDiagUtils::IsGraphicsEngine(engineId));

    for (auto const & smcEngine : m_ActiveSmcEngines)
    {
        if ((smcEngine->GetLwRmPtr() == pLwRm) &&
            (MDiagUtils::GetGrEngineId(0) == engineId))
        {
            return smcEngine;
        }
    }
    return nullptr;
}

RC PolicyManager::AddVaSpace(PmVaSpace * pVaSpace)
{
    RC rc;
    MASSERT(pVaSpace);

    Tasker::MutexHolder mh(m_Mutex);

    // Add the vaspace.
    //
    DebugPrintf("PolicyManager: Add VaSpace '%s', handle 0x%08x\n",
                pVaSpace->GetName().c_str(), pVaSpace->GetHandle());
    if (pVaSpace->IsGlobal() || InTest(pVaSpace->GetTest()))
    {
        m_ActiveVaSpaces.push_back(pVaSpace);
    }
    else
    {
        m_InactiveVaSpaces.push_back(pVaSpace);
    }
    CHECK_RC(AddGpuDevice(pVaSpace->GetGpuDevice()));
    return rc;
}

PmVaSpace * PolicyManager::GetVaSpace(LwRm::Handle hVaSpace, PmTest * pTest) const
{
    // Move the m_InActiveVaSpace loop to query the vaspace for the following reason.
    // 1. For the case, if it is conlwrrent run, the GetVaSpace and the StartTest
    // is one by one. No Yeild between them.
    // 2. For the case, if one test has been kick off and it run fast enough that the
    // second test or third test or later test hasn't been registered the vaspace into
    // the policymanager, the endTest only move the local test which this vaspace is
    // owned by this test only to the inactive test which doesn't affect to other test
    // to query.
    // 3. For the case, if it is the first test to register, it will register all the
    // vaspace it own and global vaspace no matter where they are (m_ActiveVaSpaces or
    // m_InActiveVaSpaces)
    for (PmVaSpaces::const_iterator ppVaSpace = m_ActiveVaSpaces.begin();
         ppVaSpace != m_ActiveVaSpaces.end(); ++ppVaSpace)
    {
        // for default vaspace in different test, create different pmvaspace for each test
        if ((*ppVaSpace)->IsDefaultVaSpace()  &&
            (hVaSpace == 0)  &&
            ((*ppVaSpace)->GetTest() == pTest))
        {
            // default vas (handle = 0), need to check the test registering the vas
            return *ppVaSpace;
        }
        else if ((!(*ppVaSpace)->IsDefaultVaSpace()) &&
                ((*ppVaSpace)->GetHandle() == hVaSpace))
        {
            // non-default vas, handle is unique to identify the vas
            return *ppVaSpace;
        }
    }

    return NULL;
}

PmVaSpace * PolicyManager::GetActiveVaSpace(LwRm::Handle hVaSpace, LwRm* pLwRm) const
{
    for (PmVaSpaces::const_iterator ppVaSpace = m_ActiveVaSpaces.begin();
         ppVaSpace != m_ActiveVaSpaces.end(); ++ppVaSpace)
    {
        if ((*ppVaSpace)->GetHandle() == hVaSpace)
        {
            // If GpuPartiton's LwRm* has been specified (by using
            // Policy.SetSmcEngine) then match its LwRm* with the VaSpace's LwRm*
            if (pLwRm)
            {
                if ((*ppVaSpace)->GetLwRmPtr() == pLwRm)
                    return *ppVaSpace;
            }
            else
                return *ppVaSpace;
        }
    }

    return NULL;
}

PolicyManagerMessageDebugger::~PolicyManagerMessageDebugger()
{
    MASSERT(m_TeeModule.size() == 0);
}

void PolicyManagerMessageDebugger::Release()
{
    for(map<string, TeeModule *>::iterator it = m_TeeModule.begin();
            it != m_TeeModule.end(); ++it)
    {
        delete (*it).second;
    }

    m_TeeModule.clear();
}

PolicyManagerMessageDebugger * PolicyManagerMessageDebugger::Instance()
{
    if(m_Instance)
        return m_Instance;

    m_Instance = new PolicyManagerMessageDebugger();

    return m_Instance;
}

RC PolicyManagerMessageDebugger::Init(const ArgReader * reader)
{
    RC rc;
    static bool init = false;
    UINT32 count = 0;

    if(init)
        return rc;

    if(reader->ParamPresent("-message_enable"))
    {
        count = reader->ParamPresent("-message_enable");
        PolicyManagerMessageDebugger::Instance()->ParseEnDisMessages(
                reader, count, "-message_enable", true);
        init = true;
    }

    if(reader->ParamPresent("-message_disable"))
    {
        count = reader->ParamPresent("-message_disable");
        PolicyManagerMessageDebugger::Instance()->ParseEnDisMessages(
                reader, count, "-message_disable", false);
        init = true;
    }

    return rc;
}

void PolicyManagerMessageDebugger::ParseEnDisMessages
(
    const ArgReader * reader,
    const UINT32 count,
    const string & keyword,
    bool enable
)
{
    for(UINT32 i = 0; i < count; ++i)
    {
        string moduleName = reader->ParamNStr(keyword.c_str(), i, 0);
        if((moduleName.find("Action") != string::npos) ||
                (moduleName.find("ActionBlock") != string::npos))
        {
            moduleName = "PolicyManager_" + moduleName;
            EnDisMessages(moduleName, enable);
        }
        else if (moduleName.find("PolicyManager") != string::npos)
        {
            EnDisMessages(moduleName, enable);
        }
    }
}

void PolicyManagerMessageDebugger::EnDisMessages
(
    const string & ModuleNames,
    bool enable
)
{
    string::size_type start = 0;
    string::size_type end = ModuleNames.find_first_of(":,; |+.", start);
    while (start != end)
    {
        if (end == string::npos)
        {
            string ModuleName = ModuleNames.substr(start);
            Tee::EnDisModule(ModuleName.c_str(), enable);
            start = end;
        }
        else
        {
            string ModuleName = ModuleNames.substr(start, end - start);
            Tee::EnDisModule(ModuleName.c_str(), enable);
            start = ModuleNames.find_first_not_of(":,; |+.", end);
            end = ModuleNames.find_first_of(":,; |+.", start);
        }
    }
}

void PolicyManagerMessageDebugger::AddTeeModule
(
    const string & actionBlockName,
    const string & actionName
)
{
    string name = "PolicyManager_ActionBlock_" + actionBlockName + "(" + actionName + ")";
    if(!RealAddTeeModule(name)) return;

    // For the ActionBlock.OnTriggerCount() which action name is null
    if(!actionName.empty())
    {
        RealAddTeeModule("PolicyManager_" + actionName);
    }

    // For Policy.Define(Trigger.Start(), Action.Print("UVM_CE: start"));
    // which actionBlock name is empty
    if(!actionBlockName.empty())
    {
        RealAddTeeModule("PolicyManager_ActionBlock_" + actionBlockName);
    }

    RealAddTeeModule("PolicyManager");
}

bool PolicyManagerMessageDebugger::RealAddTeeModule
(
    const string & name
)
{
    UpdateName(const_cast<string *>(&name));

    map<string, TeeModule *>::iterator it = m_TeeModule.find(name);
    if (it != m_TeeModule.end())
    {
        return false;
    }

    bool enable = false;
    // if -d is enabled, enable all tee modules in PolicyManager parse stage
    if((Tee::LevDebug == CommandLine::ScreenLevel()) &&
       (Tee::LevDebug == CommandLine::FileLevel()) &&
       (Tee::LevDebug == CommandLine::DebuggerLevel()))
    {
        enable = true;
    }

    TeeModule * pModule = new TeeModule(name.c_str(), enable, "Mdiag");
    m_TeeModule[name] = pModule;

    return true;
}

UINT32 PolicyManagerMessageDebugger::GetTeeModuleCode
(
    const string & actionBlockName,
    const string & actionName
)
{
    string name = "";

    if(actionBlockName.empty() && actionName.empty())
    {
        return 0;
    }

    if(!actionBlockName.empty())
    {
        name = "PolicyManager_ActionBlock_" + actionBlockName + "(" + actionName + ")";
        UpdateName(&name);
        if((m_TeeModule.find(name) != m_TeeModule.end()) &&
            Tee::IsModuleEnabled(m_TeeModule[name]->GetCode()))
        {
            return m_TeeModule[name]->GetCode();
        }

        name = "PolicyManager_ActionBlock_" + actionBlockName;
        UpdateName(&name);
        if ((m_TeeModule.find(name) != m_TeeModule.end()) &&
            Tee::IsModuleEnabled(m_TeeModule[name]->GetCode()))
        {
            return m_TeeModule[name]->GetCode();
        }

    }
    else
    {
        name = "PolicyManager_(" + actionName + ")";
        UpdateName(&name);

        if((m_TeeModule.find(name) != m_TeeModule.end()) &&
            Tee::IsModuleEnabled(m_TeeModule[name]->GetCode()))
        {
            return m_TeeModule[name]->GetCode();
        }

        name = "PolicyManager_" + actionName;
        UpdateName(&name);
        if((m_TeeModule.find(name) != m_TeeModule.end()) &&
            Tee::IsModuleEnabled(m_TeeModule[name]->GetCode()))
        {
            return m_TeeModule[name]->GetCode();
        }
    }

    name = "PolicyManager";
    if(m_TeeModule.find(name) != m_TeeModule.end())
    {
        return m_TeeModule[name]->GetCode();
    }

    return 0;
}

void PolicyManagerMessageDebugger::UpdateName
(
    string * name
)
{
    // replace all '.' to '_' since '.' has been recongnized as the splitted symbol
    std::size_t found = name->find_first_of(".");
    while (found != string::npos)
    {
        (*name)[found] = '_';
        found = name->find_first_of(".", found + 1);
    }
}

PolicyManager::EngineInfo * PolicyManager::GetEngineInfo
(
    UINT32 subdevIdx,
    LwRm* pLwRm,
    UINT32 engineId
)
{
    EngineInfo * pInfo = NULL;
    auto keyEngineInfoMap = make_tuple(subdevIdx, pLwRm, engineId);

    if (m_EngineInfos.find(keyEngineInfoMap) != m_EngineInfos.end())
    {
        pInfo = m_EngineInfos[keyEngineInfoMap].get();
    }

    return pInfo;
}

RC PolicyManager::CreateEngineInfo
(
    GpuSubdevice * pGpuSubdevice,
    LwRm* pLwRm,
    UINT32 engineId
)
{
    RC rc;

    MASSERT(pGpuSubdevice);
    MASSERT(pLwRm);

    UINT32 subdevIdx = pGpuSubdevice->GetSubdeviceInst();
    if(GetEngineInfo(subdevIdx, pLwRm, engineId) == NULL)
    {
        LW2080_CTRL_GPU_GET_ENGINE_FAULT_INFO_PARAMS params = {0};
        params.engineType = engineId;

        CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdevice,
                    LW2080_CTRL_CMD_GPU_GET_ENGINE_FAULT_INFO,
                    (void *)&params,
                    sizeof(params)));

        unique_ptr<EngineInfo> pEngineInfo = make_unique<EngineInfo>();
        pEngineInfo->startMMUEngineId = params.mmuFaultId;
        pEngineInfo->bSubCtxAware = params.bSubcontextSupported;

        if (pEngineInfo->bSubCtxAware)
        {
            if (!pGpuSubdevice->IsSMCEnabled())
            {
                LW2080_CTRL_FIFO_GET_INFO_PARAMS fifoParams = {0};
                fifoParams.fifoInfoTblSize = 1;
                fifoParams.fifoInfoTbl[0].index = LW2080_CTRL_FIFO_INFO_INDEX_MAX_SUBCONTEXT_PER_GROUP;

                CHECK_RC(pLwRm->ControlBySubdevice(pGpuSubdevice,
                            LW2080_CTRL_CMD_FIFO_GET_INFO,
                            (void *)&fifoParams,
                            sizeof(fifoParams)));
                pEngineInfo->maxSubCtx = fifoParams.fifoInfoTbl[0].data;
            }
            else
            {
                pEngineInfo->maxSubCtx = GetMaxSubCtxForSmcEngine(pLwRm, pGpuSubdevice);
            }
        }
        auto keyEngineInfoMap = make_tuple(subdevIdx, pLwRm, engineId);
        m_EngineInfos[keyEngineInfoMap] = move(pEngineInfo);
    }

    return rc;
}

UINT32 PolicyManager::VEIDToMMUEngineId
(
    UINT32 VEID,
    GpuSubdevice * pGpuSubdevice,
    LwRm* pLwRm,
    UINT32 engineId
)
{
    UINT32 subdeviceId = pGpuSubdevice->GetSubdeviceInst();
    PolicyManager::EngineInfo * pEngineInfo = GetEngineInfo(subdeviceId, pLwRm, engineId);

    if(pEngineInfo == NULL)
    {
        RC rc = CreateEngineInfo(pGpuSubdevice, pLwRm, engineId);
        if (rc != 0)
        {
            ErrPrintf("%s: Can't get the Graphics Engine information.\n",
                      __FUNCTION__);
        }
        pEngineInfo = GetEngineInfo(subdeviceId, pLwRm, engineId);
    }

    MASSERT(pEngineInfo);

    if ((*pEngineInfo).bSubCtxAware)
    {

        if ((VEID >= 0) &&
                (VEID < pEngineInfo->maxSubCtx))
        {
            UINT32 MMUEngineId = VEID + (*pEngineInfo).startMMUEngineId;
            DebugPrintf("%s: VEID %02x = MMUEngineId %02x.\n",
                        __FUNCTION__, VEID, MMUEngineId);
            return MMUEngineId;
        }
        else
        {
            DebugPrintf("%s: The VEID (0x%x) is not the GRAPHICS ENGINE which "
                        "doesn't support veid. GRAPHICS ENGINE MMU fault range[0x%x, 0x%x]\n",
                        __FUNCTION__, VEID, (*pEngineInfo).startMMUEngineId,
                        (*pEngineInfo).startMMUEngineId + (*pEngineInfo).maxSubCtx);
            return UNITIALIZED_MMU_ENGINE_ID;
        }
    }

    DebugPrintf("%s: Not support the VEID.\n", __FUNCTION__);
    return UNITIALIZED_MMU_ENGINE_ID;
}

UINT32 PolicyManager::MMUEngineIdToVEID
(
    UINT32 MMUEngineId,
    GpuSubdevice * pGpuSubdevice,
    LwRm* pLwRm,
    UINT32 engineId
)
{
    UINT32 subdeviceId = pGpuSubdevice->GetSubdeviceInst();
    PolicyManager::EngineInfo * pEngineInfo = GetEngineInfo(subdeviceId, pLwRm, engineId);
    if(pEngineInfo == NULL)
    {
        RC rc = CreateEngineInfo(pGpuSubdevice, pLwRm, engineId);
        if (rc != 0)
        {
            ErrPrintf("%s: Can't get the Graphics Engine information.\n",
                      __FUNCTION__);
            return SubCtx::VEID_END;
        }
        pEngineInfo = GetEngineInfo(subdeviceId, pLwRm, engineId);
    }

    MASSERT(pEngineInfo);

    if ((*pEngineInfo).bSubCtxAware)
    {
        if ((MMUEngineId >= (*pEngineInfo).startMMUEngineId) &&
                (MMUEngineId < (*pEngineInfo).startMMUEngineId +
                 (*pEngineInfo).maxSubCtx))
        {
            UINT32 VEID = MMUEngineId - (*pEngineInfo).startMMUEngineId;
            DebugPrintf("%s: VEID %02x = MMUEngineId %02x.\n", __FUNCTION__, VEID, MMUEngineId);
            return VEID;
        }
        else
        {
            DebugPrintf("%s: The ENGINE ID (0x%x) is not the GRAPHICS ENGINE which "
                        "doesn't support veid. GRAPHICS ENGINE MMU fault range[0x%x, 0x%x]\n",
                        __FUNCTION__, MMUEngineId, (*pEngineInfo).startMMUEngineId,
                        (*pEngineInfo).startMMUEngineId + (*pEngineInfo).maxSubCtx);
            return SubCtx::VEID_END;
        }
    }

    DebugPrintf("%s: Engine %s does not support the MMU Engine Id.\n",
                pGpuSubdevice->GetParentDevice()->GetEngineName(engineId), __FUNCTION__);
    return SubCtx::VEID_END;
}

void PolicyManager::AddUtlMutex(void * mutex, const string & name)
{
    if (m_MutexLists.find(name) != m_MutexLists.end())
    {
        InfoPrintf("%s: Duplicated mutex %s.\n", __FUNCTION__, name.c_str());
        MASSERT(0);
        return;
    }

    m_MutexLists[name] = mutex;
}

Mutexes PolicyManager::GetUtlMutex(const RegExp & name)
{
    Mutexes mutexLists;
    for (auto it : m_MutexLists)
    {
        if (name.Matches(it.first))
        {
            mutexLists[it.first] = it.second;
        }
    }

    return mutexLists;
}

RC PolicyManager::DeleteUtlMutex(const string & name)
{
    for (auto it = m_MutexLists.begin();
            it != m_MutexLists.end(); )
    {
        if (it->first == name)
        {
            it = m_MutexLists.erase(it);
        }
        else
        {
            it ++;
        }
    }

    return OK;
}

bool PolicyManager::HasUpdateSurfaceInMmu(const string & name) const
{
    for (auto regName : m_OwnedSurfacesMmu)
    {
        if (regName.Matches(name))
        {
            return true;
        }
    }

    return false;
}

void PolicyManager::SavePteKind(PmMemRanges ranges)
{
    m_SavedPteKinds.push_back(make_pair(ranges, ranges[0].GetMdiagSurf()->GetPteKind()));
}
