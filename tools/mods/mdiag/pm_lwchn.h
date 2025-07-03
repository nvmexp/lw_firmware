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

//! \file  pm_lwchn.h
//! \brief Defines a PmChannel subclass that wraps around a LWGpuChannel

#ifndef INCLUDED_PM_LWCHN_H
#define INCLUDED_PM_LWCHN_H

#ifndef INCLUDED_PMCHAN_H
#include "mdiag/advschd/pmchan.h"
#endif

#ifndef INCLUDED_LWGPU_CHANNEL_H
#include "mdiag/utils/lwgpu_channel.h"
#endif

#ifndef INCLUDED_STL_STACK
#define INCLUDED_STL_STACK
#include <stack>
#endif

class LWGpuChannel;
class LWGpuTsg;

//!---------------------------------------------------------------
//! \brief PmChannel subclass that wraps around a mdiag LWGpuChannel
//!
class PmChannel_LwGpu : public PmChannel
{
public:
    PmChannel_LwGpu(PolicyManager *pPolicyManager, PmTest *pTest,
                    GpuDevice *pGpuDevice, const string &name,
                    LWGpuChannel *pChannel, LWGpuTsg* pTsg, LwRm* pLwRm);
    ~PmChannel_LwGpu() { }
    virtual RC Write(UINT32 subchannel, UINT32 method, UINT32 data);
    virtual RC Write(UINT32 subchannel, UINT32 method, UINT32 count,
                     const UINT32 *pData);
    virtual RC SetPrivEnable(bool priv);
    virtual RC SetContextDmaSemaphore(LwRm::Handle hCtxDma);
    virtual RC SetSemaphoreOffset(UINT64 Offset);
    virtual void SetSemaphoreReleaseFlags(UINT32 flags);
    virtual UINT32 GetSemaphoreReleaseFlags();
    virtual RC SetSemaphorePayloadSize(Channel::SemaphorePayloadSize size);
    virtual Channel::SemaphorePayloadSize GetSemaphorePayloadSize();
    virtual bool Has64bitSemaphores();
    virtual RC SemaphoreRelease(UINT64 Data);
    virtual RC SemaphoreReduction(Channel::Reduction Op,
                                  Channel::ReductionType Type, UINT64 Data);
    virtual RC SemaphoreAcquire(UINT64 Data);
    virtual RC NonStallInterrupt();
    virtual RC BeginInsertedMethods();
    virtual RC EndInsertedMethods();
    virtual void CancelInsertedMethods();
    virtual RC Flush();
    virtual RC WaitForIdle();
    virtual RC WriteSetSubdevice(UINT32 Subdevice);
    virtual Channel *GetModsChannel() const;
    virtual LwRm::Handle GetHandle() const;
    virtual LwRm::Handle GetTsgHandle() const;
    virtual Type         GetType()   const;
    virtual UINT32       GetMethodCount() const;
    virtual bool         GetEnabled() const;
    virtual RC           SetEnabled(bool enabled);
    virtual bool         IsAtsEnabled() const;
    virtual VaSpace* GetVaSpace() { return m_pChannel->GetVASpace().get(); }
    virtual LwRm::Handle GetVaSpaceHandle() const { return m_pChannel->GetVASpaceHandle(); }
    virtual shared_ptr<SubCtx> GetSubCtxInTsg(UINT32 veid) const;
    virtual shared_ptr<SubCtx> GetSubCtx() const;
    virtual LWGpuTsg *   GetTsg() const { return m_pTsg; }
    virtual UINT32 GetChannelInitMethodCount() const { return m_pChannel->GetChannelInitMethodCount(); }
    virtual RC RealCreateChannel(UINT32 engineId);
    virtual UINT32       GetEngineId() const { return m_pChannel->GetEngineId(); }
    virtual PmSmcEngine* GetPmSmcEngine();

private:
    LWGpuChannel  *m_pChannel;
    LWGpuTsg      *m_pTsg;
    Type           m_Type;
};

#endif // INCLUDED_PM_LWCHN_H
