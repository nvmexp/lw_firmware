/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Policy manager channel wrapper interface.
#ifndef INCLUDED_PMCHWRAP_H
#define INCLUDED_PMCHWRAP_H

#ifndef INCLUDED_CHANWRAP_H
#include "core/include/chanwrap.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#ifndef INCLUDED_TASKER_H
#include "core/include/tasker.h"
#endif

#ifndef INCLUDED_STL_STACK
#include <stack>
#define INCLUDED_STL_STACK
#endif

//--------------------------------------------------------------------
//! \brief Class that wraps channels used by Policy Manager when
//! OnMethodWrite, OnMethodExelwte, or OnWaitIdle triggers are used
//!
//! This class has several responsibilities:
//! - It breaks up method writes into smaller blocks as required by
//!   the OnMethodWrite and OnMethodExelwte triggers.
//! - It generates method write events.
//! - It generates wait for idle events.
//! - It generates robust-channel events.
//!
class PmChannelWrapper : public ChannelWrapper
{
public:
    PmChannelWrapper(Channel *pChannel);
    ~PmChannelWrapper() { }
    static bool IsSupported(Channel *pChannel);

    void SetNextOnWrite(UINT32 nextOnWrite);
    void SetStalled();
    void ClearStalled();
    RC BlockFlush(bool Blocked);
    PmChannel *GetPmChannel() const { return m_pPmChannel; }
    bool InTest() const;
    RC EndTest();

    //--------------------------------------------------------------------
    //! \brief RAII class for saving/restoring the channel stalled state
    //!
    //! The constructor flags the channel as stalled (no methods will be
    //! processed) while the destructor removes the stalled status.
    //!
    //! The channel can be NULL, in which case this class does nothing.
    //!
    class ChannelStalledHolder
    {
    public:
        ChannelStalledHolder(PmChannelWrapper *pWrap);
        ~ChannelStalledHolder();
    private:
        PmChannelWrapper *m_pWrapper; //!< The PmChannelWrapper instance to
                                      //!< stall/unstall
    };

    // The remaining public methods all override base-class Channel methods

    //! Write a single method into the channel.
    virtual RC Write
    (
        UINT32 Subchannel,
        UINT32 Method,
        UINT32 Data
    );

    //! Write multiple methods to the channel but pass the data as an array
    virtual RC Write
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );

    // Write multiple non-incrementing methods to the channel with data as array
    virtual RC WriteNonInc
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );

    //! Write increment-once method
    virtual RC WriteIncOnce
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count,
        const UINT32 *  pData
    );

    //! Write immediate-data method
    virtual RC WriteImmediate
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Data
    );
    virtual RC BeginInsertedMethods();
    virtual RC EndInsertedMethods();
    virtual void CancelInsertedMethods();
    virtual RC WriteHeader
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count
    );
    virtual RC WriteNonIncHeader
    (
        UINT32          Subchannel,
        UINT32          Method,
        UINT32          Count
    );
    virtual RC WriteNop();

    //! Execute an arbitrary block of commands as a subroutine
    virtual RC CallSubroutine(UINT64 Offset, UINT32 Size);
    virtual RC InsertSubroutine(const UINT32 *pBuffer, UINT32 count);

    //! Set the channel into priv or user mode (default is user)
    virtual RC SetPrivEnable(bool Priv);

    //! Wait for the hardware to finish exelwting all flushed methods.
    virtual RC WaitIdle(FLOAT64 TimeoutMs);

    virtual RC Flush();

    //! Loop the methods and data.
    virtual RC WriteSetSubdevice(UINT32 mask);
    virtual RC SetCachedPut(UINT32 PutPtr);
    virtual RC SetPut(UINT32 PutPtr);

    virtual RC RecoverFromRobustChannelError();

    virtual PmChannelWrapper * GetPmChannelWrapper();

private:
    void SetPmChannel(PmChannel *pPmChannel) { m_pPmChannel = pPmChannel; }
                                           //!< Called by PmChannel constructor
    friend class PmChannel;                //!< So it can call SetPmChannel()

    PmChannel     *m_pPmChannel;           //!< Pointer to the PmChannel
    bool           m_bCSWarningPrinted;    //!< true if the CallSubroutine
                                           //!< warning was printed
    enum WriteType
    {
        INCREMENTING,
        NON_INC,
        INC_ONCE
    };

    // OnWrite notification status variables
    UINT32 m_TotalMethodsWritten;   //!< The total number of methods written
    UINT32 m_NextOnWrite;           //!< The number of methods to write prior
                                    //!< to calling OnWrite
    bool   m_bProcessOnWrite;       //!< true if methods should be tracked for
                                    //!< OnWrite calls
    bool   m_bRelwrsive;            //!< Detect relwrsive calls
    bool   m_bPendingMethodId;   //!< MethodId event is pending from prev write
    UINT32 m_PendingMethodIdClass;  //!< Used if m_bPendingMethodIdEvent = true
    UINT32 m_PendingMethodIdMethod; //!< Used if m_bPendingMethodIdEvent = true
    UINT32 m_NestedInsertedMethods; //!< Number of nested BeginInsertedMethods

    Tasker::ThreadID m_StalledThreadId; //!< The thread Id of the thread that
                                        //!< has stalled the channel.  Channel
                                        //!< operations from the stalled thread
                                        //!< Id are not permitted if they would
                                        //!< cause a flush (or any other method
                                        //!< polling type operation)
    bool m_bBlockFlush;                 //!< when set to true, prevents channel
                                        //!< from flushing
    bool m_bChannelNeedsFlushing;       //!< when unpausing a channel, issue a flush
                                        //!< if a previous flush had been detected - in
                                        //!< case user never flush again

    bool   IsLwrrentThreadStalling();
    RC     ValidateOperation(UINT32 Count);
    RC     PreWrite(WriteType, UINT32 Subch, UINT32 Method,
                    UINT32 Count, UINT32 *pWriteCount);
    RC     OnWaitIdle();

    void   *m_Mutex;                //!< Mutex for accessing the PmChannelWrapper
};

#endif // INCLUDED_PMCHWRAP_H
