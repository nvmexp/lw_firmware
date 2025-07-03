/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2014, 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tsg.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputestc.h"
#include "rchelper.h"
#include "core/include/utility.h"
#include "class/cl2080.h"   // KEPLER_CHANNEL_GROUP_A
#include "class/cla06c.h"   // KEPLER_CHANNEL_GROUP_A
#include "ctrl/ctrla06c.h"  // LWA06C_CTRL_TIMESLICE_PARAMS
#include "lwos.h"           // LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS
#include <algorithm>

/* static */ set<Tsg*> Tsg::s_AllTsgs;

// All TSG classes, used as arg of LwRm::GetFirstSupportedClass
//
static UINT32 s_SupportedTsgClasses[] =
{
    KEPLER_CHANNEL_GROUP_A
};

//--------------------------------------------------------------------
//! \brief Tsg constructor
//!
Tsg::Tsg(LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS *pChannelGroupParams) :
    m_pTestConfig(&m_DefaultTestConfig),
    m_Class(0),
    m_hCtxShare(0),
    m_hTsg(0),
    m_TimeSliceUs(DEFAULT_TIME_SLICE),
    m_hVASpace(0),
    m_EngineId(pChannelGroupParams ? pChannelGroupParams->engineType : LW2080_ENGINE_TYPE_NULL)
{
    s_AllTsgs.insert(this);
}

//--------------------------------------------------------------------
//! \brief Tsg destructor.  Frees the Tsg if Free() wasn't called.
//!
/* virtual */ Tsg::~Tsg()
{
    Free();
    s_AllTsgs.erase(this);
}

//--------------------------------------------------------------------
//! \brief Colwert a TSG handle to the corresponding Tsg object
//!
//! \return A pointer to the Tsg object, or NULL if the handle does
//! not correspond to any known Tsg object.
//!
/* static */ Tsg *Tsg::GetTsgFromHandle
(
    LwRm::Handle hClient,
    LwRm::Handle hTsg
)
{
    for (set<Tsg*>::iterator ppTsg = s_AllTsgs.begin();
         ppTsg != s_AllTsgs.end(); ++ppTsg)
    {
        if ((*ppTsg)->GetHandle() == hTsg &&
            (*ppTsg)->GetRmClient()->GetClientHandle() == hClient)
        {
            return *ppTsg;
        }
    }
    return NULL;
}

//--------------------------------------------------------------------
//! \brief Set the test configuration (optional)
//!
//! The TestConfiguration determines the RmClient and GpuDevice the
//! Tsg is allocated under.
//!
//! If this method is not called, the Tsg uses a default
//! TestConfiguration.  In that case, the caller should use
//! GetTestConfiguration()->SetGpuDevice() to set the GpuDevice.
//!
void Tsg::SetTestConfiguration(GpuTestConfiguration *pTestConfig)
{
    MASSERT(m_hTsg == 0);    // Call before Alloc()
    MASSERT(pTestConfig != NULL);
    m_pTestConfig  = pTestConfig;
}

//--------------------------------------------------------------------
//! \brief Set the TSG class (optional/discouraged)
//!
//! If this method is not used, then LwRm::GetFirstSupportedClass()
//! will be used to determine the TSG class, which is usually better
//! than calling SetClass() explicitly.
//!
void Tsg::SetClass(UINT32 TsgClass)
{
    MASSERT(m_hTsg == 0);    // Call before Alloc()
    m_Class  = TsgClass;
}

//--------------------------------------------------------------------
//! \brief Set the TSG engineId 
//!
void Tsg::SetEngineId(UINT32 engineId)
{
    MASSERT(m_hTsg == 0);    // Call before Alloc()
    MASSERT(engineId < LW2080_ENGINE_TYPE_LAST);
    m_EngineId = engineId;
}

//--------------------------------------------------------------------
//! \brief Allocate the TSG
//!
RC Tsg::Alloc()
{
    MASSERT(m_Channels.empty());
    MASSERT(m_hTsg == 0);
    MASSERT(GetGpuDevice() != NULL);

    LwRm *pLwRm = GetRmClient();
    GpuDevice *pGpuDevice = GetGpuDevice();
    Utility::CleanupOnReturn<Tsg> Cleanup(this, &Tsg::Free);
    RC rc;

    if (pGpuDevice->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID) &&
        (m_EngineId == LW2080_ENGINE_TYPE_NULL))
    {
        const bool bRequireEngineId = GpuPtr()->GetRequireChannelEngineId();
        Printf(bRequireEngineId ? Tee::PriError : Tee::PriWarn,
               "%s : TSG allocations require a valid engine ID!\n", __FUNCTION__);
        if (bRequireEngineId)
            return RC::SOFTWARE_ERROR;
    }

    // Choose a TSG class if SetClass() wasn't called
    //
    if (m_Class == 0)
    {
        CHECK_RC(pLwRm->GetFirstSupportedClass(
                     static_cast<UINT32>(NUMELEMS(s_SupportedTsgClasses)),
                     s_SupportedTsgClasses,
                     &m_Class, pGpuDevice));
    }

    // Allocate the TSG object
    //
    CHECK_RC(AllocErrorNotifier(&m_ErrorNotifier));

    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS AllocParams = {0};
    AllocParams.hObjectError = m_ErrorNotifier.GetCtxDmaHandleGpu();
    AllocParams.hVASpace = m_hCtxShare ? 0 : m_hVASpace;
    AllocParams.engineType = m_EngineId;
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(pGpuDevice),
                          &m_hTsg, m_Class, &AllocParams));

    m_pRcHelper.reset(new RcHelper(pLwRm, pGpuDevice, m_hTsg,
                                   m_ErrorNotifier.GetAddress(),
                                   this));
    Cleanup.Cancel();
    return rc;
}

//--------------------------------------------------------------------
//! \brief Free the TSG allocated by Alloc()
//!
/* virtual */ void Tsg::Free()
{
    MASSERT(m_Channels.empty());

    m_pRcHelper.reset();
    if (m_hTsg)
    {
        GetRmClient()->Free(m_hTsg);
        m_hTsg = 0;
    }
    m_ErrorNotifier.Free();
    m_Channels.clear();
}

//--------------------------------------------------------------------
//! \brief Get a channel allocated under this Tsg, by channel handle
//!
Channel *Tsg::GetChannelFromHandle(LwRm::Handle hCh) const
{
    for (UINT32 i = 0; i < m_Channels.size(); ++i)
    {
        if (m_Channels[i]->GetHandle() == hCh)
            return m_Channels[i];
    }

    return nullptr;
}

//--------------------------------------------------------------------
//! \brief Get a channel allocated under this Tsg, by index
//!
//! \param Idx An integer between 0 and GetNumChannels() - 1.
//! Channels are indexed in the order they were created.
//!
Channel *Tsg::GetChannel(UINT32 Idx) const
{
    MASSERT(Idx < m_Channels.size());
    return m_Channels[Idx]->GetWrapper();
}

//--------------------------------------------------------------------
//! \brief Called by the RM when a robust-channel error oclwrs
//!
//! \sa Channel::RobustChannelCallback
//!
RC Tsg::RobustChannelCallback
(
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    MASSERT(m_hTsg != 0);
    return m_pRcHelper->RobustChannelCallback(errorLevel, errorType,
                                              pData, pRecoveryCallback);
}

//--------------------------------------------------------------------
//! \brief set tsg timeslice value
//!
//! \param timeslice in microsecond
//! RM is responsible to colwert it into closest hw format
RC Tsg::SetTsgTimeSlice(UINT64 timesliceUs)
{
    RC rc;

    LwRm *pLwRm = GetRmClient();
    LWA06C_CTRL_TIMESLICE_PARAMS params = {0};
    params.timesliceUs = timesliceUs;
    CHECK_RC(pLwRm->Control(GetHandle(),
                            LWA06C_CTRL_CMD_SET_TIMESLICE,
                            &params,
                            sizeof(params)));

    m_TimeSliceUs = timesliceUs;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Add a channel to the Tsg.
//!
//! This method should only be called by the Channel constructor.
//!
//! Implementation note: As a rule, most Channel pointers point to the
//! outermost ChannelWrapper around a channel.  But AddChannel() is
//! called before the channel wrappers are created, so the
//! m_Channels[] is an exception to that rule.  As a result, almost
//! all Tsg methods that use m_Channels[] must call GetWrapper()
//! before returning the Channel to the caller.
//!
void Tsg::AddChannel(Channel *pCh)
{
    MASSERT(m_hTsg != 0);
    MASSERT(pCh->GetGpuDevice() == GetGpuDevice());
    MASSERT(pCh->GetRmClient() == GetRmClient());
    for (UINT32 ii = 0; ii < m_Channels.size(); ++ii)
    {
        MASSERT(m_Channels[ii]->GetWrapper() != pCh->GetWrapper());
    }

    m_Channels.push_back(pCh);
}

//--------------------------------------------------------------------
//! \brief Remove a channel from the Tsg.
//!
//! This method should only be called by the Channel destructor.
//!
void Tsg::RemoveChannel(Channel *pCh)
{
    MASSERT(m_hTsg != 0);

    UINT32 NewSize = 0;
    for (UINT32 ii = 0; ii < m_Channels.size(); ++ii)
    {
        if (m_Channels[ii]->GetWrapper() != pCh->GetWrapper())
        {
            m_Channels[NewSize] = m_Channels[ii];
            ++NewSize;
        }
    }

    MASSERT(NewSize == m_Channels.size() - 1);
    m_Channels.resize(NewSize);
}

//--------------------------------------------------------------------
//! \brief Process incoming robust-channel errors
//!
//! Check for robust-channel errors on the TSG.  If any have oclwrred,
//! run the recovery code on each channel and set the channels' error
//! code to the RC error.
//!
//! Called by Channel::CheckError() when the channel is on a TSG.
//!
void Tsg::UpdateError()
{
    MASSERT(m_hTsg != 0);
    RcHelper::RecoveryHolder recoveryHolder;

    if (( m_pRcHelper->DetectNewRobustChannelError() ||
          GetGpuDevice()->GetResetInProgress()) &&
        recoveryHolder.TryAcquire(m_pRcHelper.get()))
    {
        StickyRC firstRc;

        if (m_pRcHelper->DetectNewRobustChannelError())
        {
            for (UINT32 ii = 0; ii < GetNumChannels(); ++ii)
            {
                firstRc = GetChannel(ii)->RecoverFromRobustChannelError();
            }
        }

        if (GetGpuDevice()->GetResetInProgress())
        {
            firstRc = RC::RESET_IN_PROGRESS;
        }

        if (firstRc != OK)
        {
            for (UINT32 ii = 0; ii < GetNumChannels(); ++ii)
            {
                GetChannel(ii)->SetError(firstRc);
            }
        }
    }
}

//--------------------------------------------------------------------
//! \brief Allocate the error notifier
//!
//! \param pErrorNotifier The error-notifier surface
//!
/* virtual */ RC Tsg::AllocErrorNotifier(Surface2D *pErrorNotifier)
{
    return m_pTestConfig->AllocateErrorNotifier(pErrorNotifier, 0);
}

//--------------------------------------------------------------------
//! \brief Allocate the subcontext based on flag
//!
//!
/* virtual */ RC Tsg::AllocSubcontext(Subcontext **ppSubctx, LwRm::Handle hVASpace, LwU32 flags)
{
    Subcontext *pSubctx = new Subcontext(this, hVASpace);
    if (pSubctx == NULL)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    RC rc = pSubctx->Alloc(flags);
    if (rc == OK)
    {
        *ppSubctx = pSubctx;
    }
    else
    {
        delete pSubctx;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Allocate the subcontext based on flag
//!
//!
/* virtual */ void Tsg::FreeSubcontext(Subcontext *pSubctx)
{
    pSubctx->Free();
    delete pSubctx;
}

#ifdef LW_VERIF_FEATURES
//--------------------------------------------------------------------
//! \brief Allocate the subcontext with a specific veid
//!
//!
/* virtual */ RC Tsg::AllocSubcontextSpecified(Subcontext **ppSubctx, LwRm::Handle hVASpace, LwU32 id)
{
    Subcontext *pSubctx = new Subcontext(this, hVASpace);
    if (pSubctx == NULL)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    RC rc = pSubctx->AllocSpecified(id);
    if (rc == OK)
    {
        *ppSubctx = pSubctx;
    }
    else
    {
        delete pSubctx;
    }

    return rc;
}
#endif

