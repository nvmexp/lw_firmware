/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fsp.h"
#include "core/include/fileholder.h"
#include "core/include/tasker.h"
#include "core/include/cpu.h"

void* Fsp::s_Mutex = nullptr;
UINT32 Fsp::s_RefCount = 0;
//------------------------------------------------------------------------------
Fsp::Fsp(TestDevice *pDev)
    : m_pDev(pDev)
{
    MASSERT(pDev);
    AllocateResources();
}

Fsp::~Fsp()
{
    FreeResources();
}
//------------------------------------------------------------------------------------------------
RC Fsp::AllocateResources()
{
    if (Cpu::AtomicCmpXchg32(&s_RefCount, 0, 1))
    {
        s_Mutex = Tasker::AllocMutex("Fsp::s_Mutex", Tasker::mtxUnchecked);
    }
    else
    {
        Cpu::AtomicAdd(&s_RefCount, 1);
    }

    return (RC::OK);
}

//------------------------------------------------------------------------------------------------
void Fsp::FreeResources()
{
    if (Cpu::AtomicCmpXchg32(&s_RefCount, 1, 0))
    {
        Tasker::FreeMutex(s_Mutex);
        s_Mutex = 0;
    }
    else
    {
        Cpu::AtomicAdd(&s_RefCount, -1);
    }
}

//------------------------------------------------------------------------------------------------
// Assumes a single FSP packet is being sent
// useCase: ??
// flag: ??
// additionalData:
RC Fsp::SendFspMsg
(
    Fsp::DataUseCase useCase,
    Fsp::DataFlag flag,
    Fsp::CmdResponse *pResponse
)
{
    vector<UINT08> unusedData;
    return SendFspMsg(useCase, flag, unusedData, pResponse);
}

//------------------------------------------------------------------------------------------------
// Poll on the THERM scratch register until we get success (0xff), which indicated the FSP is
// fully initialized and ready to accept FSP packets.
// Note: This is necessary when running on Emulation because the GPU clocks are very slow compared
// to the CPU clocks. On real silicon we probably don't need this.
RC Fsp::PollFspInitialized()
{
    RC rc;
    UINT32 timeoutMs = s_FspTimeoutMs;
    FLOAT64 pollHz = Tasker::GetMaxHwPollHz();
    Tasker::SetMaxHwPollHz(10);
    RegHal& regs = m_pDev->Regs();
    DEFER
    {
        Tasker::SetMaxHwPollHz(pollHz);
    };

    // FSP runs extreamly slow on emulation.
    if (m_pDev->IsEmulation())
    {
        timeoutMs = 900000; // 600000 was too short on a full_111_FSP netlist
    }
    if (regs.HasRWAccess(MODS_THERM_I2CS_SCRATCH))
    {
        Printf(Tee::PriNormal, "Waiting for FSP to initialize...\n");
        CHECK_RC(Tasker::PollHw(
            [](void *pRegs)
            {
                return static_cast<RegHal*>(pRegs)->Read32(MODS_THERM_I2CS_SCRATCH) == 0xff;
            },
            &m_pDev->Regs(),
            timeoutMs,  // this is a guess, need more testing.
            __FUNCTION__,
            "FSP Initialized poll"));
    }
    else
    {
        Printf(Tee::PriError,
              "PollFspInitialized::I2CS_SCRATCH is priv protected! Need HULK license!!\n");
        rc = RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC Fsp::SendFspMsg
(
    Fsp::DataUseCase useCase,
    Fsp::DataFlag flag,
    const vector<UINT08> &additionalData,
    Fsp::CmdResponse *pResponse
)
{
    Tasker::MutexHolder mh(s_Mutex);

    RC rc;
    RegHal& regs = m_pDev->Regs();
    // Don't start sending cmd packets until the FSP is ready to recieve them.
    CHECK_RC(PollFspInitialized());

    // Construct FSP msg to be copied to EMEM
    vector<UINT08> fspMsg;
    UINT32 fspMsgSizeBytes = 0;
    CHECK_RC(ConstructFspMsg(useCase, flag, additionalData, &fspMsg, &fspMsgSizeBytes));
    MASSERT(fspMsgSizeBytes <= s_FspQueueSize);

    // Poll the FSP queue to be empty
    CHECK_RC(PollFspQueueEmpty());

    // Configure EMEM
    // EMEM offset is broken up into _BLK, _OFFS and lower 2 bits - so combine them all into one
    UINT32 blkHigh = 0;
    regs.LookupField(MODS_PFSP_EMEMC_BLK, &blkHigh, nullptr);
    UINT32 fullOffsetMask = (1 << (blkHigh + 1)) - 1;

    UINT32 lwrrEmemcVal = regs.Read32(MODS_PFSP_EMEMC, s_FspEmemModsChannel) & ~fullOffsetMask;
    lwrrEmemcVal |= s_FspModsQueueStartOffset;
    regs.Write32(MODS_PFSP_EMEMC, s_FspEmemModsChannel, lwrrEmemcVal);

    static constexpr UINT32 s_DwordSize = 4;
    MASSERT((fspMsgSizeBytes % s_DwordSize) == 0);

    regs.Write32(MODS_PFSP_EMEMC_AINCW_TRUE, s_FspEmemModsChannel);
    for (UINT32 idx = 0; idx < fspMsgSizeBytes; idx += s_DwordSize)
    {
        regs.Write32(MODS_PFSP_EMEMD, s_FspEmemModsChannel,
                     *(reinterpret_cast<UINT32*>(&fspMsg[idx])));
    }

    UINT32 ememEndOffset = regs.Read32(MODS_PFSP_EMEMC, s_FspEmemModsChannel) & fullOffsetMask;
    // Update HEAD and TAIL with EMEM offset
    // The write to HEAD needs to happen after TAIL because it will interrupt the FSP
    //
    // Note that TAIL will be set back to HEAD by the FSP when the cmd has been processed
    // There is no cirlwlar buffer
    //
    // Subtracting 1 dword size (values are in bytes)
    // When we use the auto dword inc feature, after sending all the dwords, LW_PSSP_EMEMC_OFFS
    // points to the next free DWORD. While we need to set MODS_PFSP_QUEUE_TAIL to the last
    // dword in the packet.
    regs.Write32(MODS_PFSP_QUEUE_TAIL, s_FspEmemModsChannel, ememEndOffset - s_DwordSize);
    regs.Write32(MODS_PFSP_QUEUE_HEAD, s_FspEmemModsChannel, s_FspModsQueueStartOffset);

    // Poll for FSP response from the EMEM
    rc = PollFspResponseComplete(pResponse);
    CHECK_RC(rc);

    return rc;
}

//-----------------------------------------------------------------------------------
RC Fsp::PollFspQueueEmpty()
{
    return Tasker::PollHw(
        [](void *pRegs)
        {
            UINT32 queueHead = static_cast<RegHal*>(pRegs)->Read32(MODS_PFSP_QUEUE_HEAD,
                                                                 s_FspEmemModsChannel);
            UINT32 queueTail = static_cast<RegHal*>(pRegs)->Read32(MODS_PFSP_QUEUE_TAIL,
                                                                 s_FspEmemModsChannel);
            return queueHead == queueTail;
        },
        &m_pDev->Regs(),
        Tasker::FixTimeout(s_FspTimeoutMs),
        __FUNCTION__,
        "FSP QUEUE Empty Poll");
}

//-----------------------------------------------------------------------------------
// Wait for the msg queue head pointer to be properly updated with the start queue offset.
// On emulation the CPU gets here long before the GPU has updated the registers so this
// is necessary. Otherwise we read back the wrong values when polling for the subseqent 
// response packet complete sequence.
RC Fsp::PollMsgQueueHeadInitialized()
{
    RC rc;
    FLOAT64 pollHz = Tasker::GetMaxHwPollHz();
    Tasker::SetMaxHwPollHz(10);
    DEFER
    {
        Tasker::SetMaxHwPollHz(pollHz);
    };
    CHECK_RC(Tasker::PollHw(
        [](void *pRegs)
        {
            UINT32 queHead = static_cast<RegHal*>(pRegs)->Read32(MODS_PFSP_MSGQ_HEAD,
                                                                 s_FspEmemModsChannel);
            return queHead == s_FspModsQueueStartOffset;
        },
        &m_pDev->Regs(),
        120000,  // this is a guess, need more testing.
        __FUNCTION__,
        "FSP_MSGQ_HEAD poll"));
    return rc;
}

//-----------------------------------------------------------------------------------
RC Fsp::PollFspResponseComplete(Fsp::CmdResponse *pResponse)
{
    RC rc;

    RegHal& regs = m_pDev->Regs();
    CHECK_RC(PollMsgQueueHeadInitialized());

    // Poll until the head & tail don't match. This indicates that the FSP has processed the
    // last cmd packet and is ready to return the response packet.
    CHECK_RC(Tasker::PollHw(
        [](void *pRegs)
        {
            UINT32 head = static_cast<RegHal*>(pRegs)->Read32(MODS_PFSP_MSGQ_HEAD,
                                                                 s_FspEmemModsChannel);
            UINT32 tail = static_cast<RegHal*>(pRegs)->Read32(MODS_PFSP_MSGQ_TAIL,
                                                                 s_FspEmemModsChannel);
            return head != tail;
        },
        &m_pDev->Regs(),
        Tasker::FixTimeout(s_FspTimeoutMs),
        __FUNCTION__,
        "FSP MSGQ Response Poll"));

    UINT32 msgHead = regs.Read32(MODS_PFSP_MSGQ_HEAD, s_FspEmemModsChannel);
    // debug prints, remove after power on is complete.
    constexpr UINT08 channel = s_FspEmemModsChannel;
    Printf(Tee::PriLow, "FSP_QUEUE_HEAD=0x%x\n", regs.Read32(MODS_PFSP_QUEUE_HEAD, channel));
    Printf(Tee::PriLow, "FSP_QUEUE_TAIL=0x%x\n", regs.Read32(MODS_PFSP_QUEUE_TAIL, channel));
    Printf(Tee::PriLow, "FSP_MSGQ_HEAD=0x%x\n", regs.Read32(MODS_PFSP_MSGQ_HEAD, channel));
    Printf(Tee::PriLow, "FSP_MSGQ_TAIL=0x%x\n", regs.Read32(MODS_PFSP_MSGQ_TAIL, channel));

    // Parse the response pkt
    Fsp::CmdResponse localResponse;
    Fsp::CmdResponse *pRes = (pResponse) ? pResponse : &localResponse;

    UINT32 blkHigh;
    regs.LookupField(MODS_PFSP_EMEMC_BLK, &blkHigh, nullptr);
    UINT32 fullOffsetMask = (1 << blkHigh) - 1;

    UINT32 lwrrEmemcVal = regs.Read32(MODS_PFSP_EMEMC, s_FspEmemModsChannel);
    regs.Write32(MODS_PFSP_EMEMC, s_FspEmemModsChannel,
                 (lwrrEmemcVal & ~fullOffsetMask) | msgHead);

    regs.Write32(MODS_PFSP_EMEMC_AINCR_TRUE, s_FspEmemModsChannel);
    for (UINT32 idx = 0; idx < s_FspCmdResponseSizeDwords; idx++)
    {
        pRes->data[idx] = regs.Read32(MODS_PFSP_EMEMD, s_FspEmemModsChannel);
    }

    // Set TAIL == HEAD to notify the FSP the response packet has been received by the client
    regs.Write32(MODS_PFSP_MSGQ_TAIL, s_FspEmemModsChannel, msgHead);

    if (pRes->fields.errorCode != 0)
    {
        Printf(Tee::PriError, "FSP returned non-zero error code 0x%x\n",
                               pRes->fields.errorCode);
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//-------------------------------------------------------------------------------------------------
// Construct the FSP message packet.
// Build a command packet that is byte packed and only consumes the number of bytes required
// by the command. Then round up to the nearest DWORD with zero filled bytes.
// Note: Do not exceed more bytes than is required by the command or the FSP will report an
//       error. For example if the command reqires 10 bytes, the build a packet with only 12 bytes
//       and don't build a packet with 16 bytes.
RC Fsp::ConstructFspMsg
(
    Fsp::DataUseCase useCase,
    Fsp::DataFlag flag,
    const vector<UINT08> &additionalData,
    vector<UINT08> *pFspMsg,
    UINT32 *pFspMsgSizeBytes
) const
{
    MASSERT(pFspMsg);
    MASSERT(pFspMsgSizeBytes);

    // Ref - https://confluence.lwpu.com/display/GFWGH100/FSP+IPC-RPC+Design#FSPIPC-RPCDesign-RPCDesign-LwidiaDataModel(LWDM)

    // Header msg tag
    // [2:0] MSG tag           = 0
    // [3:3] Tag owner         = 1 (source)
    // [5:4] Packet Sequence   = 0
    // [6:6] End of Message    = 1
    // [7:7] Start of Message  = 1
    // MODS use cases only need to send a single packet
    UINT08 msgTag = (1U << 7) | (1U << 6) | (1U << 3);

    // Header
    // We are basically keying off of lwdm message type, so endpoint ids need not be specified.
    Fsp::MsgHeader header = {0};
    header.fields.version   = 0;
    header.fields.msgTags   = msgTag;
    header.fields.msgType   = s_FspHeaderVendorTypePci;
    header.fields.vendorId  = s_FspHeaderVendorIdLw;
    header.fields.lwMsgType = s_FspHeaderMsgTypePrc;

    // Add to the final FSP msg packet
    pFspMsg->insert(pFspMsg->end(), std::begin(header.data), std::end(header.data));
    pFspMsg->push_back(static_cast<UINT08>(useCase));    // modsSubMsgType
    pFspMsg->push_back(static_cast<UINT08>(flag));       // flags
    if (additionalData.size())
    {
        pFspMsg->insert(pFspMsg->end(), std::begin(additionalData), std::end(additionalData));
    }
    // Now make sure the packet is dword aligned.
    while(pFspMsg->size() % 4)
    {
        pFspMsg->push_back(0);
    }

    *pFspMsgSizeBytes = static_cast<UINT32>(pFspMsg->size());

    return RC::OK;
}
