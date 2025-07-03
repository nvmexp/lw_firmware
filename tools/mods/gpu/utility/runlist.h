/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_RUNLIST_H
#define INCLUDED_RUNLIST_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif
#ifndef INCLUDED_SURF2D_H
#include "surf2d.h"
#endif
#ifndef INCLUDED_STL_DEQUE
#include <deque>
#define INCLUDED_STL_DEQUE
#endif
#ifndef INCLUDED_SURFRDWR_H
#include "surfrdwr.h"
#endif

//--------------------------------------------------------------------
//! \brief Runlist for a single engine
//!
//! This class implements a runlist for one engine.  It's mostly used
//! by the RunlistChannelWrapper class, which automagically takes care
//! of updating the runlist as part of Channel::Flush().  Callers
//! should not create their own instances of this class, but use the
//! GetRunlist methods in GpuDevice instead.
//!
//! BRIEF OVERVIEW OF RUNLISTS:
//!
//! Starting in fermi, the GPUs have two ways to schedule channels:
//! legacy and "basic prime" runlists.
//!
//! Legacy scheduling is what was used to in pre-fermi chips:
//! (1) Mods allocates a bunch of channels
//! (2) We use SetObject() to assign objects to the subchannels.
//! (3) We push a bunch of methods on the channel for the various
//!     objects, which may or may not be on the same engine.
//! (4) We flush the whole thing to the GPU.
//! (5) The Host engine automagically passes the methods on each
//!     channel to the various engines.  If two channels have methods
//!     for the same engine, Host automagically decides which ones go
//!     first.
//!
//! Basic prime moves some of the Host engine's responsibility to
//! software.  The steps are mostly the same; the differences are
//! indicated in capital letters:
//! (1) Mods allocates a bunch of channels
//! (2) We use SetObject() to assign objects to the subchannels.
//! (3) We push a bunch of methods on the channel for the various
//!     objects.  THE NON-HOST METHODS MUST ALL BE FOR THE SAME
//!     ENGINE.
//! (4) We flush the whole thing to the GPU.
//! (5) WE ADD THE CHANNEL TO THE ENGINE'S RUNLIST.  EACH ENGINE HAS
//!     A CIRLWLAR RUNLIST ASSOCIATED WITH IT.
//! (6) HOST PASSES METHODS TO EACH ENGINE IN A VERY STRAIGHTFORWARD
//!     WAY: IT POPS THE NEXT ENTRY FROM THE RUNLIST, READS A CHANNEL
//!     FROM THE RUNLIST ENTRY, PASSES METHODS FROM THAT CHANNEL TO
//!     THE ENGINE, AND THEN CONTINUES TO THE NEXT RUNLIST ENTRY.
//!
//! ABOUT BACK-END SEMAPHORES:
//!
//! One unfortunate feature of runlists is that there's no built-in
//! way for mods to tell when the engine has finished processing a
//! runlist entry, so there's no built-in way to tell when we can
//! re-use the entry.  It's not enough to look at the runlist's "get"
//! pointer, because that only tells when Host has finished reading
//! the entry; the engine may still be using it.  (This distinction is
//! important if we need to preempt the runlist, and then tell the
//! engine to pick up where it left off.)
//!
//! The only supported way to tell whether a runlist entry is done is
//! to put a back-end semaphore release at the end of the entry.
//!
//! OTHER NOTES:
//!
//! By internal convention, this class stores Channels as the
//! outermost ChannelWrapper.  The public interfaces all call
//! pChannel->GetWrapper() to enforce this; the caller need not do so.
//!
//! \sa RunlistChannelWrapper
//!
class Runlist
{
public:
    ~Runlist();

    bool      IsAllocated() const { return !m_hRunlist.empty(); }
    GpuDevice *GetGpuDevice() const { return m_pGpuDevice; }
    LwRm      *GetRmClient() const { return m_pLwRm; }
    UINT32     GetEngine()    const { return m_Engine; }
    void       SetDefaultTimeoutMs(FLOAT64 val)   { m_TimeoutMs = val; }
    FLOAT64    GetDefaultTimeoutMs()        const { return m_TimeoutMs; }
    bool       ContainsChannel(Channel *pChannel) const;

    RC Flush(Channel *pWrappedChannel);
    RC WaitIdle(Channel *pChannel, FLOAT64 TimeoutMs);
    RC WaitIdle(FLOAT64 TimeoutMs);
    RC RemoveOldEntries();

    RC Freeze();
    RC Unfreeze();
    RC WriteFrozenEntries(UINT32 NumEntries);

    void BlockOnFlush(bool Block);
    bool IsBlockedAndFlushed();

    // Struct used to hold mods's idea of a runlist entry.  The actual
    // runlist entry contains a CHID instead of a Channel*, but mods
    // doesn't normally work with CHIDs.
    // Also contains semaphore payload and semaphore index.
    //
    struct Entry
    {
        Channel *pChannel;
        UINT32   GpPutLimit;
        UINT32   Payload;
        UINT32   SemId;
        Entry(Channel *pCh, UINT32 Lim, UINT32 Payload, UINT32 SemId)
            : pChannel(pCh), GpPutLimit(Lim), Payload(Payload), SemId(SemId) {}
        bool operator==(const Entry& Other) const;
    };

    typedef deque<Entry> Entries;

    RC Preempt(Entries* pEntries, Tasker::MutexHolder* pLock);
    RC Resubmit(const Entries& NewEntries);
    RC RecoverFromRobustChannelError(Channel *pWrappedChannel);
    bool RecoveringFromRobustChannelError() const
        { return m_RecoveringFromRobustChannelError; }

private:
    friend class GpuDevice;

    // Surface2D class is multi client compliant and should never use the
    // LwRmPtr constructor without specifying which client
    DISALLOW_LWRMPTR_IN_CLASS(Runlist);

    // These methods (including the constructor) are only called by
    // GpuDevice
    //
    Runlist(GpuDevice *pGpuDevice, LwRm *pLwRm, UINT32 Engine);
    RC Alloc(Memory::Location Location, UINT32 NumEntries);
    void Free();

private:
    UINT32 GetSemSize() const { return m_UseTimeStamp ? 16 : 4; }

    // Struct used by WaitForNumEntries & WaitForNumEntriesPollFunc
    //
    struct WaitForNumEntriesPollArgs
    {
        Runlist *pRunlist;
        UINT32 NumEntries;
        RC rc;
    };

    // Struct used by FlushablePollFunc
    //
    struct FlushablePollArgs
    {
        Runlist *pRunlist;
        RC rc;
    };

    bool IsLoggingEnabled();
    RC WaitForNumEntries(UINT32 NumEntries, FLOAT64 TimeoutMs);
    static bool WaitForNumEntriesPollFunc(void *pPollArgs);
    static bool FlushUnblockedPollFunc(void *pRunlist);
    static bool FlushablePollFunc(void *pFlushablePollArgs);
    RC WriteEntriesToBuffer(Entries::const_iterator Begin, Entries::const_iterator End, UINT32 Index);
    RC ValidateResubmittedEntries(const Entries& NewEntries) const;
    RC ValidateEntryInBuffer(const Entry *pEntry, UINT32 Index);
    RC WriteRlPut(const Entry *pEntry, UINT32 NumEntries, UINT32 NewPut);
    void MakeAllSemFree();
    RC WriteRobustChannelRecoverySemaphore(Channel *pChannel,
                                           const Entry *pEntry);
    UINT32 UnfrozenEntries() const;

    GpuDevice    *m_pGpuDevice; //!< The GpuDevice that the runlist is for
    LwRm         *m_pLwRm;      //!< The RM client that the runlist is for
    UINT32        m_Engine;     //!< The engine that the runlist is for.
                                //!< There cannot be more than one runlist for
                                //!< each gpuDevice/engine pair.
    FLOAT64       m_TimeoutMs;  //!< Default timeout.

    Surface2D     m_Buffer;     //!< Buffer used to hold the runlist entries.
    SurfaceUtils::MappingSurfaceReadWriter m_RunlistReadWriter;
                                //!< Object used to read & write the surface
                                //!< containing runlist entries.
    UINT32        m_MaxEntries; //!< Maximum number of runlist entries.
    Entries       m_Entries;    //!< High-level abstraction of entries
                                //!< in m_Buffer.
    deque<UINT32> m_FreeSem;    //!< Indices of unused semaphores (FIFO).
    Entries       m_SavedEntries;//!< List of entries saved by Preempt() and checked in Resubmit().

    Surface2D     m_Semaphores; //!< Backend semaphores (one per subdev)
                                //!< that tell which entries are done.
    SurfaceUtils::MappingSurfaceReader m_SemaphoreReader; //!< Object used to read semaphore
                                //!< values from m_Semaphores.
    bool          m_UseTimeStamp;//!< Enables use of semaphore time stamp to determine
                                //!< correctness of runlist entry exelwtion order.
    UINT32        m_LwrrentPayload; //!< Payload in m_Semaphores for current
                                    //!< entry.  Supports wraparound.

    vector<LwRm::Handle> m_hRunlist; //!< Handle for the runlist objects,
                                     //!< one per subdev.
    vector<void*> m_pUserD;     //!< Contains the runlist put/get pointers.
                                //!< One per subdev.
    UINT32        m_LwrrentPut; //!< Value last written to m_pUserD->Put

    void         *m_pMutex;     //!< Used to make sure that methods that write
                                //!< the runlist have exclusive control
    void         *m_pFlushMutex;//!< Used to help to preserve atomicity of
                                //!< Flush and Preempt/Resubmit.

    bool          m_Frozen;     //!< True if the runlist is "frozen"
    UINT32  m_NumFrozenEntries; //!< Queue of entries written while
                                //!< runlist is frozen.

    UINT32 m_BlockOnFlushCount; //!< Num. times BlockOnFlush(true) was called.
                                //!< If nonzero, block on any method that locks
                                //!< m_pFlushMutex.

    bool m_RecoveringFromRobustChannelError;
};

#endif // INCLUDED_RUNLIST_H
