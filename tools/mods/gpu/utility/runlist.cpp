/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2013,2015-2018 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "runlist.h"
#include "class/cl906e.h"  // GF100_CHANNEL_RUNLIST
#include "ctrl/ctrl0080.h" // LW0080_CTRL_CMD_FIFO_GET_CHANNELLIST
#include "gpu/include/gpudev.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "semawrap.h"
#include "runlwrap.h"
#include "core/include/utility.h"
#include "surfrdwr.h"
#include "core/include/version.h"
#include <algorithm>

//--------------------------------------------------------------------
bool Runlist::Entry::operator==(const Entry& Other) const
{
    return
        (pChannel == Other.pChannel) &&
        (GpPutLimit == Other.GpPutLimit) &&
        (Payload == Other.Payload) &&
        (SemId == Other.SemId);
}

//--------------------------------------------------------------------
//! \brief Constructor.
//!
Runlist::Runlist
(
    GpuDevice *pGpuDevice,
    LwRm *pLwRm,
    UINT32 Engine
) :
    m_pGpuDevice(pGpuDevice),
    m_pLwRm(pLwRm),
    m_Engine(Engine),
    m_TimeoutMs(Tasker::GetDefaultTimeoutMs()),
    m_RunlistReadWriter(m_Buffer),
    m_MaxEntries(0),
    m_SemaphoreReader(m_Semaphores),
    m_UseTimeStamp(true),
    m_LwrrentPayload(0),
    m_LwrrentPut(0),
    m_pMutex(Tasker::AllocMutex("Runlist::m_pMutex", Tasker::mtxUnchecked)),
    m_pFlushMutex(Tasker::AllocMutex("Runlist::m_pFlushMutex",
                                     Tasker::mtxUnchecked)),
    m_Frozen(false),
    m_NumFrozenEntries(0),
    m_BlockOnFlushCount(0),
    m_RecoveringFromRobustChannelError(false)
{
    MASSERT(pGpuDevice);
    MASSERT(pLwRm);
}

//--------------------------------------------------------------------
//! \brief Destructor.  Calls Free() in case runlist isn't already free
//!
Runlist::~Runlist()
{
    if (m_NumFrozenEntries > 0)
    {
        Printf(Tee::PriNormal,
               "Warning: Some frozen runlist entries were never written"
               " to the GPU in engine #%d\n",
               m_Engine);
    }
    {
        Tasker::MutexHolder mutexHolder1(m_pFlushMutex);
        Tasker::MutexHolder mutexHolder2(m_pMutex);
        Free();
    }
    Tasker::FreeMutex(m_pFlushMutex);
    Tasker::FreeMutex(m_pMutex);
}

//--------------------------------------------------------------------
//! \brief Return true if this runlist contains the indicated channel
//!
bool Runlist::ContainsChannel(Channel *pChannel) const
{
    // Don't look at m_Entries during the moment when the channel is
    // off the runlist during RecoverFromRobustChannelsError.
    Tasker::MutexHolder mutexHolder(m_pMutex);

    pChannel = pChannel->GetWrapper();
    for (Entries::const_iterator pEntry = m_Entries.begin();
         pEntry != m_Entries.end(); ++pEntry)
    {
        if ((*pEntry).pChannel == pChannel)
        {
            return true;
        }
    }
    return false;
}

// Tiny ad-hoc RAII class used by Runlist::Flush.  Near the start of
// Flush, we allocate a SemId from m_FreeSem.  This class
// restores the SemId to m_FreeSem if we exit prematurely.
//
class FreeSemIdOnError
{
public:
    FreeSemIdOnError(UINT32 SemId, deque<UINT32> *pFreeSem) :
        m_SemId(SemId), m_pFreeSem(pFreeSem) {}
    ~FreeSemIdOnError() { if (m_pFreeSem) m_pFreeSem->push_back(m_SemId); }
    void Cancel() { m_pFreeSem = NULL; }
private:
    UINT32 m_SemId;
    deque<UINT32> *m_pFreeSem;
};

//--------------------------------------------------------------------
//! \brief Called by RunlistChannelWrapper::Flush() to flush the channel
//!
//! This method does most of the work of RunlistChannelWrapper::Flush().
//! This method writes the backend semaphore, calls
//! pWrappedChanenl->Flush(), writes the runlist entry, and updates
//! RL_PUT.
//!
//! \param pWrappedChannel The m_pWrappedChannel member of the calling
//!     RunlistChannelWrapper object.
//!
RC Runlist::Flush(Channel *pWrappedChannel)
{
    MASSERT(pWrappedChannel != NULL);
    Channel *pChannel = pWrappedChannel->GetWrapper();
    RC rc;

    // Skip the runlist-related stuff if Channel::Flush is called from
    // RecoverFromRobustChannelError().
    //
    if (m_RecoveringFromRobustChannelError)
    {
        return pWrappedChannel->Flush();
    }

    // If the channel is on any other runlist, then we have to wait
    // until it's off the other runlist before we can add it to this
    // one.
    //
    Runlist *pOtherRunlist = m_pGpuDevice->GetRunlist(pChannel);
    if (pOtherRunlist != NULL && pOtherRunlist != this)
    {
        CHECK_RC(pOtherRunlist->WaitIdle(pChannel, m_TimeoutMs));
    }

    // Wait until there are at least two empty entries, and lock
    // m_pFlushMutex.
    //
    Tasker::MutexHolder FlushMutexHolder;
    FlushablePollArgs PollArgs = { this, OK };
    CHECK_RC(FlushMutexHolder.PollAndAcquire(FlushablePollFunc, &PollArgs,
                                             m_pFlushMutex,
                                             Tasker::NO_TIMEOUT));
    CHECK_RC(PollArgs.rc);

    // Load the new entry.  After this point, if we get an error, we
    // must restore NewEntry.SemId to m_FreeSem.  (Note: We don't
    // really need to lock m_pMutex, since nothing in this block can
    // call Tasker::Yield, but I want to emphasise which parts of this
    // function have to be atomic and which parts don't.)
    //
    Entry NewEntry(pChannel, 0, 0, 0);
    {
        Tasker::MutexHolder mutexHolder(m_pMutex);

        MASSERT(!m_FreeSem.empty());
        ++m_LwrrentPayload;
        NewEntry.Payload = m_LwrrentPayload;
        NewEntry.SemId = m_FreeSem.front();
        m_FreeSem.pop_front();
    }
    FreeSemIdOnError freeSemIdOnError(NewEntry.SemId, &m_FreeSem);

    // Push a backend semaphore
    //
    SemaphoreChannelWrapper *pSemWrap = pChannel->GetSemaphoreChannelWrapper();
    MASSERT(pSemWrap != NULL);

    CHECK_RC(pChannel->BeginInsertedMethods());
    {
        Utility::CleanupOnReturn<Channel>
            CleanupInsertedMethods(pChannel, &Channel::CancelInsertedMethods);
        UINT64 SemOffset = (m_Semaphores.GetCtxDmaOffsetGpu() +
                            NewEntry.SemId * GetSemSize() *
                            m_pGpuDevice->GetNumSubdevices());

        UINT32 flags = Channel::FlagSemRelWithWFI;
        if (m_UseTimeStamp)
            flags |= Channel::FlagSemRelWithTime;
        pChannel->SetSemaphoreReleaseFlags(flags);

        if (m_pGpuDevice->GetNumSubdevices() == 1)
        {
            CHECK_RC(pSemWrap->ReleaseBackendSemaphore(
                         SemOffset, NewEntry.Payload, true, NULL));
        }
        else
        {
            for (UINT32 sub = 0; sub < m_pGpuDevice->GetNumSubdevices(); ++sub)
            {
                CHECK_RC(pChannel->WriteSetSubdevice(1 << sub));
                CHECK_RC(pSemWrap->ReleaseBackendSemaphore(
                             SemOffset + sub * GetSemSize(),
                             NewEntry.Payload, true, NULL));
            }
            CHECK_RC(pChannel->WriteSetSubdevice(
                         Channel::AllSubdevicesMask));
        }
        CleanupInsertedMethods.Cancel();
    }
    CHECK_RC(pChannel->EndInsertedMethods());

    // Flush the wrapped channel
    //
    CHECK_RC(pWrappedChannel->Flush());

    // Update the runlist
    //
    CHECK_RC(pChannel->GetGpPut(&NewEntry.GpPutLimit));
    {
        Tasker::MutexHolder mutexHolder(m_pMutex);
        UINT32 NewPut = (m_LwrrentPut + 1) % m_MaxEntries;

        m_Entries.push_back(NewEntry);
        freeSemIdOnError.Cancel();

        CHECK_RC(WriteEntriesToBuffer(m_Entries.end()-1, m_Entries.end(),
                                      m_LwrrentPut));
        if (m_Frozen)
            ++m_NumFrozenEntries;
        else
            CHECK_RC(WriteRlPut(&m_Entries.back(), 1, NewPut));

        m_LwrrentPut = NewPut;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return number of entries that have been flushed to HW.
//!
UINT32 Runlist::UnfrozenEntries() const
{
    return static_cast<UINT32>(m_Entries.size()) - m_NumFrozenEntries;
}

//--------------------------------------------------------------------
//! \brief Wait until all runlist entries for this channel are done
//!
RC Runlist::WaitIdle(Channel *pChannel, FLOAT64 TimeoutMs)
{
    pChannel = pChannel->GetWrapper();

    // Search backwards from the end of the runlist.  If we find an
    // entry for this channel, then we'll wait until that entry has
    // been processed.
    //
    // If no entries are found for this channel, we'll wait until
    // m_Entries.size() entries are left -- which is pretty much a
    // no-op.
    //
    UINT32 NumEntriesToWaitFor;

    {
        // Don't look at m_Entries during the moment when the channel is
        // off the runlist during RecoverFromRobustChannelsError.
        Tasker::MutexHolder mutexHolder(m_pMutex);

        NumEntriesToWaitFor = static_cast<UINT32>(m_Entries.size());
        for (UINT32 ii = 0; ii < m_Entries.size(); ++ii)
        {
            if (m_Entries[m_Entries.size() - ii - 1].pChannel == pChannel)
            {
                NumEntriesToWaitFor = ii;
                break;
            }
        }
        // Don't hold the mutex during the whole wait period.
    }

    // Wait.
    //
    return WaitForNumEntries(NumEntriesToWaitFor, TimeoutMs);
}

//--------------------------------------------------------------------
//! \brief Wait until all runlist entries are done
//!
RC Runlist::WaitIdle(FLOAT64 TimeoutMs)
{
    return WaitForNumEntries(0, TimeoutMs);
}

//--------------------------------------------------------------------
//! \brief Freeze the runlist
//!
//! When the runlist is "frozen", any new entries are enqueued in
//! software, but not written to the hardware runlist.
//!
//! This feature is mostly used by PolicyManager for performance
//! testing, not as part of normal operations.
//!
//! \sa Unfreeze()
//! \sa WriteFrozenEntries()
//!
RC Runlist::Freeze()
{
    MASSERT(m_Frozen || m_NumFrozenEntries == 0);
    m_Frozen = true;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Unfreeze the runlist
//!
//! This method undoes Runlist::Freeze().  It appends the enqueued
//! methods (if any) to the runlist, and unfreezes the runlist.
//!
//! \sa Freeze()
//! \sa WriteFrozenEntries()
//!
RC Runlist::Unfreeze()
{
    MASSERT(m_Frozen || m_NumFrozenEntries == 0);
    RC rc;

    if (m_NumFrozenEntries > 0)
    {
        Tasker::MutexHolder mutexHolder(m_pMutex);
        CHECK_RC(WriteFrozenEntries(m_NumFrozenEntries));
        MASSERT(m_NumFrozenEntries == 0);
    }

    m_Frozen = false;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write some frozen entries to the runlist
//!
//! If the runlist is frozen, new entries are being enqueued rather
//! than appended to the runlist.  This method writes the indicated
//! number of oldest frozen entries to the runlist.
//!
//! The data should have been written to the runlist buffer when the
//! entry was enqueued, so this method just updates RL_PUT.
//!
//! \sa Freeze()
//! \sa Unfreeze()
//!
RC Runlist::WriteFrozenEntries(UINT32 NumEntries)
{
    MASSERT(m_Frozen || m_NumFrozenEntries == 0);
    MASSERT(m_NumFrozenEntries <= m_Entries.size());
    RC rc;

    if (m_NumFrozenEntries > 0 && NumEntries > 0)
    {
        Tasker::MutexHolder mutexHolder(m_pMutex);
        for (UINT32 ii = 0; ii < NumEntries && m_NumFrozenEntries > 0; ++ii)
        {
            Entry *pEntry = &m_Entries[m_Entries.size() - m_NumFrozenEntries];
            UINT32 Index = ((m_MaxEntries + m_LwrrentPut - m_NumFrozenEntries)
                            % m_MaxEntries);

            CHECK_RC(ValidateEntryInBuffer(pEntry, Index));
            CHECK_RC(WriteRlPut(pEntry, 1, (Index + 1) % m_MaxEntries));
            --m_NumFrozenEntries;
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Cause Flush() to block or unblock
//!
//! This method is used to temporarily prevent mods from calling
//! Flush().  When this method is called with Block==true, any thread
//! that calls Runlist::Flush (or any other method that locks
//! m_pFlushMutex) will block until this method is called with
//! Block==false.
//!
//! This method maintains a counter.  If it is called N times with
//! Block==true, it must be called N times with Block==false in order
//! to unblock Flush().
//!
//! Internally, this method simply increments or decrements
//! m_BlockOnFlushCount.  It is up to any other function that locks
//! m_pFlushMutex (except the destructor) to block until
//! m_BlockOnFlushCount == 0.
//!
//! This method is normally called by the RM (via a callback) to lock
//! RL_PUT during a few critical sections.  See bug 560946.
//!
void Runlist::BlockOnFlush(bool Block)
{
    // Primitive mutual exclusion.  Ideally, we'd lock m_pFlushMutex,
    // but this function may be called from an ISR.
    //
    Platform::Irql OldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);

    // Update m_BlockOnFlushCount
    //
    if (Block)
    {
        ++m_BlockOnFlushCount;
    }
    else
    {
        MASSERT(m_BlockOnFlushCount > 0);
        if (m_BlockOnFlushCount > 0)
        {
            --m_BlockOnFlushCount;
        }
    }

    Platform::LowerIrql(OldIrql);
}

//--------------------------------------------------------------------
//! \brief Return true if runlist is empty and no other task is flushing
//!
//! This method is normally called by the RM after BlockOnFlush() to
//! check whether the GPU has finished all pending runlist entries.
//! This assures the RM that the GPU won't do any more work until they
//! release BlockOnFlush().
//!
bool Runlist::IsBlockedAndFlushed()
{
    // Lock mutexes (nonblocking) to make sure that no other task is
    // lwrrently flushing.  Especially important since BlockOnFlush
    // may be called from an ISR while m_pFlushMutex is already
    // locked.
    //
    Tasker::MutexHolder flushMutexHolder;
    Tasker::MutexHolder mutexHolder;

    if (!flushMutexHolder.TryAcquire(m_pFlushMutex) ||
        !mutexHolder.TryAcquire(m_pMutex))
    {
        return false;
    }

    RC rc = RemoveOldEntries();
    MASSERT(rc == OK);
    return (rc == OK && m_Entries.size() == 0);
}

//--------------------------------------------------------------------
//! \brief Allocate the runlist
//!
RC Runlist::Alloc
(
    Memory::Location Location,
    UINT32 NumEntries
)
{
    MASSERT(NumEntries >= 2);
    MASSERT((NumEntries & (NumEntries - 1)) == 0);
    RC rc;

    // Lock m_pFlushMutex and m_pMutex
    //
    Tasker::MutexHolder flushMutexHolder;
    flushMutexHolder.PollAndAcquire(FlushUnblockedPollFunc, this,
                                    m_pFlushMutex, Tasker::NO_TIMEOUT);
    Tasker::MutexHolder mutexHolder(m_pMutex);

    // Do nothing if the runlist is already allocated
    //
    if (IsAllocated())
    {
        return rc;
    }
    MASSERT(m_hRunlist.empty());
    MASSERT(m_pUserD.empty());

    // If the alloc fails, then free the partially-allocated runlist
    // on exit.
    //
    Utility::CleanupOnReturn<Runlist> FreeOnError(this, &Runlist::Free);

    // Allocate the buffer that holds the runlist entries
    //
    m_Buffer.SetColorFormat(ColorUtils::VOID32);
    m_Buffer.SetWidth(NumEntries);
    m_Buffer.SetHeight(sizeof(Lw906eRunlistEntry) / sizeof(UINT32));
    m_Buffer.SetLocation(Location);
    CHECK_RC(m_Buffer.Alloc(m_pGpuDevice, m_pLwRm));

    MASSERT(m_Buffer.GetSize() % sizeof(Lw906eRunlistEntry) == 0);
    m_MaxEntries = (UINT32)(m_Buffer.GetSize() / sizeof(Lw906eRunlistEntry));
    MASSERT((m_MaxEntries & (m_MaxEntries - 1)) == 0);

    // Allocate the buffer that holds the back-end semaphores
    //
    m_Semaphores.SetColorFormat(ColorUtils::VOID32);
    m_Semaphores.SetWidth(m_MaxEntries * GetSemSize()/sizeof(UINT32));
    m_Semaphores.SetHeight(m_pGpuDevice->GetNumSubdevices());
    m_Semaphores.SetLocation(Location);
    CHECK_RC(m_Semaphores.Alloc(m_pGpuDevice, m_pLwRm));
    for (UINT32 sub = 0; sub < m_pGpuDevice->GetNumSubdevices(); ++sub)
        m_Semaphores.Fill(0, sub);

    MASSERT(m_Semaphores.GetSize() >=
            m_pGpuDevice->GetNumSubdevices() * m_MaxEntries * GetSemSize());

    // Record all available semaphores (per device) as free
    //
    MakeAllSemFree();

    m_LwrrentPayload = 0;

    // Allocate a runlist object for each subdevice, and map the
    // corresponding UserD
    //
    m_hRunlist.reserve(m_pGpuDevice->GetNumSubdevices());
    m_pUserD.reserve(m_pGpuDevice->GetNumSubdevices());
    for (UINT32 ii = 0; ii < m_pGpuDevice->GetNumSubdevices(); ++ii)
    {
        GpuSubdevice *pGpuSubdevice = m_pGpuDevice->GetSubdevice(ii);

        LwRm::Handle hRunlist = 0;
        LW_CHANNELRUNLIST_ALLOCATION_PARAMETERS RunlistAllocParams = {0};
        RunlistAllocParams.hRunlistBase = m_Buffer.GetMemHandle();
        RunlistAllocParams.engineID = m_Engine;
        CHECK_RC(m_pLwRm->Alloc(m_pLwRm->GetSubdeviceHandle(pGpuSubdevice),
                                &hRunlist, GF100_CHANNEL_RUNLIST,
                                &RunlistAllocParams));
        m_hRunlist.push_back(hRunlist);

        void *pUserD;
        CHECK_RC(m_pLwRm->MapMemory(hRunlist, 0, sizeof(Lw906eRunlistUserD),
                                    &pUserD, 0, pGpuSubdevice));
        m_pUserD.push_back(pUserD);
    }
    MASSERT(m_hRunlist.size() == m_pGpuDevice->GetNumSubdevices());
    MASSERT(m_pUserD.size() == m_pGpuDevice->GetNumSubdevices());

    m_LwrrentPut = 0;

    // Do an initial preempt.  Needed to finish initializing the RL in
    // the GPU.
    //
    Tasker::MutexHolder preemptLock;
    CHECK_RC(Preempt(NULL, &preemptLock));

    // If we get this far, then the alloc succeeded, so cancel the
    // auto-cleanup.
    //
    FreeOnError.Cancel();

    return rc;
}

//--------------------------------------------------------------------
//! \brief Free all resources, and return to an uninitialized state
//!
void Runlist::Free()
{
    MASSERT(m_pUserD.size() <= m_hRunlist.size());
    for (UINT32 ii = 0; ii < m_pUserD.size(); ++ii)
    {
        m_pLwRm->UnmapMemory(m_hRunlist[ii], m_pUserD[ii], 0,
                             m_pGpuDevice->GetSubdevice(ii));
    }
    m_pUserD.clear();
    m_LwrrentPut = 0;

    for (UINT32 ii = 0; ii < m_hRunlist.size(); ++ii)
    {
        m_pLwRm->Free(m_hRunlist[ii]);
    }
    m_hRunlist.clear();

    m_SemaphoreReader.Unmap();
    m_Semaphores.Free();
    m_LwrrentPayload = 0;

    m_RunlistReadWriter.Unmap();
    m_Buffer.Free();
    m_MaxEntries = 0;
    m_Entries.clear();
    m_FreeSem.clear();

    m_Frozen = false;
    m_NumFrozenEntries = 0;
}

//--------------------------------------------------------------------
//! \brief Return true if logging is enabled on any channel in the runlist
//!
bool Runlist::IsLoggingEnabled()
{
    for (Entries::iterator pEntry = m_Entries.begin();
         pEntry != m_Entries.end(); ++pEntry)
    {
        RunlistChannelWrapper *pWrapper =
            pEntry->pChannel->GetRunlistChannelWrapper();
        MASSERT(pWrapper != NULL);
        if (pWrapper->GetEnableLogging())
        {
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Wait until the runlist has no more than NumEntries on it.
//!
RC Runlist::WaitForNumEntries(UINT32 NumEntries, FLOAT64 TimeoutMs)
{
    RC rc;

    // Print a warning message if/when we run out of entries on
    // fmodel.  This usually won't happen, but if it does, and if we
    // can't clean up old entries for some reason (e.g. a huge atomic
    // operation that fills the entire runlist so that we can't insert
    // the backend semaphores), the fmodel can assert due to
    // inactivity before the timeout expires.  Without this warning,
    // the caller has no idea why the fmodel asserted.
    //
    if (Platform::GetSimulationMode() == Platform::Fmodel &&
        m_Entries.size() >= m_MaxEntries - 1)
    {
        Printf(
            Tee::PriNormal,
            "Warning: Runlist for engine %d has filled %d of %d entries.\n"
            "    Waiting for old entries to complete.  If the fmodel times\n"
            "    out, try using -runlistsize to increase the runlist size\n",
            m_Engine, (UINT32)m_Entries.size(), m_MaxEntries);
    }

    // Poll until enough entries have been reclaimed
    //
    WaitForNumEntriesPollArgs PollArgs = { this, NumEntries, OK };
    CHECK_RC(POLLWRAP(WaitForNumEntriesPollFunc, &PollArgs, TimeoutMs));
    if (rc == OK)
        rc = PollArgs.rc;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Polling function used to implement WaitForNumEntries()
//!
/* static */ bool Runlist::WaitForNumEntriesPollFunc(void *pPollArgs)
{
    WaitForNumEntriesPollArgs *pArgs =
        static_cast<WaitForNumEntriesPollArgs*>(pPollArgs);
    Runlist *pRunlist = pArgs->pRunlist;

    Tasker::MutexHolder mutexHolder(pRunlist->m_pMutex);

    // Clean up old entries.
    //
    pArgs->rc = pRunlist->RemoveOldEntries();
    if (pArgs->rc != OK)
    {
        return true;
    }

    // If we haven't cleaned up enough entries yet, then call
    // CheckError() on each channel so that we can respond to errors
    // while we wait.
    //
    if (pRunlist->m_Entries.size() > pArgs->NumEntries)
    {
        for (Entries::iterator pEntry = pRunlist->m_Entries.begin();
             pEntry != pRunlist->m_Entries.end(); ++pEntry)
        {
            pArgs->rc = pEntry->pChannel->CheckError();
            if (pArgs->rc != OK)
            {
                return true;
            }
        }
    }

    // Stop polling if we reach our goal.
    //
    MASSERT(pArgs->rc == OK);
    return (pArgs->NumEntries >= pRunlist->m_Entries.size());
}

//--------------------------------------------------------------------
//! Polling function that waits until Flush() is not blocked on
//! BlockOnFlush().
//!
/* static */ bool Runlist::FlushUnblockedPollFunc(void *pRunlist)
{
    return (((Runlist*)pRunlist)->m_BlockOnFlushCount == 0);
}

//--------------------------------------------------------------------
//! Polling function that waits until we can flush, which means that
//! Flush() is not blocked on BlockOnFlush(), and there are at least
//! two empty entries in the runlist.
//!
/* static */ bool Runlist::FlushablePollFunc(void *pFlushablePollArgs)
{
    FlushablePollArgs *pArgs = (FlushablePollArgs*)pFlushablePollArgs;
    Runlist *pRunlist = pArgs->pRunlist;

    // Remove old entries from the runlist, so that we can tell when
    // two entries are free.  Abort the poll on error.
    //
    pArgs->rc = pRunlist->RemoveOldEntries();
    if (pArgs->rc != OK)
        return true;

    // Return true if we can flush
    //
    if (pRunlist->m_BlockOnFlushCount == 0 &&
        pRunlist->m_Entries.size() <= pRunlist->m_MaxEntries - 2)
    {
        return true;
    }
    else
    {
        // Scan error for all channels in runlist
        for (Entries::iterator pEntry = pRunlist->m_Entries.begin();
            pEntry != pRunlist->m_Entries.end(); ++pEntry)
        {
            pArgs->rc = pEntry->pChannel->CheckError();
            if (pArgs->rc != OK)
            {
                return true;
            }
        }

        return false;
    }
}

//--------------------------------------------------------------------
//! \brief Remove old runlist entries
//!
//! Remove old runlist entries from m_Entries if their backend
//! semaphores have been released on all subdevices.
//!
//! As a sanity test, this method also checks to make sure the
//! semaphores get released in the right order, and MASSERTs if not.
//!
RC Runlist::RemoveOldEntries()
{
    const UINT32 NumSubdev = m_pGpuDevice->GetNumSubdevices();
    const UINT32 IlwalidValue = m_LwrrentPayload + 0x1000;
    UINT32 RemainingEntries = 0;
    RC rc;

    // For each subdevice, find out how many entries are still
    // running.  Set RemainingEntries to the maximum value.
    //
    for (UINT32 sub = 0; sub < NumSubdev; ++sub)
    {
        // Read all the pending backend semaphores
        //
        vector<UINT32> Contents(static_cast<size_t>(
            m_Semaphores.GetSize()/sizeof(UINT32)), IlwalidValue);
        m_SemaphoreReader.SetTargetSubdevice(sub);
        for (UINT32 EIdx = 0; EIdx < m_Entries.size(); EIdx++)
        {
            const Entry& e = m_Entries[EIdx];
            const UINT32 PayloadIdx = e.SemId*NumSubdev + sub;
            CHECK_RC(m_SemaphoreReader.ReadBytes(PayloadIdx*GetSemSize(),
                        &Contents[PayloadIdx*GetSemSize()/sizeof(UINT32)],
                        GetSemSize()));
        }

        // Find first entry which wasn't exelwted yet.
        //
        UINT32 EntryIdx = 0;
        for ( ; EntryIdx < m_Entries.size(); EntryIdx++)
        {
            const Entry& e = m_Entries[EntryIdx];
            const UINT32 PayloadIdx = (e.SemId*NumSubdev + sub) *
                    GetSemSize()/sizeof(UINT32);
            const UINT32 Payload = Contents[PayloadIdx];
            MASSERT(Payload != IlwalidValue);
            if (Payload != e.Payload)
            {
                break;
            }

            // Ensure exelwtion order.
            //
            if ((EntryIdx > 0) && m_UseTimeStamp)
            {
                const Entry& PrevEntry = m_Entries[EntryIdx-1];
                const UINT32 PrevPayloadIdx =
                    (PrevEntry.SemId * NumSubdev + sub) *
                    GetSemSize() / sizeof(UINT32);
                const UINT64 TimeStamp =
                    *reinterpret_cast<UINT64*>(&Contents[PayloadIdx+2]);
                const UINT64 PrevTimeStamp =
                    *reinterpret_cast<UINT64*>(&Contents[PrevPayloadIdx+2]);
                MASSERT(TimeStamp && PrevTimeStamp);
                if (PrevTimeStamp > TimeStamp)
                {
                    Printf(Tee::PriHigh,
                           "Runlist entry was exelwted out of order on"
                           " subdev %u.  ChannelId=%u, GpPutLimit=0x%08x,"
                           " SemaphoreOffset=0x%08llx,"
                           " TimeStamp=0x%llx,"
                           " NextSemaphoreOffset=0x%08llx,"
                           " NextTimeStamp=0x%llx\n",
                           sub,
                           PrevEntry.pChannel->GetChannelId(),
                           PrevEntry.GpPutLimit,
                           (m_Semaphores.GetCtxDmaOffsetGpu()
                            + PrevPayloadIdx * sizeof(UINT32)),
                           PrevTimeStamp,
                           (m_Semaphores.GetCtxDmaOffsetGpu()
                            + PayloadIdx * sizeof(UINT32)),
                           TimeStamp);
                    MASSERT(!"Runlist entry exelwted out of order!");
                }
            }
        }

        // Make sure all the remaining entries haven't exelwted yet.
        //
        for (UINT32 Idx2 = EntryIdx; Idx2 < m_Entries.size(); Idx2++)
        {
            const Entry& e = m_Entries[Idx2];
            const UINT32 PayloadIdx = (e.SemId*NumSubdev + sub) *
                    GetSemSize()/sizeof(UINT32);
            const UINT32 Payload = Contents[PayloadIdx];
            if (Payload == e.Payload)
            {
                Printf(Tee::PriHigh,
                       "Runlist entry was exelwted out of order on"
                       " subdev %u.  ChannelId=%u, GpPutLimit=0x%08x,"
                       " SemaphoreOffset=0x%08llx\n",
                       sub, e.pChannel->GetChannelId(), e.GpPutLimit,
                       (m_Semaphores.GetCtxDmaOffsetGpu() +
                        PayloadIdx * sizeof(UINT32)));
                MASSERT(!"Runlist entry exelwted out of order!");
            }
        }
        RemainingEntries = max(RemainingEntries,
                               static_cast<UINT32>(m_Entries.size() -
                                                   EntryIdx));
    }

    // Remove old entries from m_Entries, and add their backend
    // semaphores to m_FreeSem so we can reuse them.
    //
    while (m_Entries.size() > RemainingEntries)
    {
        if (IsLoggingEnabled() &&
            (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
        {
            Printf(Tee::PriNormal,
                   "DEV_%x_RL_%x: finished entry[0x%x]:"
                   " chid 0x%02x, gpPutLimit 0x%04x\n",
                   m_pGpuDevice->GetDeviceInst(), m_Engine,
                   (m_LwrrentPut - (UINT32)m_Entries.size() + m_MaxEntries) %
                   m_MaxEntries,
                   m_Entries.front().pChannel->GetChannelId(),
                   m_Entries.front().GpPutLimit);
        }
        m_FreeSem.push_back(m_Entries.front().SemId);
        m_Entries.pop_front();
    }
    MASSERT(RemainingEntries == m_Entries.size());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write several contiguous entries to the buffer
//!
//! \param pEntries The entries to write
//! \param NumEntries Number of entries to write
//! \param Index Index of first entry in the buffer to write.
//!
RC Runlist::WriteEntriesToBuffer
(
    Entries::const_iterator Begin,
    Entries::const_iterator End,
    UINT32 Index
)
{
    const size_t NumEntries = End - Begin;
    MASSERT(Index + NumEntries <= m_MaxEntries);
    RC rc;

    // Construct the buffer entries
    //
    const UINT32 EntrySize = sizeof(Lw906eRunlistEntry);
    vector<UINT08> BufferEntries;
    for (Entries::const_iterator EntryIt=Begin; EntryIt != End; ++EntryIt)
    {
        Lw906eRunlistEntry BufferEntry = {0};
        BufferEntry.ChID = EntryIt->pChannel->GetChannelId();
        BufferEntry.GpPutLimit = EntryIt->GpPutLimit;
        const UINT08* const EntryPtr = const_cast<UINT08*>(
            reinterpret_cast<volatile UINT08*>(&BufferEntry));
        BufferEntries.insert(BufferEntries.end(), EntryPtr,
                             EntryPtr + sizeof(BufferEntry));
    }

    // Write the buffer entries
    //
    CHECK_RC(m_RunlistReadWriter.WriteBytes(
                        Index * EntrySize,
                        &BufferEntries[0],
                        NumEntries * EntrySize));

    // Log the new entries
    //
    if (IsLoggingEnabled() &&
        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
    {
        UINT32 i = Index;
        for (Entries::const_iterator EntryIt=Begin;
             EntryIt != End; ++EntryIt, i++)
        {
            Printf(Tee::PriNormal,
                   "DEV_%x_RL_%x: wrote entry[0x%x]:"
                   " chid 0x%02x gpPutLimit 0x%04x\n",
                   m_pGpuDevice->GetDeviceInst(), m_Engine, i,
                   EntryIt->pChannel->GetChannelId(), EntryIt->GpPutLimit);
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Confirm that an entry in the buffer is what we expect
//!
//! This method should be used if we already called
//! WriteEntriesToBuffer() to write an entry, and we want to make sure
//! that the entry still contains the data we wrote.  Return
//! DATA_MISMATCH if they don't match.
//!
//! \param pEntry The entry that we previously wrote
//! \param Index Which entry in the buffer we previously wrote
//!
RC Runlist::ValidateEntryInBuffer(const Runlist::Entry *pEntry, UINT32 Index)
{
    MASSERT(pEntry != NULL);
    MASSERT(Index < m_MaxEntries);
    RC rc;

    // Construct the buffer entry that we expect
    //
    Lw906eRunlistEntry BufferEntry = {0};
    BufferEntry.ChID = pEntry->pChannel->GetChannelId();
    BufferEntry.GpPutLimit = pEntry->GpPutLimit;

    // Compare our entry to the actual data in the buffer
    //
    for (UINT32 ii = 0; ii < m_pGpuDevice->GetNumSubdevices(); ++ii)
    {
        Lw906eRunlistEntry ActualEntry;

        m_RunlistReadWriter.SetTargetSubdevice(ii);
        CHECK_RC(m_RunlistReadWriter.ReadBytes(
                            Index * sizeof(ActualEntry),
                            const_cast<void*>((volatile void*)&ActualEntry),
                            sizeof(ActualEntry)));

        if (0 != memcmp(const_cast<void*>((volatile void*)&BufferEntry),
                        const_cast<void*>((volatile void*)&ActualEntry),
                        sizeof(BufferEntry)))
        {
            return RC::DATA_MISMATCH;
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write the RL_PUT pointer
//!
//! \param pEntry The runlist entry we're adding.  Lwrrently, this
//!     method assumes that we only advance RL_PUT by one.
//! \param NewPut The new RL_PUT value to write.  NewPut should point at
//!     the entry *after* the one we want to send to the GPU, so if we
//!     just called WriteEntriesToBuffer() with some Index, then NewPut
//!     should be ((Index+1) % m_MaxEntries).
//!
RC Runlist::WriteRlPut(const Entry *pEntry, UINT32 NumEntries, UINT32 NewPut)
{
    MASSERT(NewPut < m_MaxEntries);
    MASSERT(NumEntries < m_MaxEntries);
    RC rc;

    // Check for RC errors before updating RL_PUT (sending a RL entry down
    // after an RC error will likely cause hangs since the channel cannot
    // continue to process data after an RC error)
    //
    if (pEntry != NULL && !m_RecoveringFromRobustChannelError)
    {
        CHECK_RC(pEntry->pChannel->CheckError());
    }

    // Callwlate the previous value of RL_PUT.
    //
    UINT32 OldPut = (NewPut + m_MaxEntries - NumEntries) % m_MaxEntries;

    // Flush WC to make sure the GPU can read the buffer entries
    //
    Platform::FlushCpuWriteCombineBuffer();

    // Update the buffer's put-pointer
    //
    for (UINT32 ii = 0; ii < m_pGpuDevice->GetNumSubdevices(); ++ii)
    {
        Lw906eRunlistUserD *pUserD = (Lw906eRunlistUserD*)(m_pUserD[ii]);
        MASSERT(MEM_RD32(&pUserD->Put) == OldPut);
        MEM_WR32(&pUserD->Put, NewPut);
    }

    // Log the new RL_PUT value
    //
    if (IsLoggingEnabled() &&
        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
    {
        Printf(Tee::PriNormal, "DEV_%x_RL_%x: runlist put was 0x%x now 0x%x\n",
               m_pGpuDevice->GetDeviceInst(), m_Engine, OldPut, NewPut);
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Preempt the runlist
//!
//! \param pEntries Optional vector which will contain unfinished
//!                 entries upon return.  The list can then be
//!                 rearranged and passed back to Resubmit().  This
//!                 pointer can be zero if we don't intend to resubmit
//!                 the entries.
//!
//! \param pLock Pointer to a mutex holder object which will be used to lock
//!              the flush mutex. This is used in PmActionBlock::Execute() to
//!              guarantee that the mutex will stay locked for the exelwtion
//!              of all actions in the action block, including Resubmit()
//!              actions following Preempt(). This will guarantee that no flush
//!              is performed while the runlist is preempted.
//!
//! \sa Resubmit()
//!
RC Runlist::Preempt
(
    Entries* pEntries,
    Tasker::MutexHolder* pLock
)
{
    // Guarantee atomicity of Preempt/Resubmit pair.
    // It is expected that the Lock objects releases the mutex
    // after all actions in an action block complete.
    //
    MASSERT(pLock);
    pLock->PollAndAcquire(FlushUnblockedPollFunc, this,
                          m_pFlushMutex, Tasker::NO_TIMEOUT);
    Tasker::MutexHolder mutexHolder(m_pMutex);
    RC rc;

    // Tell the RM to preempt
    //
    LW0080_CTRL_FIFO_PREEMPT_RUNLIST_PARAMS preemptRunlistParams = {0};
    preemptRunlistParams.engineID = m_Engine;
    CHECK_RC(m_pLwRm->ControlByDevice(
                         m_pGpuDevice,
                         LW0080_CTRL_CMD_FIFO_PREEMPT_RUNLIST,
                         &preemptRunlistParams, sizeof(preemptRunlistParams)));

    // Copy the old runlist entries to *pEntries
    //
    if (pEntries != NULL)
    {
        // Release entries which were already done
        //
        CHECK_RC(RemoveOldEntries());

        // Copy the remaining entries
        //
        m_SavedEntries.swap(m_Entries);
        *pEntries = m_SavedEntries;

        // Remove subsequent entries for the same channel to avoid temptation
        // of splitting them by the caller
        //
        for (UINT32 i = 0; i+1 < pEntries->size(); )
        {
            if ((*pEntries)[i].pChannel == (*pEntries)[i+1].pChannel)
            {
                pEntries->erase(pEntries->begin()+i);
            }
            else
            {
                i++;
            }
        }
    }

    m_Entries.clear();
    m_LwrrentPut = 0;
    m_NumFrozenEntries = 0;

    MakeAllSemFree();

    return rc;
}

//--------------------------------------------------------------------
//! \brief Ensure the caller who resubmits entries does not cheat
//!
RC Runlist::ValidateResubmittedEntries(const Entries& NewEntries) const
{
    for (Entries::const_iterator NewEntry = NewEntries.begin();
         NewEntry != NewEntries.end(); ++NewEntry)
    {
        // Check pending channel errors.
        // Otherwise updating put pointer would fail.
        //
        RC rc;
        CHECK_RC(NewEntry->pChannel->CheckError());

        // Find the entry in the saved list
        //
        const Entries::const_iterator OldEntry =
            find(m_SavedEntries.begin(), m_SavedEntries.end(), *NewEntry);
        if (OldEntry == m_SavedEntries.end())
        {
            Printf(Tee::PriHigh,
                   "Resubmitted runlist entry not on the original runlist!\n");
            MASSERT(!"Unrecognized entry passed to Runlist::Resubmit()");
            return RC::SOFTWARE_ERROR;
        }

        // Find previous entry on the same channel
        //
        Entries::const_reverse_iterator PrevEntry(NewEntry);
        for ( ; PrevEntry != NewEntries.rend(); ++PrevEntry)
        {
            if (PrevEntry->pChannel == NewEntry->pChannel)
            {
                break;
            }
        }

        // Find the same entry on the saved list, preceding the old entry
        //
        if (PrevEntry != NewEntries.rend())
        {
            const Entries::const_iterator PrevOldEntry =
                find(m_SavedEntries.begin(), OldEntry, *PrevEntry);
            if (PrevOldEntry == OldEntry)
            {
                Printf(Tee::PriHigh,
                       "Resubmitted runlist contains entries"
                       " reordered on the same channel!\n");
                MASSERT(!"Reordering entries on same channel is not allowed");
                return RC::SOFTWARE_ERROR;
            }
        }
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Resubmit entries after preemption
//!
//! This function can be only called after prior call to
//! Runlist::Preempt() with nonzero pEntries argument. In this
//! case Runlist::Preempt() will return a list of entries which can be
//! rearranged and resubmitted with this function.
//!
//! The resubmitted entries must have been on the list returned
//! by Runlist::Preempt(). Entries for a particular channel must be
//! in the same order they were before and subsequent entries for
//! the same channel must still be subsequent.
//!
//! \sa Preempt()
//!
RC Runlist::Resubmit(const Entries& NewEntries)
{
    MASSERT(m_LwrrentPut == 0);
    MASSERT(m_Entries.empty());
    MASSERT(m_NumFrozenEntries == 0);
    RC rc;

    Tasker::MutexHolder mutexHolder(m_pMutex);

    // Check if the resubmitted list of entries matches
    // the one returned by last call to Preempt()
    //
    m_Entries = NewEntries; // WAR for bad djgpp v1 deque implementation
    CHECK_RC(ValidateResubmittedEntries(m_Entries));

    // Restore previous entries on the same channel
    //
    m_Entries.clear();
    for (Entries::const_iterator Entry = NewEntries.begin();
         Entry != NewEntries.end(); ++Entry)
    {
        // Find the same entry on the saved list
        const Entries::const_iterator OldEntry =
            find(m_SavedEntries.begin(), m_SavedEntries.end(), *Entry);

        // Find any preceding entries on the same channel
        Entries::const_iterator FirstOldEntry = OldEntry;
        while (FirstOldEntry != m_SavedEntries.begin())
        {
            --FirstOldEntry;
            if (FirstOldEntry->pChannel != OldEntry->pChannel)
            {
                ++FirstOldEntry;
                break;
            }
        }

        // Insert preceding entries on the same channel
        m_Entries.insert(m_Entries.end(), FirstOldEntry, OldEntry);

        // Insert current entry
        m_Entries.push_back(*Entry);
    }
    m_SavedEntries.clear();

    // Remove previously used semaphores from the free list
    //
    for (UINT32 j=0; j < m_Entries.size(); j++)
    {
        deque<UINT32>::iterator it =
            find(m_FreeSem.begin(), m_FreeSem.end(), m_Entries[j].SemId);
        MASSERT(it != m_FreeSem.end());
        m_FreeSem.erase(it);
    }

    // Write the entries
    //
    const UINT32 NumEntries = static_cast<UINT32>(m_Entries.size());
    CHECK_RC(WriteEntriesToBuffer(m_Entries.begin(), m_Entries.end(), 0));
    if (m_Frozen)
    {
        m_NumFrozenEntries = NumEntries;
    }
    else
    {
        CHECK_RC(WriteRlPut(0, NumEntries, NumEntries));
    }

    m_LwrrentPut = NumEntries;

    return OK;
}

//--------------------------------------------------------------------
//! \brief Recover after a robust-channel error
//!
//! Called by RunlistChannelWrapper::RecoverFromRobustChannelError()
//! to do recovery on the wrapped channel *and* on the runlist object.
//!
//! When a robust-channel error oclwrs on a channel, the RM removes
//! all entries from the runlist that the channel was on (including
//! entries for channels that didn't have any error), just as if a
//! preempt oclwrred.  It also resets the GpFifo to the quiescent
//! state, so that GpGet=GpPut=0.
//!
//! Unfortunately, mods can't simply free the faulting channel at this
//! point.  The GPU has stored information about where the channel was
//! interrupted, which needs to be cleaned up.  At a bare minimum,
//! mods has to submit an empty GPFIFO entry.  But more maximum
//! compatibility with KMD, we've been asked to submit GPFIFO entries
//! that release the semaphores, to make sure that the GPU can handle
//! that.
//!
//! So, this method clears the GpFifo on all faulting channels, writes
//! token gpfifo entries to the faulting channels, and resubmits all
//! channels that were on the runlist before the error.
//!
//! \param pWrappedChannel The m_pWrappedChannel member of the calling
//!     RunlistChannelWrapper object.
//!
RC Runlist::RecoverFromRobustChannelError(Channel *pWrappedChannel)
{
    Channel *pFaultingChannel = pWrappedChannel->GetWrapper();
    RC rc;

    // Wait for all pending runlist operations to finish.
    //
    // In two-stage robust-channel error recovery (the standard for
    // Vista and runlists), it's OK for the code to keep updating
    // GP_PUT and RL_PUT for a while after the RM has detected an
    // error.  So we'll let any pending flushes and whatnot finish so
    // that we can have a "clean" runlist during recovery.
    //
    // Once we tell the RM that we're recovering, in
    // m_pWrappedChannel->RecoverFromRobustChannelError() below, then
    // we're committed to finishing the recovery process.  So we lock
    // the mutexes first, to ensure a clean recovery.
    //
    Tasker::MutexHolder flushMutexHolder;
    flushMutexHolder.PollAndAcquire(FlushUnblockedPollFunc, this,
                                    m_pFlushMutex, Tasker::NO_TIMEOUT);
    Tasker::MutexHolder mutexHolder(m_pMutex);

    // Do error recovery on the channel.
    //
    CHECK_RC(pWrappedChannel->RecoverFromRobustChannelError());

    // If we're calling this function relwrsively, presumably due to
    // the CheckError() calls below, then stop after doing recovery on
    // the channel.  The caller will do recovery on the runlist
    // itself.
    //
    if (m_RecoveringFromRobustChannelError)
    {
        return rc;
    }
    Utility::TempValue<bool> Tmp(m_RecoveringFromRobustChannelError, true);

    // Assume that the runlist was reset if RL_PUT == RL_GET == 0 on
    // all subdevices.  (It's possible for the runlist to be in that
    // state even if it wasn't explicitly reset, but since that's the
    // reset state anyways, the difference is moot.)
    //
    bool RunlistWasReset = true;
    for (UINT32 ii = 0; ii < m_pGpuDevice->GetNumSubdevices(); ++ii)
    {
        Lw906eRunlistUserD *pUserD = (Lw906eRunlistUserD*)(m_pUserD[ii]);
        if (MEM_RD32(&pUserD->Put) != 0 || MEM_RD32(&pUserD->Get) != 0)
        {
            RunlistWasReset = false;
            break;
        }
    }

    // Find out whether the faulting channel was on the runlist when
    // the runlist was reset.
    //
    CHECK_RC(RemoveOldEntries());
    bool FaultingChannelWasOnRunlist = false;
    for (Entries::iterator pEntry = m_Entries.begin();
         pEntry != m_Entries.end(); ++pEntry)
    {
        if (pFaultingChannel == pEntry->pChannel)
        {
            FaultingChannelWasOnRunlist = true;
            break;
        }
    }

    // If a channel gets an error while on the runlist, the RM should
    // always reset the runlist.  MASSERT if this condition isn't met.
    //
    MASSERT(RunlistWasReset || !FaultingChannelWasOnRunlist);

    // If the runlist was reset, then replace the pushbuffers of all
    // faulting channels with trivial pushbuffers that simply release
    // the semaphores.
    //
    if (RunlistWasReset)
    {
        set<Channel*> FaultingChannels;
        UINT32 NumEntries;

        // Find the faulting channels
        //
        for (Entries::iterator pEntry = m_Entries.begin();
             pEntry != m_Entries.end(); ++pEntry)
        {
            if (pEntry->pChannel->CheckError() != OK)
            {
                FaultingChannels.insert(pEntry->pChannel);
            }
        }

        // Push dummy GPFIFO entries on the faulting channels
        //
        for (set<Channel*>::iterator ppChannel = FaultingChannels.begin();
             ppChannel !=FaultingChannels.end(); ++ppChannel)
        {
            Channel *pChannel = *ppChannel;
            UINT32 GpPut = 0;

            // Clear the pushbuffer info.  This was already done in
            // hardware when the robust-channel error was detected; we're
            // just updating software to match.
            //
            pChannel->ClearPushbuffer();

            for (Entries::iterator pEntry = m_Entries.begin();
                 pEntry != m_Entries.end(); ++pEntry)
            {
                if (pEntry->pChannel == pChannel)
                {
                    CHECK_RC(WriteRobustChannelRecoverySemaphore(pChannel,
                                                                 &(*pEntry)));
                    ++GpPut;
                    pEntry->GpPutLimit = GpPut;
                }
            }
        }

        // Re-load the runlist
        //
        NumEntries = (UINT32)m_Entries.size();
        CHECK_RC(WriteEntriesToBuffer(m_Entries.begin(), m_Entries.end(), 0));
        if (m_Frozen)
        {
            m_NumFrozenEntries = NumEntries;
        }
        else
        {
            CHECK_RC(WriteRlPut(0, NumEntries, NumEntries));
        }
        m_LwrrentPut = NumEntries;
    }

    if (!FaultingChannelWasOnRunlist)
    {
        MASSERT(m_Entries.size() < m_MaxEntries);
        MASSERT(!m_FreeSem.empty());

        Entry newEntry(pFaultingChannel, 1,
                       ++m_LwrrentPayload, m_FreeSem.front());
        UINT32 NewPut = (m_LwrrentPut + 1) % m_MaxEntries;

        pFaultingChannel->ClearPushbuffer();
        CHECK_RC(WriteRobustChannelRecoverySemaphore(pFaultingChannel,
                                                     &newEntry));
        m_Entries.push_back(newEntry);
        m_FreeSem.pop_front();
        CHECK_RC(WriteEntriesToBuffer(m_Entries.end() - 1, m_Entries.end(),
                                      m_LwrrentPut));
        if (m_Frozen)
            ++m_NumFrozenEntries;
        else
            CHECK_RC(WriteRlPut(&newEntry, 1, NewPut));

        m_LwrrentPut = NewPut;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Marks all semaphores as free.
//!
void Runlist::MakeAllSemFree()
{
    m_FreeSem.clear();
    for (UINT32 i = 0; i < m_MaxEntries; i++)
    {
        m_FreeSem.push_back(i);
    }
}

// Ad-hoc RAII class used by Runlist::WriteRobustChannelRecoverySemaphore.
// Temporarily clears a channel error, and then restores the original
// error in the destructor.
//
class TempClearChannelError
{
public:
    TempClearChannelError(Channel *pCh) :
        m_pCh(pCh), m_OldError(pCh->CheckError()) { pCh->ClearError(); }
    ~TempClearChannelError() { m_pCh->SetError(m_OldError); }
private:
    Channel *m_pCh;
    RC m_OldError;
};

//--------------------------------------------------------------------
//! \brief Helper function that re-writes a semaphore
//!
//! This method is called by RecoverFromRobustChannelError().  After a
//! channel is reset, one part of recovery is to restore the channel's
//! entries on the runlist, but without all the methods that were in
//! the pushbuffer -- just the backend semaphores at the end of each
//! entry.  This method does the task of re-writing a backend
//! semaphore on a channel.
//!
//! Since the channel's SetObject methods were forgotten in the reset,
//! this method writes host semaphores.  And since the channel has a
//! pending error (i.e. CheckError returns non-OK), this method
//! temporarily clears the error in order to write the methods.
//!
//! \param pChannel The channel to re-write the sempahore on
//! \param pEntry The runlist entry containing the semaphore we're re-writing.
//!
RC Runlist::WriteRobustChannelRecoverySemaphore
(
    Channel *pChannel,
    const Entry *pEntry
)
{
    RC rc;
    UINT64 SemOffset = (m_Semaphores.GetCtxDmaOffsetGpu() +
                        pEntry->SemId * m_pGpuDevice->GetNumSubdevices() *
                        GetSemSize());

    // Temporarily clear the channel error
    //
    TempClearChannelError Tmp(pChannel);

    // Call pChannel->BeginInsertedMethods(), and make sure we call
    // CancelInsertedMethods if we get an error before we can call the
    // matching EndInsertedMethods
    //
    CHECK_RC(pChannel->BeginInsertedMethods());
    Utility::CleanupOnReturn<Channel>
        CancelInsertedMethodsOnErr(pChannel, &Channel::CancelInsertedMethods);

    // Write the semaphore
    //
    pChannel->SetSemaphoreReleaseFlags(
        Channel::FlagSemRelWithWFI |
        (m_UseTimeStamp ? Channel::FlagSemRelWithTime : 0));
    CHECK_RC(pChannel->SetSemaphorePayloadSize(Channel::SEM_PAYLOAD_SIZE_32BIT));
    if (m_pGpuDevice->GetNumSubdevices() == 1)
    {
        CHECK_RC(pChannel->SetSemaphoreOffset(SemOffset));
        CHECK_RC(pChannel->SemaphoreRelease(pEntry->Payload));
    }
    else
    {
        for (UINT32 sub = 0; sub < m_pGpuDevice->GetNumSubdevices(); ++sub)
        {
            CHECK_RC(pChannel->WriteSetSubdevice(1 << sub));
            CHECK_RC(pChannel->SetSemaphoreOffset(
                         SemOffset + sub * GetSemSize()));
            CHECK_RC(pChannel->SemaphoreRelease(pEntry->Payload));
        }
    }
    CHECK_RC(pChannel->Flush());

    // Call pChannel->EndInsertedMethods(), which corresponds to the
    // BeginInsertedMethods we called above.
    //
    CancelInsertedMethodsOnErr.Cancel();
    CHECK_RC(pChannel->EndInsertedMethods());

    return rc;
}
