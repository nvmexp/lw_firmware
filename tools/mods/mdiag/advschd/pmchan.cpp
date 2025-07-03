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

//! \file
//! \brief Implement a PmChannel wrapper around a Channel or LWGpuChannel
#include "class/cl9097.h" // FERMI_A
#include "class/cl9197.h" // FERMI_B
#include "class/cl9297.h" // FERMI_C
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cl90c0.h" // FERMI_COMPUTE_A
#include "class/cl91c0.h" // FERMI_COMPUTE_B
#include "class/cla0c0.h" // KEPLER_COMPUTE_A
#include "class/cla1c0.h" // KEPLER_COMPUTE_B
#include "class/cla097.h" // KEPLER_A
#include "class/cla197.h" // KEPLER_B
#include "class/cla297.h" // KEPLER_C
#include "class/clb097.h" // MAXWELL_A
#include "class/clb197.h" // MAXWELL_B
#include "class/clb0c0.h" // MAXWELL_COMPUTE_A
#include "class/clb1c0.h" // MAXWELL_COMPUTE_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc0c0.h" // PASCAL_COMPUTE_A
#include "class/clc397.h" // VOLTA_A
#include "class/clc3c0.h" // VOLTA_COMPUTE_A
#include "class/clc365.h" // ACCESS_COUNTER_NOTIFY_BUFFER
#include "pmchan.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "mdiag/sysspec.h"
#include "mdiag/vaspace.h"
#include "pmchwrap.h"
#include "pmevent.h"
#include "pmevntmn.h"
#include "pmsurf.h"
#include "pmtest.h"
#include "pmutils.h"
#include "pmvaspace.h"
#include "gpu/include/gralloc.h"
#include "ctrl/ctrl2080/ctrl2080fifo.h"
#include "mdiag/utils/mdiagsurf.h"
#include "mdiag/utils/utils.h"
#include "mdiag/lwgpu.h"
#include "core/include/chiplibtracecapture.h"

#define MSGID() __MSGID__(Mdiag, Gpu, ChannelOp)

//--------------------------------------------------------------------
//!\brief PmChannel constructor
//!
PmChannel::PmChannel
(
    PolicyManager    *pPolicyManager,
    PmTest           *pTest,
    GpuDevice        *pGpuDevice,
    const string     &name,
    bool             createdByPm,
    LwRm             *pLwRm
) :
    m_pPolicyManager(pPolicyManager),
    m_pTest(pTest),
    m_Name(name),
    m_pGpuDevice(pGpuDevice),
    m_pPmChannelWrapper(nullptr),
    m_pMethodInt(NULL),
    m_pCtxSwInt(NULL),
    m_CreatedByPM(createdByPm),
    m_InstPa(0),
    m_InstAperture(UNINITIALIZED_INST_APERTURE),
    m_pLwRm(pLwRm),
    m_pPmSmcEngine(nullptr)
{
    MASSERT(m_pPolicyManager != NULL);
    MASSERT(m_pTest == NULL || m_pTest->GetPolicyManager() == m_pPolicyManager);
    MASSERT(pGpuDevice != NULL);
    MASSERT(pLwRm);

    // Find an unused channel number
    //
    m_ChannelNumber = 0;

    PmChannels activeChannels = pPolicyManager->GetActiveChannels();
    for (PmChannels::iterator ppCh = activeChannels.begin();
         ppCh != activeChannels.end(); ++ppCh)
    {
        m_ChannelNumber = max(m_ChannelNumber, (*ppCh)->m_ChannelNumber + 1);
    }

    PmChannels inactiveChannels = pPolicyManager->GetInactiveChannels();
    for (PmChannels::iterator ppCh = inactiveChannels.begin();
         ppCh != inactiveChannels.end(); ++ppCh)
    {
        m_ChannelNumber = max(m_ChannelNumber, (*ppCh)->m_ChannelNumber + 1);
    }
}

//--------------------------------------------------------------------
//!\brief PmChannel destructor
//!
PmChannel::~PmChannel()
{
    for (map<string, PmNonStallInt*>::const_iterator iter =
             m_NonStallInts.begin();
         iter != m_NonStallInts.end(); ++iter)
    {
        delete iter->second;
    }

    delete m_pMethodInt;
    delete m_pCtxSwInt;
}

//--------------------------------------------------------------------
//!\brief Set ChannelWrapper for this PmChannel
//! This is being done separtely here (instead of the constructor)
//! since PmChannel_Mods passes nullptr for PmChannelWrapper in its 
//! constructor
void PmChannel::SetChannelWrapper(PmChannelWrapper *pPmChannelWrapper)
{
    MASSERT(pPmChannelWrapper);
    m_pPmChannelWrapper = pPmChannelWrapper;
    
    // Set the channel's PmChannelWrapper to know where this object is.
    //
    m_pPmChannelWrapper->SetPmChannel(this);
}

//--------------------------------------------------------------------
//!\brief Set a name for one of the subchannnels
//!
void PmChannel::FreeSubchannels()
{
    vector<LwRm::Handle>::iterator iter;

    for (iter = m_SubchannelHandles.begin();
         iter != m_SubchannelHandles.end();
         ++iter)
    {
        m_pLwRm->Free(*iter);
    }
}

//--------------------------------------------------------------------
//!\brief Set a name for one of the subchannnels
//!
RC PmChannel::SetSubchannelName(UINT32 subch, string name)
{
    MASSERT(subch < Channel::NumSubchannels);
    m_SubchannelNames[subch].name = name;
    return OK;
}

//--------------------------------------------------------------------
//!\brief Return the name of one of the subchannnels
//!
string PmChannel::GetSubchannelName(UINT32 subch) const
{
    MASSERT(subch < Channel::NumSubchannels);
    return m_SubchannelNames[subch].name;
}

//--------------------------------------------------------------------
//!\brief Set the class number for one of the subchannnels
//!
RC PmChannel::SetSubchannelClass(UINT32 subch, UINT32 classNum)
{
    MASSERT(subch < Channel::NumSubchannels);
    m_SubchannelNames[subch].classNum = classNum;
    return OK;
}

//--------------------------------------------------------------------
//!\brief Return the class number of one of the subchannnels
//!
UINT32 PmChannel::GetSubchannelClass(UINT32 subch) const
{
    MASSERT(subch < Channel::NumSubchannels);
    return m_SubchannelNames[subch].classNum;
}

//--------------------------------------------------------------------
//!\brief Scan subchannels to get CE subchannel number
//!       Return failure if CE subchannel is not found
//!
RC PmChannel::GetCESubchannelNum(UINT32* pCESubchNum)
{
    RC rc;

    if (NULL == pCESubchNum)
    {
        return RC::ILWALID_ARGUMENT;
    }

    DmaCopyAlloc ceClasses;
    ceClasses.SetOldestSupportedClass(GF100_DMA_COPY);

    for (UINT32 subch = 0; subch < Channel::NumSubchannels; ++subch)
    {
        UINT32 classNum = m_SubchannelNames[subch].classNum;
        if ((classNum != 0) &&
            ceClasses.HasClass(classNum))
        {
            *pCESubchNum = subch;
            return rc;
        }
    }

    return RC::SOFTWARE_ERROR;
}

//--------------------------------------------------------------------
//!\brief Scan subchannels to check whether graphic object
//!       has been created
//!
bool PmChannel::HasGraphicObjClass() const
{
    ThreeDAlloc graphicsClasses;
    graphicsClasses.SetOldestSupportedClass(FERMI_A);

    for (UINT32 subch = 0; subch < Channel::NumSubchannels; ++subch)
    {
        UINT32 classNum = m_SubchannelNames[subch].classNum;
        if ((classNum != 0) &&
            graphicsClasses.HasClass(classNum))
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------
//!\brief Scan subchannels to check whether compute object
//!       has been created
//!
bool PmChannel::HasComputeObjClass() const
{
    ComputeAlloc computeClasses;
    computeClasses.SetOldestSupportedClass(FERMI_COMPUTE_A);

    for (UINT32 subch = 0; subch < Channel::NumSubchannels; ++subch)
    {
        UINT32 classNum = m_SubchannelNames[subch].classNum;
        if ((classNum != 0) &&
            computeClasses.HasClass(classNum))
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------
//! \brief This method should be called when the test ends
//!
//! This method calls EndTest on the corresponding PmChannelWrapper,
//! to tell it to stop trying to generate events.
//!
RC PmChannel::EndTest()
{
    MASSERT(m_pPmChannelWrapper != NULL);
    return m_pPmChannelWrapper->EndTest();
}

//--------------------------------------------------------------------
//!\brief Colwenience method to write several conselwtive methods.
//!
//! \param subchannel The subchannel on which to write the methods
//! \param method The first method to write
//! \param count The number of methods to write
//! \param data Variable-length argument list of method data.
//!
RC PmChannel::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    UINT32 data,
    ...
)
{
    static const UINT32 BUFFER_SIZE = 8;
    UINT32 dataBuffer[BUFFER_SIZE];
    vector<UINT32> tempBuf;
    UINT32 *pData;

    if (count <= BUFFER_SIZE)
    {
        pData = dataBuffer;
    }
    else
    {
        tempBuf.resize(count);
        pData = &tempBuf[0];
    }

    va_list argptr;

    va_start(argptr, data);
    pData[0] = data;
    for (UINT32 ii = 1; ii < count; ii++)
    {
        pData[ii] = va_arg(argptr, UINT32);
    }
    va_end(argptr);

    RC rc = Write(subchannel, method, count, pData);

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return the named NonStallInt descriptor for this channel.
//!
//! This method creates the NonStallInt descriptor if it doesn't
//! already exist.
//! \see PmNonStallInt
//!
PmNonStallInt *PmChannel::GetNonStallInt(const string &intName)
{
    PmNonStallInt *pReturlwalue;

    map<string, PmNonStallInt*>::iterator iter = m_NonStallInts.find(intName);
    if (iter == m_NonStallInts.end())
    {
        pReturlwalue = new PmNonStallInt(intName, this);
        m_NonStallInts[intName] = pReturlwalue;
    }
    else
    {
        pReturlwalue = iter->second;
    }
    return pReturlwalue;
}

//--------------------------------------------------------------------
//! \brief Return the MethodInt descriptor for this channel.
//!
//! This method creates the MethodInt descriptor if it doesn't
//! already exist.
//! \see PmMethodInt
//!
PmMethodInt *PmChannel::GetMethodInt()
{
    if (m_pMethodInt == NULL)
    {
        m_pMethodInt = new PmMethodInt(this);
    }

    return m_pMethodInt;
}

//--------------------------------------------------------------------
//! \brief Return the CtxSwInt descriptor for this channel.
//!
//! This method creates the ContextSwitchInt descriptor if it doesn't
//! already exist.
//! \see PmContextSwitchInt
//!
PmContextSwitchInt *PmChannel::GetCtxSwInt()
{
    if (m_pCtxSwInt == NULL)
    {
        m_pCtxSwInt = new PmContextSwitchInt(this,
                             m_pPolicyManager->GetCtxSwIntMode());
    }

    return m_pCtxSwInt;
}

//--------------------------------------------------------------------
//! \brief Get the Instance Memory physical address and physical aperture
//!  for this channel
//!
//! This function call RM API to get the Instance block information with
//! the channel object handle
RC PmChannel::GetInstBlockInfo()
{
    RC rc;
    LwRm::Handle hChannel = this->GetHandle();

    if(m_InstPa && m_InstAperture != UNINITIALIZED_INST_APERTURE)
        return OK;

    // call rm API
    GpuSubdevice * pSubdev = GetGpuDevice()->GetSubdevice(0);
    LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO_PARAMS channelParams = {0};
    channelParams.hChannel = hChannel;
    rc = m_pLwRm->Control(m_pLwRm->GetSubdeviceHandle(pSubdev),
                LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO,
                (void *)&channelParams,
                sizeof(channelParams));

    if(rc != OK)
    {
        ErrPrintf("%s: Can't get the channel 0x%x's instance block physical address.\n",
                  __FUNCTION__, hChannel);
        MASSERT(!"Ilwalidate instance memory physical address.");
        return rc;
    }

    m_InstPa = channelParams.chMemInfo.inst.base;
    m_InstAperture = RmApertureToHwAperture(channelParams.chMemInfo.inst.aperture);

    return rc;
}

UINT32 PmChannel::RmApertureToHwAperture(UINT32 rmAperture)
{
    switch (rmAperture)
    {
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_ILWALID:
            MASSERT(!"PolicyManager::RmApertureToHwAperture:InValid Aperture");
            return UNINITIALIZED_INST_APERTURE;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_VIDMEM:
            return LWC365_NOTIFY_BUF_ENTRY_APERTURE_VID_MEM;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_COH:
            return LWC365_NOTIFY_BUF_ENTRY_APERTURE_SYS_MEM_COHERENT;
        case LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_APERTURE_SYSMEM_NCOH:
            return LWC365_NOTIFY_BUF_ENTRY_APERTURE_SYS_MEM_NONCOHERENT;
        default:
            MASSERT(!"PolicyManager::RmApertureToHwAperture: UnSupported aperture.");
            return UNINITIALIZED_INST_APERTURE;
    }
}

//--------------------------------------------------------------------
//! \brief Return the Instance Memory physical address for this channel
//!
//! This function call RM API to get the Instance pa with the channel
//! object handle
UINT64 PmChannel::GetInstPhysAddr()
{
    RC rc;

    CHECK_RC(GetInstBlockInfo());

    return m_InstPa;
}

//--------------------------------------------------------------------
//! \brief Return the Instance Memory aperture for this channel
//!
//! This function call RM API to get the Instance aperture with the channel
//! object handle
UINT32 PmChannel::GetInstAperture()
{
    RC rc;

    CHECK_RC(GetInstBlockInfo());

    return m_InstAperture;
}

// ###################################################################
// PmChannel::InsertMethods
// ###################################################################

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmChannel::InsertMethods::InsertMethods(PmChannel *pChannel) :
    m_pChannel(pChannel),
    m_InProgress(false)
{
    MASSERT(pChannel != NULL);
}

//--------------------------------------------------------------------
//! \brief Destructor.
//!
//! Calls Channel::CancelInsertedMethods() if there was a Begin()
//! without an End().  That should only happen if a CHECK_RC error
//! oclwrred between the two.
//!
PmChannel::InsertMethods::~InsertMethods()
{
    if (m_InProgress)
    {
        WarnPrintf("Channel::BeginInsertedMethods called without matching EndInsertedMethods. This should only happen if an error oclwrred.\n");
        m_pChannel->CancelInsertedMethods();
    }
}

//--------------------------------------------------------------------
//! Thin wrapper around Channel::BeginInsertedMethods that records
//! whether the function succeeded.
//!
RC PmChannel::InsertMethods::Begin()
{
    MASSERT(!m_InProgress);
    RC rc = m_pChannel->BeginInsertedMethods();
    if (rc == OK)
        m_InProgress = true;
    return rc;
}

//--------------------------------------------------------------------
//! Thin wrapper around Channel::EndInsertedMethods that records
//! whether the function was called.
//!
RC PmChannel::InsertMethods::End()
{
    MASSERT(m_InProgress);
    m_InProgress = false;
    return m_pChannel->EndInsertedMethods();
}

// ###################################################################
// PmChannel_Mods
// ###################################################################

//--------------------------------------------------------------------
//!\brief Constructor
//!
PmChannel_Mods::PmChannel_Mods
(
    PolicyManager *pPolicyManager,
    PmTest        *pTest,
    GpuDevice     *pGpuDevice,
    const string  &name,
    bool          createdByPm,
    PmVaSpace     *pPmVaSpace,
    LwRm          *pLwRm
) :
    PmChannel(pPolicyManager, pTest, pGpuDevice, name,
              createdByPm, pLwRm),
    m_pChannel(nullptr),
    m_pPmVaSpace(pPmVaSpace),
    m_EngineId(LW2080_ENGINE_TYPE_NULL)
{
}

//--------------------------------------------------------------------
//!\brief Set Channel for PmChannel_Mods 
//! This is not done in the constructor since the actual channel is
//! allocated later
void PmChannel_Mods::SetChannel(Channel *pChannel)
{
    MASSERT(pChannel);
    
    m_pChannel = pChannel;

    UINT32 gpPut;
    if (m_pChannel->GetGpPut(&gpPut) == OK)
        m_Type = GPFIFO;
    else
        m_Type = PIO;

    SetChannelWrapper(pChannel->GetPmChannelWrapper());
}

//--------------------------------------------------------------------
//!\brief Write a method to the channel
//!
/* virtual */ RC PmChannel_Mods::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    DebugPrintf(MSGID(), "PmChannel_Mods::Write: ch=%d subch=%d; addr=0x%04x, data=0x%08x\n",
                m_pChannel->GetChannelId(), subchannel, method, data);
    return m_pChannel->Write(subchannel, method, data);
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
/* virtual */ RC PmChannel_Mods::Write
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    DebugPrintf(MSGID(), "PmChannel_Mods::Write: ch=%d subch=%d; addr=0x%04x, data=",
                m_pChannel->GetChannelId(), subchannel, method);
    for (unsigned i = 0; i < count; ++i)
    {
        RawDbgPrintf(MSGID(), "0x%08x ", *(pData+i));
    }
    RawDbgPrintf(MSGID(), "\n");

    return m_pChannel->Write(subchannel, method, count, pData);
}

//--------------------------------------------------------------------
//!\brief Set the privilege bit for subsequent GPFIFO entries
//!
//! \param priv The value of the priv bit
//!
/* virtual */ RC PmChannel_Mods::SetPrivEnable(bool priv)
{
    return m_pChannel->SetPrivEnable(priv);
}

//--------------------------------------------------------------------
//! \brief Write method(s) to set the context of the semaphore
//!
/* virtual */ RC PmChannel_Mods::SetContextDmaSemaphore(LwRm::Handle hCtxDma)
{
    return m_pChannel->SetContextDmaSemaphore(hCtxDma);
}

//--------------------------------------------------------------------
//! \brief Write method(s) to set the offset of a semaphore
//!
/* virtual */ RC PmChannel_Mods::SetSemaphoreOffset(UINT64 Offset)
{
    return m_pChannel->SetSemaphoreOffset(Offset);
}

//--------------------------------------------------------------------
//! \brief Set the semaphore release flags
//!
/* virtual */ void PmChannel_Mods::SetSemaphoreReleaseFlags(UINT32 flags)
{
    m_pChannel->SetSemaphoreReleaseFlags(flags);
}

//--------------------------------------------------------------------
//! \brief Get the semaphore release flags
//!
/* virtual */ UINT32 PmChannel_Mods::GetSemaphoreReleaseFlags()
{
    return m_pChannel->GetSemaphoreReleaseFlags();
}

//--------------------------------------------------------------------
//! \brief Set semaphore payload size
//!
/* virtual */ RC PmChannel_Mods::SetSemaphorePayloadSize
(
    Channel::SemaphorePayloadSize size
)
{
    return m_pChannel->SetSemaphorePayloadSize(size);
}

//--------------------------------------------------------------------
//! \brief Get semaphore payload size
//!
/* virtual */ Channel::SemaphorePayloadSize PmChannel_Mods::GetSemaphorePayloadSize()
{
    return m_pChannel->GetSemaphorePayloadSize();
}

//--------------------------------------------------------------------
//! \brief Has 64bit semaphore payload support
//!
/* virtual */ bool PmChannel_Mods::Has64bitSemaphores()
{
    return m_pChannel->Has64bitSemaphores();
}

//--------------------------------------------------------------------
//! \brief Write method(s) to release a semaphore
//!
/* virtual */ RC PmChannel_Mods::SemaphoreRelease(UINT64 Data)
{
    return m_pChannel->SemaphoreRelease(Data);
}

//--------------------------------------------------------------------
//! \brief Write method(s) to do a semaphore reduction
//!
/* virtual */ RC PmChannel_Mods::SemaphoreReduction
(
    Channel::Reduction Op,
    Channel::ReductionType Type,
    UINT64 Data
)
{
    return m_pChannel->SemaphoreReduction(Op, Type, Data);
}

//--------------------------------------------------------------------
//! \brief Write method(s) to acquire a semaphore
//!
/* virtual */ RC PmChannel_Mods::SemaphoreAcquire(UINT64 Data)
{
    return m_pChannel->SemaphoreAcquire(Data);
}

//--------------------------------------------------------------------
//! \brief Write method to request a non-stalling int from the channel
//!
/* virtual */ RC PmChannel_Mods::NonStallInterrupt()
{
    return m_pChannel->NonStallInterrupt();
}

//--------------------------------------------------------------------
//!\brief Indicate that we're inserting methods into the pushbuffer
//!
/* virtual */ RC PmChannel_Mods::BeginInsertedMethods()
{
    return m_pChannel->BeginInsertedMethods();
}

//--------------------------------------------------------------------
//!\brief Indicate that we're done inserting methods into the pushbuffer
//!
/* virtual */ RC PmChannel_Mods::EndInsertedMethods()
{
    return m_pChannel->EndInsertedMethods();
}

//--------------------------------------------------------------------
//! Used to cancel a pending BeginInsertedMethods/EndInsertedMethods
//! in the event of an error
//!
/* virtual */ void PmChannel_Mods::CancelInsertedMethods()
{
    m_pChannel->CancelInsertedMethods();
}

//--------------------------------------------------------------------
//!\brief Flush the channel
//!
/* virtual */ RC PmChannel_Mods::Flush()
{
    return m_pChannel->Flush();
}

//--------------------------------------------------------------------
//!\brief Flush the channel
//!
/* virtual */ RC PmChannel_Mods::WaitForIdle()
{
    RC rc;

    rc =  m_pChannel->WaitIdle(m_pChannel->GetDefaultTimeoutMs());

    LWGpuResource* lwgpu = PolicyManager::Instance()->GetLWGpuResourceBySubdev(GetGpuDevice()->GetSubdevice(0));
    InfoPrintf("%s channel id %u, engine id %u%s, WFI %s\n",
               __FUNCTION__, m_pChannel->GetChannelId(), m_pChannel->GetEngineId(),
               lwgpu->IsSMCEnabled() ? Utility::StrPrintf(", swizid %u", lwgpu->GetSwizzId(GetLwRmPtr())).c_str() : "",
               rc == OK ? "succeed" : "failed");

    return rc;
}

//--------------------------------------------------------------------
//!\brief Set the subdevice mask for all subsequent writes
//!
/* virtual */ RC PmChannel_Mods::WriteSetSubdevice(UINT32 Subdevice)
{
    return m_pChannel->WriteSetSubdevice(Subdevice);
}

//--------------------------------------------------------------------
//!\brief Return the underlying mods channel
//!
/* virtual */ Channel *PmChannel_Mods::GetModsChannel() const
{
    return m_pChannel;
}

//--------------------------------------------------------------------
//!\brief Get the channel handle
//!
/* virtual */ LwRm::Handle PmChannel_Mods::GetHandle() const
{
    return m_pChannel->GetHandle();
}

//--------------------------------------------------------------------
//!\brief Get the tsg handle
//!
/* virtual */ LwRm::Handle PmChannel_Mods::GetTsgHandle() const
{
    return 0;
}

//--------------------------------------------------------------------
//!\brief Get the channel type
//!
/* virtual */ PmChannel::Type PmChannel_Mods::GetType() const
{
    return m_Type;
}

//--------------------------------------------------------------------
//!\brief Get the number of methods expected to be written on the
//! channel
//!
/* virtual */  UINT32 PmChannel_Mods::GetMethodCount() const
{
    // MODS channels do not lwrrently support this method
    return 0;
}

//--------------------------------------------------------------------
//! \brief Return true if the channel is enabled
//! \sa SetEnabled()
//!
/* virtual */  bool PmChannel_Mods::GetEnabled() const
{
    return true;
}

//--------------------------------------------------------------------
//! \brief Enable or disable the channel
//!
//! When disabled, a channel ignores all writes.  Used when we want to
//! suppress activity on one channel (e.g. in response to a channel
//! error) without aborting the entire test.
//!
//! This feature is not supported for this subclass
//! \sa GetEnabled()
//!
/* virtual */  RC PmChannel_Mods::SetEnabled(bool enabled)
{
    if (!enabled)
        return RC::UNSUPPORTED_FUNCTION;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Get VaSpace
//!
/*virtual*/ VaSpace* PmChannel_Mods::GetVaSpace()
{
    return m_pPmVaSpace->GetVaSpace();
}

//--------------------------------------------------------------------
//! \brief Get VaSpace handle
//!
/*virtual*/ LwRm::Handle PmChannel_Mods::GetVaSpaceHandle() const
{
    return m_pPmVaSpace->GetHandle();
}

//--------------------------------------------------------------------
//! \brief check if ATS is enabled
//!
//! If channel support ATS, return true,
//! Otherwise, return false.
//!
/* virtual */ bool PmChannel_Mods::IsAtsEnabled() const
{
    ErrPrintf("Function not implemented");
    MASSERT(0);
    return false;
}

/* virtual */shared_ptr<SubCtx> PmChannel_Mods::GetSubCtxInTsg
(
    UINT32 veid
) const
{
    return shared_ptr<SubCtx>();
}

/* virtual */ shared_ptr<SubCtx> PmChannel_Mods::GetSubCtx
(
) const
{
    return shared_ptr<SubCtx>();
}

RC PmChannel_Mods::RealCreateChannel(UINT32 engineId)
{
    RC rc;
    PolicyManager *pPolicyManager = GetPolicyManager();
    ChiplibOpScope newScope(string(m_pPmVaSpace->GetTest() ? m_pPmVaSpace->GetTest()->GetTestName() : "") + " Alloc channel " + GetName(), NON_IRQ, ChiplibOpScope::SCOPE_COMMON, NULL);

    m_EngineId = engineId;

    unique_ptr<MdiagSurf> pErrorNotifier = make_unique<MdiagSurf>();
    unique_ptr<MdiagSurf> pPushBuffer = make_unique<MdiagSurf>();
    unique_ptr<MdiagSurf> pGpFifo = make_unique<MdiagSurf>();
    LwRm::Handle channelHandle;
    LwRm::Handle hVaSpace = m_pPmVaSpace->GetHandle();

    rc = MDiagUtils::AllocateChannelBuffers(GetGpuDevice(),
                pErrorNotifier.get(), pPushBuffer.get(), pGpFifo.get(), 
                GetName().c_str(), hVaSpace, GetLwRmPtr());

    if (OK != rc)
    {
        ErrPrintf("Unable to allocate channel buffers for PM created channel %s.\n", GetName().c_str());
        return rc;
    }

    GpuTestConfiguration testConfig;
    UINT32 channelClass;
    CHECK_RC(GetLwRmPtr()->GetFirstSupportedClass(
                testConfig.GetNumFifoClasses(),
                testConfig.GetFifoClasses(),
                &channelClass,
                GetGpuDevice()));

    rc = GetLwRmPtr()->AllocChannelGpFifo(
            &channelHandle,
            channelClass,
            pErrorNotifier->GetMemHandle(),
            pErrorNotifier->GetAddress(),
            pPushBuffer->GetMemHandle(),
            pPushBuffer->GetAddress(),
            pPushBuffer->GetCtxDmaOffsetGpu(),
            (UINT32) pPushBuffer->GetSize(),
            pGpFifo->GetAddress(),
            pGpFifo->GetCtxDmaOffsetGpu(),
            512,
            0, // Handle ContextSwitchObjectHandle
            0, // UINT32 verifFlags
            0, // UINT32 verifFlags2
            0, // UINT32 flags
            GetGpuDevice(),
            0, // Tsg *pTsg
            hVaSpace,
            engineId);  // Handle vaspace

    if (OK != rc)
    {
        ErrPrintf("Unable to allocate PM created GpFifo channel %s.\n", GetName().c_str());
        return rc;
    }

    UINT32 hCtxDma = GetGpuDevice()->GetGartCtxDma(GetLwRmPtr());
    rc = GetLwRmPtr()->BindContextDma(channelHandle, hCtxDma);

    if (OK != rc)
    {
        ErrPrintf("couldn't bind GPU device Gart Ctx Dma: %s\n", rc.Message());
        return rc;
    }

    if (pErrorNotifier->GetCtxDmaHandle())
    {
        rc = GetLwRmPtr()->BindContextDma(channelHandle,
                pErrorNotifier->GetCtxDmaHandle());

        if (OK != rc)
        {
            ErrPrintf("couldn't bind error notifier to channel: %s\n", rc.Message());
            return rc;
        }
    }

    Channel *pChannel = GetLwRmPtr()->GetChannel(channelHandle);
    SetChannel(pChannel);

    pChannel->EnableLogging(pPolicyManager->GetChannelLogging());

    CHECK_RC(pPolicyManager->AddSurface(0, pErrorNotifier.release(),
                false));
    CHECK_RC(pPolicyManager->AddSurface(0, pPushBuffer.release(),
                false));
    CHECK_RC(pPolicyManager->AddSurface(0, pGpFifo.release(), false));

    m_ChannelAllocInfo.AddChannel(this, GetGpuDevice()->GetSubdevice(0), engineId);
    m_ChannelAllocInfo.Print(GetName());

    return OK;
}

