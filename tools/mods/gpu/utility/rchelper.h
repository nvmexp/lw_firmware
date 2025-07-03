/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_RCHELPER_H
#define INCLUDED_RCHELPER_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_TASKER_H
#include "core/include/tasker.h"
#endif

#ifndef INCLUDED_UTILITY_H
#include "core/include/utility.h"
#endif

#ifndef INCLUDED_STL_DEQUE
#include <deque>
#define INCLUDED_STL_DEQUE
#endif

class GpuDevice;
typedef volatile struct LwNotificationRec LwNotification;

//--------------------------------------------------------------------
//! \brief Helper class for robust-channel error handling
//!
//! This class handles most of the work of detecting
class RcHelper
{
public:
    RcHelper(LwRm *pLwRm, GpuDevice *pGpuDevice,
             LwRm::Handle hObject, void *pErrorNotifier,
             Tsg *pTsg);
    ~RcHelper();
    bool DetectNewRobustChannelError() const;
    RC FlushIncomingErrors(FLOAT64 TimeoutMs);
    RC GetLastFlushedError() const { return m_LastFlushedError; }
    RC RobustChannelCallback(UINT32 errorLevel, UINT32 errorType,
                             void *pData, void *pRecoveryCallback);

    //! RAII class used to make sure that only one caller attempts to
    //! do error-recovery.  Similar to MutexHolder, but also prevents
    //! relwrsion.
    //!
    class RecoveryHolder
    {
    public:
        RecoveryHolder() : m_pRcHelper(NULL), m_Acquired(false) {}
        ~RecoveryHolder() {if (m_Acquired) m_pRcHelper->m_InRecovery = false;}
        bool TryAcquire(RcHelper *pRcHelper);
    private:
        Tasker::MutexHolder m_MutexHolder;
        RcHelper *m_pRcHelper;
        bool m_Acquired;
    };

private:
    // Used by CallbackPollFunction
    //
    struct CallbackPollData
    {
        void *pData;
        void *pRecoveryCallback;
        LwU32 hClient;
        LwU32 hDevice;
        LwU32 hObject;
        RC rc;
    };

    // One entry in the error queue for two-stage error recovery
    //
    struct RobustChannelError
    {
        UINT32 errorLevel;
        UINT32 errorType;
        void *pData;
        void *pRecoveryCallback;
    };

    bool UseNotifier() const;
    static bool CallbackPollFunction(void *pCallbackPollData);

    LwRm *m_pLwRm;           //!< The RM client m_hObject was allocated on
    GpuDevice *m_pGpuDevice; //!< The GPU device m_hObject was allocated on
    LwRm::Handle m_hObject;  //!< The channel or TSG getting RC errors
    LwNotification *m_pErrorNotifier; //!< The error notifier, if any
    deque<RobustChannelError> m_RobustChannelErrors; //!< The two-stage queue

    RC m_LastFlushedError;   //!< Last error found by FlushIncomingErrors
    void *m_RecoveryMutex;   //!< Used by RecoveryHolder to prevent collision
    bool m_InRecovery;       //!< Used by RecoveryHolder to prevent relwrsion
    friend class RcHelper::RecoveryHolder;

    Tsg *m_pTsg; //!< NULL for legacy channel
};

#endif // INCLUDED_RCHELPER_H
