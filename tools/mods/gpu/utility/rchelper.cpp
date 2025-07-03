/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2014,2016, 2018, 2020 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "rchelper.h"
#include "core/include/display.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "rmcd.h"  // RC_RESET_CALLBACK
#if defined(INCLUDE_MDIAG)
#include "mdiag/advschd/policymn.h"
#include "mdiag/utl/utl.h"
#endif
#include "tsg.h"
//--------------------------------------------------------------------
//! \brief RcHelper constructor
//!
//! \param pLwRm The RM client that hObject was allocated on.
//! \param pLwRm The GPU device that hObject was allocated on.
//! \param hObject The channel or TSG for which we want to detect
//!                robust-channel errors.
//! \param pErrorNotifier The error notifier associated with hObject.
//!
RcHelper::RcHelper
(
    LwRm *pLwRm,
    GpuDevice *pGpuDevice,
    LwRm::Handle hObject,
    void *pErrorNotifier,
    Tsg *pTsg
) :
    m_pLwRm(pLwRm),
    m_pGpuDevice(pGpuDevice),
    m_hObject(hObject),
    m_pErrorNotifier((LwNotification*)pErrorNotifier),
    m_RecoveryMutex(Tasker::AllocMutex("RcHelper::m_RecoveryMutex",
                                       Tasker::mtxUnchecked)),
    m_InRecovery(false),
    m_pTsg(pTsg)
{
    MASSERT(m_pLwRm);
    MASSERT(m_pGpuDevice);
    MASSERT(m_hObject);

    if (m_pErrorNotifier)
    {
        MEM_WR16(&m_pErrorNotifier->status, 0);
    }
}

//--------------------------------------------------------------------
//! \brief RcHelper destructor
//!
RcHelper::~RcHelper()
{
    Tasker::FreeMutex(m_RecoveryMutex);
}

//--------------------------------------------------------------------
//! \brief Detect a new robust-channel error
//!
//! This is used to implement Channel::DetectNewRobustChannelError,
//! which detects a new robust-channel error without doing any
//! error-recovery.
//!
//! \param IncludeOldError If true, then in addition to checking for
//!     new robust-channel errors, also check for a pre-existing error
//!     that was already processed but not yet cleared.
//! \param OldError The channel's internal m_Error field, which
//!     contains the previous uncleared RC error the channel got.
//!     Ignored if IncludeOldError is false.
//! \return true if an robust-channel error oclwrred
//!
bool RcHelper::DetectNewRobustChannelError() const
{
    if (UseNotifier())
        return MEM_RD16(&m_pErrorNotifier->status) == 0xffff;
    else
        return !m_RobustChannelErrors.empty();
}

//--------------------------------------------------------------------
//! \brief Flush the queue of incoming robust-channel errors
//!
//! If we're using error notifiers, this function reads the error from
//! the notifier and clears the notifier.  If we're using two-stage
//! error recovery, this function calls the RM callback for each error
//! the RM has reported, and clears the error queue.
//!
//! In either case, the caller can use GetLastFlushedError() to read
//! the error that was flushed by this method.
//!
//! \param TimeoutMs Timeout for polling the RM during two-stage
//!     recovery
//!
RC RcHelper::FlushIncomingErrors(FLOAT64 TimeoutMs)
{
    RC rc;

    if (UseNotifier())
    {
        if (MEM_RD16(&m_pErrorNotifier->status) == 0xffff)
        {
            UINT32 rcStatus = MEM_RD32(&m_pErrorNotifier->info32);
            m_LastFlushedError.Clear();
            m_LastFlushedError = Channel::RobustChannelsCodeToModsRC(rcStatus);
            MEM_WR16(&m_pErrorNotifier->status, 0);
            MASSERT(m_LastFlushedError != OK);
        }
    }
    else
    {
        CallbackPollData PollData;

        PollData.hClient = m_pLwRm->GetClientHandle();
        PollData.hDevice = m_pLwRm->GetDeviceHandle(m_pGpuDevice);

        if (m_pTsg)
        {
            // [Bug 1578329]: RM callback can't recognize tsg handle
            // Send the 1st channel handle in tsg and RM will reset
            // all channels anyway
            Channel *channel = m_pTsg->GetChannel(0);
            MASSERT(channel);

            PollData.hObject = channel->GetHandle();
        }
        else
        {
            PollData.hObject = m_hObject;
        }

        while (!m_RobustChannelErrors.empty())
        {
            RobustChannelError error = m_RobustChannelErrors.front();
            PollData.pData = error.pData;
            PollData.pRecoveryCallback = error.pRecoveryCallback;
            PollData.rc.Clear();
            CHECK_RC(POLLWRAP_HW(&CallbackPollFunction, &PollData, TimeoutMs));
            CHECK_RC(PollData.rc);

            m_RobustChannelErrors.pop_front();
            m_LastFlushedError.Clear();
            m_LastFlushedError =
                Channel::RobustChannelsCodeToModsRC(error.errorType);
            MASSERT(m_LastFlushedError != OK);
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Push an entry on the two-stage error queue
//!
//! This method is called by ModsDrvGpuRcCallback(), which the RM
//! calls when a robust-channel error oclwrs.  This method merely
//! pushes the error on a queue, and calls the attached
//! pRecoveryCallback function later to tell the RM to do the full
//! recovery.
//!
//! This method is only used during two-stage error recovery, a
//! technique used by Windows to report RC errors during an ISR, and
//! recover later at the client's colwenience.  Other OS'es use an
//! error notifier, in which case this method isn't used.
//!
RC RcHelper::RobustChannelCallback
(
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    StickyRC rc;

#if defined(INCLUDE_MDIAG)
    rc = PolicyManager::Instance()->HandleChannelReset(m_hObject, m_pLwRm);
    if (Utl::HasInstance())
    {
        Utl::Instance()->TriggerChannelResetEvent(m_hObject, m_pLwRm);
    }
#endif

    RobustChannelError error = {0};
    error.errorLevel = errorLevel;
    error.errorType = errorType;
    error.pData = pData;
    error.pRecoveryCallback = pRecoveryCallback;

    m_RobustChannelErrors.push_back(error);
    return rc;
}

//--------------------------------------------------------------------
//! Lock the RcHelper to make sure that no other caller attempts to do
//! error-recovery, including relwrsively.  Return true on a
//! successful lock.
//!
bool RcHelper::RecoveryHolder::TryAcquire(RcHelper *pRcHelper)
{
    MASSERT(pRcHelper != NULL);
    MASSERT(m_pRcHelper == NULL);

    m_pRcHelper = pRcHelper;
    m_Acquired = (m_MutexHolder.TryAcquire(m_pRcHelper->m_RecoveryMutex) &&
                  !m_pRcHelper->m_InRecovery);
    if (m_Acquired)
        m_pRcHelper->m_InRecovery = true;
    return m_Acquired;
}

//--------------------------------------------------------------------
//! Used by FlushIncomingErrors() to poll on a recovery callback until
//! the RM can process it.
//!
/* static */ bool RcHelper::CallbackPollFunction(void *pCallbackPollData)
{
    CallbackPollData *pPollData = (CallbackPollData*)pCallbackPollData;
    RC_RESET_CALLBACK *pRecoveryCallback =
        (RC_RESET_CALLBACK*)pPollData->pRecoveryCallback;

    LwU32 status = pRecoveryCallback(pPollData->hClient, pPollData->hDevice,
#if RM_RC_CALLBACK_HANDLE_UPDATE
                                     pPollData->hObject,
#endif
                                     pPollData->hObject, pPollData->pData,
                                     LW_FALSE);
    pPollData->rc = RmApiStatusToModsRC(status);

    // Stop polling as soon as we get an error code other than
    // LWRM_IN_USE.  LWRM_IN_USE means that the RM is blocked on
    // another RM function.
    //
    return (pPollData->rc != RC::LWRM_IN_USE);
}

//--------------------------------------------------------------------
//! Check whether we're using an error notifier or a callback (aka
//! two-stage error recovery) to get robust-channel errors from the
//! RM.
//!
bool RcHelper::UseNotifier() const
{
    if (m_pGpuDevice->GetUseRobustChannelCallback())
    {
        return false;
    }
    else
    {
        MASSERT(m_pErrorNotifier);
        return true;
    }
}
