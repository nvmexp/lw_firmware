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

#include "pm_lwchn.h"
#include "mdiag/advschd/pmtest.h"
#include "core/include/channel.h"
#include "mdiag/utils/surfutil.h"
#include "mdiag/utils/randstrm.h"
#include "mdiag/utils/surf_types.h"
#include "core/include/cmdline.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/tests/gpu/lwgpu_tsg.h"
#include "vaspace.h"
#include "subctx.h"
#include "sysspec.h"

#define MSGID() __MSGID__(Mdiag, Gpu, ChannelOp)
//--------------------------------------------------------------------
//!\brief Constructor
//!
PmChannel_LwGpu::PmChannel_LwGpu
(
    PolicyManager *pPolicyManager,
    PmTest        *pTest,
    GpuDevice     *pGpuDevice,
    const string  &name,
    LWGpuChannel  *pChannel,
    LWGpuTsg      *pTsg,
    LwRm          *pLwRm
) :
    PmChannel(pPolicyManager, pTest, pGpuDevice, name,
              false, pLwRm),
    m_pChannel(pChannel), m_pTsg(pTsg)
{
    MASSERT(m_pChannel);

    UINT32 gpPut;
    if (m_pChannel->GetGpPut(&gpPut) == OK)
        m_Type = GPFIFO;
    else
        m_Type = PIO;

    SetChannelWrapper(pChannel->GetModsChannel()->GetPmChannelWrapper());
}

//--------------------------------------------------------------------
//!\brief Write a method to the channel
//!
/* virtual */ RC PmChannel_LwGpu::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);

    DebugPrintf(MSGID(), "PmChannel_LwGpu::Write: %s : ch=%d subch=%d; addr=0x%04x, data=0x%08x\n",
                GetTest()->GetName().c_str(), m_pChannel->ChannelNum(), subchannel, method, data);

    return m_pChannel->MethodWriteRC(subchannel, method, data);
}

//--------------------------------------------------------------------
//!\brief Write several conselwtive methods to the channel
//!
//! \param subchannel The subchannel on which to write the methods
//! \param method The first method to write
//! \param count The number of methods to write
//! \param pData Array of values to write to each method.  Must
//!              contain count entries.
//!
/* virtual */ RC PmChannel_LwGpu::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);

    DebugPrintf(MSGID(), "PmChannel_LwGpu::Write: %s: ch=%d subch=%d; addr=0x%04x, data=",
                GetTest()->GetName().c_str(), m_pChannel->ChannelNum(), subchannel, method);
    for (unsigned i = 0; i < count; ++i)
    {
        RawDbgPrintf(MSGID(), "0x%08x ", *(pData+i));
    }
    RawDbgPrintf(MSGID(), "\n");

    return m_pChannel->MethodWriteN_RC(subchannel, method, count, pData);
}

//--------------------------------------------------------------------
//!\brief Set the privilege bit for subsequent GPFIFO entries
//!
//! \param priv The value of the priv bit
//!
/* virtual */ RC PmChannel_LwGpu::SetPrivEnable(bool priv)
{
    return m_pChannel->GetModsChannel()->SetPrivEnable(priv);
}

//--------------------------------------------------------------------
//! \brief Write method(s) to set the context of the semaphore
//!
/* virtual */ RC PmChannel_LwGpu::SetContextDmaSemaphore(LwRm::Handle hCtxDma)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return OK;
}

//--------------------------------------------------------------------
//! \brief Write method(s) to set the offset of a semaphore
//!
/* virtual */ RC PmChannel_LwGpu::SetSemaphoreOffset(UINT64 Offset)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->SetSemaphoreOffsetRC(Offset);
}

//--------------------------------------------------------------------
//! \brief Set semaphore release flags
//!
/* virtual */ void PmChannel_LwGpu::SetSemaphoreReleaseFlags(UINT32 flags)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    m_pChannel->SetSemaphoreReleaseFlags(flags);
}

//--------------------------------------------------------------------
//! \brief Get semaphore release flags
//!
/* virtual */ UINT32 PmChannel_LwGpu::GetSemaphoreReleaseFlags()
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->GetSemaphoreReleaseFlags();
}

//--------------------------------------------------------------------
//! \brief Set semaphore payload size
//!
/* virtual */ RC PmChannel_LwGpu::SetSemaphorePayloadSize
(
    Channel::SemaphorePayloadSize size
)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->SetSemaphorePayloadSize(size);
}

//--------------------------------------------------------------------
//! \brief Get semaphore payload size
//!
/* virtual */ Channel::SemaphorePayloadSize PmChannel_LwGpu::GetSemaphorePayloadSize()
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->GetSemaphorePayloadSize();
}

//--------------------------------------------------------------------
//! \brief Has 64bit semaphore payload support
//!
/* virtual */ bool PmChannel_LwGpu::Has64bitSemaphores()
{
    return m_pChannel->Has64bitSemaphores();
}

//--------------------------------------------------------------------
//! \brief Write method(s) to release a semaphore
//!
/* virtual */ RC PmChannel_LwGpu::SemaphoreRelease(UINT64 Data)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->SemaphoreReleaseRC(Data);
}

//--------------------------------------------------------------------
//! \brief Write method(s) to do a semaphore reduction
//!
/* virtual */ RC PmChannel_LwGpu::SemaphoreReduction
(
    Channel::Reduction Op,
    Channel::ReductionType Type,
    UINT64 Data
)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->SemaphoreReductionRC(Op, Type, Data);
}

//--------------------------------------------------------------------
//! \brief Write method(s) to acquire a semaphore
//!
/* virtual */ RC PmChannel_LwGpu::SemaphoreAcquire(UINT64 Data)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->SemaphoreAcquireRC(Data);
}

//--------------------------------------------------------------------
//! \brief Write method to request a non-stalling int from the channel
//!
/* virtual */ RC PmChannel_LwGpu::NonStallInterrupt()
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->NonStallInterruptRC();
}

//--------------------------------------------------------------------
//!\brief Indicate that we're inserting methods into the pushbuffer
//!
/* virtual */ RC PmChannel_LwGpu::BeginInsertedMethods()
{
    return m_pChannel->BeginInsertedMethodsRC();
}

//--------------------------------------------------------------------
//!\brief Indicate that we're done inserting methods into the pushbuffer
//!
/* virtual */ RC PmChannel_LwGpu::EndInsertedMethods()
{
    return m_pChannel->EndInsertedMethodsRC();
}

//--------------------------------------------------------------------
//! Used to cancel a pending BeginInsertedMethods/EndInsertedMethods
//! in the event of an error
//!
/* virtual */ void PmChannel_LwGpu::CancelInsertedMethods()
{
    return m_pChannel->CancelInsertedMethods();
}

//--------------------------------------------------------------------
//!\brief Flush the channel
//!
/* virtual */ RC PmChannel_LwGpu::Flush()
{
    return m_pChannel->MethodFlushRC();
}

//--------------------------------------------------------------------
//!\brief Wait-for-idle on the channel
//!
/* virtual */ RC PmChannel_LwGpu::WaitForIdle()
{
    return m_pChannel->WaitForIdleRC();
}

//--------------------------------------------------------------------
//!\brief Set the subdevice mask for all subsequent writes
//!
/* virtual */ RC PmChannel_LwGpu::WriteSetSubdevice(UINT32 Subdevice)
{
    MASSERT(m_pChannel->GetInsertNestingLevel() > 0);
    return m_pChannel->WriteSetSubdevice(Subdevice);
}

//--------------------------------------------------------------------
//!\brief Return the underlying mods channel
//!
/* virtual */ Channel *PmChannel_LwGpu::GetModsChannel() const
{
    return m_pChannel->GetModsChannel();
}

//--------------------------------------------------------------------
//!\brief Get the channel handle
//!
/* virtual */ LwRm::Handle PmChannel_LwGpu::GetHandle() const
{
    return m_pChannel->ChannelHandle();
}

//--------------------------------------------------------------------
//!\brief Get the tsg handle
//!
/* virtual */ LwRm::Handle PmChannel_LwGpu::GetTsgHandle() const
{
    return m_pTsg ? m_pTsg->GetHandle() : 0;
}

//--------------------------------------------------------------------
//!\brief Get the channel type
//!
/* virtual */ PmChannel::Type PmChannel_LwGpu::GetType() const
{
    return m_Type;
}

//--------------------------------------------------------------------
//!\brief Get the number of methods expected to be written on the
//! channel
//!
/* virtual */  UINT32 PmChannel_LwGpu::GetMethodCount() const
{
    return m_pChannel->GetMethodCount();
}

//--------------------------------------------------------------------
//! \brief Return true if the channel is enabled
//! \sa SetEnabled()
//!
/* virtual */ bool PmChannel_LwGpu::GetEnabled() const
{
    return m_pChannel->GetEnabled();
}

//--------------------------------------------------------------------
//! \brief Enable or disable the channel
//!
//! When disabled, a channel ignores all writes.  Used when we want to
//! suppress activity on one channel (e.g. in response to a channel
//! error) without aborting the entire test.
//!
//! \sa GetEnabled()
//!
/* virtual */ RC PmChannel_LwGpu::SetEnabled(bool enabled)
{
    m_pChannel->SetEnabled(enabled);
    return OK;
}

//--------------------------------------------------------------------
//! \brief check if ATS is enabled
//!
//! If channel support ATS, return true,
//! Otherwise, return false.
//!
/* virtual */ bool PmChannel_LwGpu::IsAtsEnabled() const
{
    return m_pChannel->GetVASpace()->IsAtsEnabled();
}

/* virtual */ shared_ptr<SubCtx> PmChannel_LwGpu::GetSubCtxInTsg
(
    UINT32 veid
) const
{
    if(!m_pTsg)
    {
        return shared_ptr<SubCtx>();
    }

    TsgGpuChannels tsgChannels = m_pTsg->GetTsgGpuChannels();

    for(TsgGpuChannels::iterator ppTsgChannel = tsgChannels.begin();
        ppTsgChannel != tsgChannels.end(); ++ppTsgChannel)
    {
        shared_ptr<SubCtx> pSubCtx = (*ppTsgChannel)->GetSubCtx();
        if(pSubCtx.get() && pSubCtx->GetVeid() == veid)
        {
            return pSubCtx;
        }
    }

    // Tsg will always have the veid 0 even if it doesn't specify the subctx
    WarnPrintf("%s: No available subct in the tsg %s.\n", __FUNCTION__,
        m_pTsg->GetName().c_str());
    return shared_ptr<SubCtx>();
}

/* virtual */ shared_ptr<SubCtx> PmChannel_LwGpu::GetSubCtx
(
) const
{
    return m_pChannel->GetSubCtx();
}

/* virtual */ RC PmChannel_LwGpu::RealCreateChannel(UINT32 engineId)
{
    MASSERT(!"RealCreateChannel should not be called for PmChannel_LwGpu\n");
    return OK;
}

/* virtual */ PmSmcEngine* PmChannel_LwGpu::GetPmSmcEngine() 
{ 
    return GetTest()->GetSmcEngine(); 
}
