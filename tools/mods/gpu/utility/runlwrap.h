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

// Channel wrapper interface to handle runlists
#ifndef INCLUDED_RUNLWRAP_H
#define INCLUDED_RUNLWRAP_H

#ifndef INCLUDED_CHANWRAP_H
#include "core/include/chanwrap.h"
#endif

class Runlist;

//--------------------------------------------------------------------
//! \brief ChannelWrapper subclass that handles runlist scheduling.
//!
//! This wrapper handles basic-prime scheduling for GpFifo channels
//! that use the fermi+ basic-prime scheduling model.  It mostly
//! intercepts the Flush method, and calls the appropriate Runlist
//! object to do the work.
//!
//! \sa Runlist
//! \sa SemaphoreChannelWrapper
//!
class RunlistChannelWrapper : public ChannelWrapper
{
public:
    RunlistChannelWrapper(Channel *pChannel);
    ~RunlistChannelWrapper();
    static bool IsSupported(Channel *pChannel);
    RC GetRunlist(Runlist **ppRunlist) const;

    // Methods that override base-class Channel methods
    //
    virtual RC Flush();
    virtual RC FinishOpenGpFifoEntry(UINT64* pEnd);
    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size);
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count);
    virtual RC WaitIdle(FLOAT64 TimeoutMs);
    virtual RC RecoverFromRobustChannelError();
    virtual RC UnsetObject(LwRm::Handle hObject);
    virtual RunlistChannelWrapper *GetRunlistChannelWrapper();

private:
    RC DoFlush();
    bool m_RelwrsiveFlush;      // True if DoFlush() is called relwrsively
    Runlist *m_pLastRunlist;    // Most recent runlist the channel was on
};

#endif // INCLUDED_RUNLWRAP_H
