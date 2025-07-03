/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009,2015-2016,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "runlwrap.h"
#include "atomwrap.h"
#include "class/cl506f.h"  // LW50_CHANNEL_GPFIFO
#include "class/cl826f.h"  // G82_CHANNEL_GPFIFO
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "runlist.h"
#include "semawrap.h"
#include "core/include/utility.h"

//--------------------------------------------------------------------
//! \brief Constructor
//!
RunlistChannelWrapper::RunlistChannelWrapper
(
    Channel *pChannel
) :
    ChannelWrapper(pChannel),
    m_RelwrsiveFlush(false),
    m_pLastRunlist(NULL)
{
    MASSERT(IsSupported(pChannel));
    if (Platform::HasClientSideResman())
    {
        GpuDevice *pGpuDevice = pChannel->GetGpuDevice();
        pGpuDevice->SetUseRobustChannelCallback(true);
    }
}

//--------------------------------------------------------------------
//! \brief Destructor
//!
RunlistChannelWrapper::~RunlistChannelWrapper()
{
    Runlist *pRunlist = GetGpuDevice()->GetRunlist(this);
    if (pRunlist != NULL)
    {
        RC rc = pRunlist->RemoveOldEntries();
        if (rc != OK)
        {
            Printf(Tee::PriHigh,
                   "Error %d in RunlistChannelWrapper destructor: %s\n",
                   rc.Get(), rc.Message());
        }
        MASSERT(rc == OK);
    }
}

//--------------------------------------------------------------------
//! \brief Return true if the indicated channel supports runlists
//!
//! This static method should be called before wrapping a
//! RunlistChannelWrapper around a channel.  If it returns false,
//! don't use RunlistChannelWrapper; you'll get
//! RC_UNSUPPORTED_FUNCTION for almost every call.
//!
bool RunlistChannelWrapper::IsSupported(Channel *pChannel)
{
    MASSERT(pChannel);
    UINT32 ClassID = pChannel->GetClass();
    UINT32 Put;

    return (pChannel->GetGpPut(&Put) == OK &&
            ClassID != LW50_CHANNEL_GPFIFO &&
            ClassID != G82_CHANNEL_GPFIFO);
}

//--------------------------------------------------------------------
//! \brief Return the runlist that this channel will be added to
//!
//! Note that the runlist determination is made by snooping SetObject
//! and Write methods, so this method may return inaclwrate results if
//! called before any engine methods are written.
//!
//! \param[out] ppRunlist On exit, *ppRunlist points at the runlist.
//!
RC RunlistChannelWrapper::GetRunlist(Runlist **ppRunlist) const
{
    MASSERT(ppRunlist != NULL);
    GpuDevice *pGpuDevice = GetGpuDevice();
    SemaphoreChannelWrapper *pSemWrap =
        GetWrapper()->GetSemaphoreChannelWrapper();
    LwRm *pLwRm = GetWrapper()->GetRmClient();
    RC rc;

    // Use the SemaphoreChannelWrapper, which keeps track of which
    // subchannels/engines are being used, to figure out which runlist
    // to send this channel to.  We search backwards on the
    // subchannels we sent methods to, until we find one for an engine
    // that supports runlists.
    //
    const vector<UINT32> &subchHistory = pSemWrap->GetSubchHistory();
    *ppRunlist = NULL;

    for (vector<UINT32>::const_iterator pSubch = subchHistory.begin();
         pSubch != subchHistory.end() && *ppRunlist == NULL; ++pSubch)
    {
        UINT32 engine = pSemWrap->GetEngine(*pSubch);
        switch (engine)
        {
            case LW2080_ENGINE_TYPE_SW:
                break;
            default:
                CHECK_RC(pGpuDevice->GetRunlist(engine, pLwRm, true,
                                                ppRunlist));
                break;
        }
    }

    // If none of the subchannels are in use, then try sending to the
    // last runlist this channel was on.  Maybe we already freed the
    // objects on those subchannels, in which case the GPU is happier
    // if we keep sending to the same engine.
    //
    if (*ppRunlist == NULL)
    {
        *ppRunlist = m_pLastRunlist;
    }

    // If all else fails, default to the graphics engine.
    //
    if (*ppRunlist == NULL)
    {
        CHECK_RC(pGpuDevice->GetRunlist(LW2080_ENGINE_TYPE_GRAPHICS,
                                        pLwRm, true, ppRunlist));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Wrapper around Channel::Flush that writes a runlist entry
//!
/* virtual */ RC RunlistChannelWrapper::Flush()
{
    UINT32 GpPut;
    UINT32 CachedGpPut;
    UINT32 Put;
    UINT32 CachedPut;
    RC rc;

    CHECK_RC(GetGpPut(&GpPut));
    CHECK_RC(GetCachedGpPut(&CachedGpPut));
    CHECK_RC(GetPut(&Put));
    CHECK_RC(GetCachedPut(&CachedPut));

    if (GpPut != CachedGpPut || Put != CachedPut)
    {
        CHECK_RC(DoFlush());
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Wrapper around Channel::FinishOpenGpFifoEntry
//!
//! In addition to the usual job done by FinishOpenGpFifoEntry, this
//! method checks to see if the channel autoflushed, and adds a
//! runlist entry if so.  See "ABOUT AUTOFLUSH" in
//! RunlistChannelWrapper::DoFlush().
//!
/* virtual */ RC RunlistChannelWrapper::FinishOpenGpFifoEntry(UINT64* pEnd)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//--------------------------------------------------------------------
//! \brief Wrapper around Channel::CallSubroutine
//!
//! In addition to the usual job done by CallSubroutine(), this method
//! checks to see if the channel autoflushed, and adds a runlist entry
//! if so.  See "ABOUT AUTOFLUSH" in RunlistChannelWrapper::DoFlush().
//!
/* virtual */ RC RunlistChannelWrapper::CallSubroutine
(
    UINT64 Offset,
    UINT32 Size
)
{
    RC rc;

    if (GetAutoFlushEnable())
    {
        UINT32 OldCachedGpPut;
        UINT32 NewCachedGpPut;

        CHECK_RC(GetCachedGpPut(&OldCachedGpPut));
        CHECK_RC(m_pWrappedChannel->CallSubroutine(Offset, Size));
        CHECK_RC(GetCachedGpPut(&NewCachedGpPut));
        if (OldCachedGpPut != NewCachedGpPut)
        {
            CHECK_RC(DoFlush());
        }
    }
    else
    {
        CHECK_RC(m_pWrappedChannel->CallSubroutine(Offset, Size));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Wrapper around Channel::InsertSubroutine
//!
//! In addition to the usual job done by InsertSubroutine(), this method
//! checks to see if the channel autoflushed, and adds a runlist entry
//! if so.  See "ABOUT AUTOFLUSH" in RunlistChannelWrapper::DoFlush().
//!
/* virtual */ RC RunlistChannelWrapper::InsertSubroutine
(
    const UINT32 *pBuffer,
    UINT32 count
)
{
    RC rc;

    if (GetAutoFlushEnable())
    {
        UINT32 oldCachedGpPut;
        UINT32 newCachedGpPut;

        CHECK_RC(GetCachedGpPut(&oldCachedGpPut));
        CHECK_RC(m_pWrappedChannel->InsertSubroutine(pBuffer, count));
        CHECK_RC(GetCachedGpPut(&newCachedGpPut));
        if (oldCachedGpPut != newCachedGpPut)
        {
            CHECK_RC(DoFlush());
        }
    }
    else
    {
        CHECK_RC(m_pWrappedChannel->InsertSubroutine(pBuffer, count));
    }

    return rc;
}

//--------------------------------------------------------------------
//! Wrapper around Channel::WaitIdle that also waits for the runlist's
//! backend semaphore to release.
//!
/* virtual */ RC RunlistChannelWrapper::WaitIdle(FLOAT64 TimeoutMs)
{
    GpuDevice *pGpuDevice = GetGpuDevice();
    Runlist *pRunlist;
    RC rc;

    // Remove any backend semaphores that have already been released
    //
    pRunlist = pGpuDevice->GetRunlist(this);
    if (pRunlist)
    {
        CHECK_RC(pRunlist->RemoveOldEntries());
    }

    // Wait for the channel to go idle
    //
    CHECK_RC(m_pWrappedChannel->WaitIdle(TimeoutMs));

    // Wait for the runlist's backend semaphores to release
    //
    pRunlist = pGpuDevice->GetRunlist(this);
    if (pRunlist)
    {
        CHECK_RC(pRunlist->WaitIdle(this, TimeoutMs));
    }

    return rc;
}

//--------------------------------------------------------------------
//! Wrapper around Channel::RecoverFromRobustChannelError that
//! attempts to do error-recovery in the runlist.
//!
/* virtual */ RC RunlistChannelWrapper::RecoverFromRobustChannelError()
{
    RC rc;
    GpuDevice *pGpuDevice = GetGpuDevice();
    Runlist *pRunlist = pGpuDevice->GetRunlist(this);

    if (!pRunlist)
    {
        CHECK_RC(GetRunlist(&pRunlist));
    }

    CHECK_RC(pRunlist->RecoverFromRobustChannelError(m_pWrappedChannel));
    return rc;
}

//--------------------------------------------------------------------
//! Wrapper around Channel::UnsetObject that waits for the runlist to
//! idle.
//!
//! UnsetObject is normally called just before the object on one of
//! the subchannels will be freed.  We wait for the runlist to idle
//! because it contains backend-semaphore releases written by objects
//! in this channel, possibly including the object we're about to
//! free.
//!
/* virtual */ RC RunlistChannelWrapper::UnsetObject(LwRm::Handle hObject)
{
    RC rc;

    CHECK_RC(m_pWrappedChannel->UnsetObject(hObject));
    CHECK_RC(WaitIdle(GetDefaultTimeoutMs()));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Return this object
//!
/* virtual */
RunlistChannelWrapper *RunlistChannelWrapper::GetRunlistChannelWrapper()
{
    return this;
}

//--------------------------------------------------------------------
//! Same as RunlistChannelWrapper::Flush(), except that Flush() won't
//! flush the channel if the Put and GpPut pointers don't need to be
//! updated.  This method will *always* flush, even if the only
//! methods that will get flushed are the backend-semaphore methods
//! inserted by the Runlist object.
//!
//! This method is used by Flush() to do the actual flushing.  It's
//! also called by FinishOpenGpFifoEntry() and CallSubroutine(), in
//! case either of those methods autoflushed due to too many GpFifo
//! entries.  See "ABOUT AUTOFLUSH", below.
//!
//! ABOUT AUTOFLUSH:
//!
//! There are two ways that autoflushing can occur.
//!
//! The primary mechanism is that Write() calls Flush() when the
//! pushbuffer exceeds a threshold.  This is the simple case, since
//! the Flush() will update the runlist as usual.  The only
//! complication is that this can potentially make Flush() relwrsively
//! call itself, since the Runlist class calls Write() to insert a
//! backend semaphore, which is why we have an m_RelwrsiveFlush
//! boolean to prevent an autoflush in the middle of a "real" flush.
//!
//! There's also a secondary autoflush mechanism: if we call
//! FinishOpenGpFifoEntry() and/or CallSubroutine() too many times, we
//! could use up all GpFifo entries.  To avoid that, GpFifoChannel
//! updates GPPUT if autoflushing is enabled and the GPFIFO is getting
//! too full.  But this mechanism does not go through Flush().  So
//! RunlistChannelWrapper overwrites FinishOpenGpFifoEntry() and
//! CallSubroutine() in order to detect whether either method
//! autoflushes, and if so, it does a small after-the-fact flush to
//! create the backend semaphore and the runlist entry.
//!
RC RunlistChannelWrapper::DoFlush()
{
    RC rc;

    // Get the runlist
    //
    Runlist *pRunlist;
    CHECK_RC(GetRunlist(&pRunlist));

    // Abort if we're calling this function relwrsively.
    //
    if (m_RelwrsiveFlush && !pRunlist->RecoveringFromRobustChannelError())
    {
        return OK;
    }
    Utility::TempValue<bool> Tmp(m_RelwrsiveFlush, true);

    // Do the flush.
    //
    CHECK_RC(pRunlist->Flush(m_pWrappedChannel));
    m_pLastRunlist = pRunlist;
    return rc;
}
