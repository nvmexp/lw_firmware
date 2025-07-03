/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2014-2016, 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_TSG_H
#define INCLUDED_TSG_H

#ifndef INCLUDED_GPUTESTC_H
#include "gpu/tests/gputestc.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_SURF2D_H
#include "surf2d.h"
#endif

#ifndef INCLUDED_STL_MEMORY
#include <memory>
#define INCLUDED_STL_MEMORY
#endif

#include "subcontext.h"

class Channel;
class RcHelper;

//----------------------------------------------------------------------
//! \brief TSG (channel group) class
//!
//! This class manages a TSG (Time Slice Group) class such as
//! KEPLER_CHANNEL_GROUP_A.
//!
//! Channels can be allocated as children of a TSG, instead of being
//! children of the GpuDevice.  Channels under the same TSG share the
//! same context and round-robin runlist entry, so switching between
//! channels in a TSG is a lightweight GPU operation.
//!
//! All channels in a TSG share the same error notifier.  SW cannot
//! tell which channel is responsible for any robust-channel errors.
//!
//! Channels are allocated under a TSG by passing a Tsg arg to
//! LwRm::AllocateChannelGpFifo().
//!
class Tsg
{
public:
    Tsg(LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS *pChannelGroupParams = NULL);
    virtual ~Tsg();
    static Tsg *GetTsgFromHandle(LwRm::Handle hClient, LwRm::Handle hTsg);

    void SetTestConfiguration(GpuTestConfiguration *pTestConfig);
    GpuTestConfiguration *GetTestConfiguration() const { return m_pTestConfig;}
    GpuDevice *GetGpuDevice() const { return m_pTestConfig->GetGpuDevice(); }
    LwRm *GetRmClient() const  { return m_pTestConfig->GetRmClient(); }

    RC AllocSubcontext(Subcontext **ppSubctx, LwRm::Handle hVASpace, LwU32 flags);
    void FreeSubcontext(Subcontext *pSubctx);
#ifdef LW_VERIF_FEATURES
    RC AllocSubcontextSpecified(Subcontext **ppSubctx, LwRm::Handle hVASpace, LwU32 id);
#endif

    void SetClass(UINT32 TsgClass);
    UINT32 GetClass() const { return m_Class; }

    LwRm::Handle GetVASpaceHandle() const { return m_hVASpace; }
    void SetVASpaceHandle(LwRm::Handle hVASpace) { m_hVASpace = hVASpace; }

    RC Alloc();
    virtual void Free();

    LwRm::Handle GetHandle() const { return m_hTsg; }

    typedef vector<Channel *> Channels;

    UINT32 GetNumChannels() const { return (UINT32)m_Channels.size(); }
    Channel *GetChannel(UINT32 Idx) const;
    Channel *GetChannelFromHandle(LwRm::Handle hCh) const;

    RC RobustChannelCallback(UINT32 errorLevel, UINT32 errorType,
                             void *pData, void *pRecoveryCallback);

    enum
    {
        DEFAULT_TIME_SLICE = 0 // 0 is invalid for tsg timeslice
                               // it's safe to use it as guard value
    };
    RC SetTsgTimeSlice(UINT64 timesliceUs);
    UINT64 GetTimeSliceUs() const { return m_TimeSliceUs; }
    void SetEngineId(UINT32 engineId);
    UINT32 GetEngineId() { return m_EngineId; }

public:
    // These methods should only be called by the Channel classes
    //
    void AddChannel(Channel *pCh);
    void RemoveChannel(Channel *pCh);
    void UpdateError();
    RcHelper *GetRcHelper() const { return m_pRcHelper.get(); }
    const Channels & GetChannels() const { return m_Channels; }

protected:
    virtual RC AllocErrorNotifier(Surface2D *pErrorNotifier);

private:
    RC RecoverFromRobustChannelError();

    // Optional members set before Alloc()
    //
    GpuTestConfiguration *m_pTestConfig;  //!< Set by SetTestConfiguration
    GpuTestConfiguration m_DefaultTestConfig; //!< Default if we don't call
                                              //!< SetTestConfiguration
    UINT32 m_Class;               //!< TSG class, e.g. KEPLER_CHANNEL_GROUP_A
    LwRm::Handle m_hCtxShare;     //!< Optional CtxShare passed to TSG alloc

    // Members set during Alloc()
    //
    Surface2D m_ErrorNotifier;    //!< Error notifier
    LwRm::Handle m_hTsg;          //!< TSG handle

    // Members set after Alloc()
    //
    Channels m_Channels; //!< All channels allocated under this TSG.

    // Misc
    //
    UINT64 m_TimeSliceUs;        //!< tsg timeslice value
    unique_ptr<RcHelper> m_pRcHelper; //!< Used to process robust-channel errs
    static set<Tsg*> s_AllTsgs;  //!< All Tsg objects, used by GetTsgFromHandle
    LwRm::Handle m_hVASpace;
    UINT32 m_EngineId;
};

#endif // INCLUDED_TSG_H
