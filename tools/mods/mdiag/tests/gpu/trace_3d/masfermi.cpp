/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "massage.h"
#include "lwmisc.h"
#include "core/include/gpu.h"
#include "fermi/gf100/dev_ram.h"
#include "mdiag/sysspec.h"
#include "core/include/channel.h"
#include "teegroups.h"

#define MSGID() T3D_MSGID(Massage)

FermiMessage::FermiMessage()
: m_ImmediateData(false),
  m_Increment(noIncrement)
{
}

FermiMessage::~FermiMessage()
{
}

void FermiMessage::InitImpl(SurfaceReader& reader)
{
    m_ImmediateData = false;
    m_Increment = noIncrement;

    // Get header
    vector<UINT32> chunk = reader.Read(1);
    if (chunk.empty())
    {
        return;
    }
    const UINT32 header = chunk[0];
    const UINT32 secondaryOp = REF_VAL(LW_FIFO_DMA_SEC_OP, header);
    const UINT32 tertiaryOp = REF_VAL(LW_FIFO_DMA_TERT_OP, header);
    const UINT32 subchannel = REF_VAL(LW_FIFO_DMA_METHOD_SUBCHANNEL, header);

    UINT32 method = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS, header) << 2;
    UINT32 readPayloadSize = REF_VAL(LW_FIFO_DMA_METHOD_COUNT, header);

    switch(secondaryOp)
    {
        case LW_FIFO_DMA_SEC_OP_INC_METHOD:
            m_Increment = incrementAlways;
            break;
        case LW_FIFO_DMA_SEC_OP_IMMD_DATA_METHOD:
            m_ImmediateData = true;
            readPayloadSize = 0;
            break;
        case LW_FIFO_DMA_SEC_OP_NON_INC_METHOD:
            break;
        case LW_FIFO_DMA_SEC_OP_ONE_INC:
            m_Increment = incrementOnce;
            break;
        case LW_FIFO_DMA_SEC_OP_GRP0_USE_TERT:
            switch(tertiaryOp)
            {
                case LW_FIFO_DMA_TERT_OP_GRP0_INC_METHOD:
                    method = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS_OLD, header) << 2;
                    readPayloadSize = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);
                    m_Increment = incrementAlways;
                    break;
                case LW_FIFO_DMA_TERT_OP_GRP0_SET_SUB_DEV_MASK:
                    // !!! Regular method offset is always multiple of 4
                    // so we use 1 for simplicity -- just in case someone really want to
                    // massage this method.
                    method = 1;
                    readPayloadSize = 0;
                    m_ImmediateData = true;
                    break;
                case LW_FIFO_DMA_TERT_OP_GRP0_STORE_SUB_DEV_MASK:
                case LW_FIFO_DMA_TERT_OP_GRP0_USE_SUB_DEV_MASK:
                default:
                    MASSERT(!"MODS cannot handle USE/STORE SUB_DEV_MASK method!\n");
                    break;
            }
            break;
        case LW_FIFO_DMA_SEC_OP_GRP2_USE_TERT:
            method = REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS_OLD, header) << 2;
            readPayloadSize = REF_VAL(LW_FIFO_DMA_METHOD_COUNT_OLD, header);
            break;
        case LW_FIFO_DMA_SEC_OP_END_PB_SEGMENT:
            readPayloadSize = 0;
            // method offset will be 0 for END_PB_SEGMENT!
            break;
        default:
            MASSERT(!"Unknown method format!\n");
    }

    const vector<UINT32> payload = reader.Read(readPayloadSize);
    chunk.insert(chunk.end(), payload.begin(), payload.end());

    if (m_ImmediateData)
        DebugPrintf(MSGID(), "MPB: Hdr @%04x=%08x, mthd %04x count 0000 (immediate data)\n",
            GetPushBufferOffset(), header, method);
    else
        DebugPrintf(MSGID(), "MPB: Hdr @%04x=%08x, mthd %04x count %04x\n",
            GetPushBufferOffset(), header, method, readPayloadSize);

    // Setup base class
    SetMessageInfo(method, subchannel, 1, m_ImmediateData?1:(chunk.size()-1), chunk.size());
    SetMessageChunk(chunk, true);
}

UINT32 FermiMessage::GetMethodOffsetImpl(UINT32 index) const
{
    if (m_Increment == incrementAlways)
    {
        return GetMethod() + index*4;
    }
    else if ((m_Increment == incrementOnce) && (index > 0))
    {
        return GetMethod() + 4;
    }
    return GetMethod();
}

UINT32 FermiMessage::GetPayloadOffsetImpl(UINT32 index) const
{
    if (m_ImmediateData)
    {
        return 0;
    }
    else
    {
        return 1 + index;
    }
}

UINT32 FermiMessage::GetPayloadImpl(UINT32 index) const
{
    if (m_ImmediateData)
    {
        return REF_VAL(LW_FIFO_DMA_IMMD_DATA, GetValue(0));
    }
    else
    {
        return GetValue(1+index);
    }
}

void FermiMessage::ColwertMethodToNonIncr()
{
    MASSERT(m_ImmediateData);

    m_ImmediateData = false;
    m_Increment = noIncrement;

    // Get the chunk Copy - not the reference for updat in place, because calling
    // PushBufferMessage::SetMessageChunk() and PushBufferMessage::ReplaceMessage(chunk)
    // would be expected.
    vector<UINT32> chunk = GetMessageChunk();
    // Get the Reference to the header
    UINT32& header = chunk[0];

    DebugPrintf(MSGID(), "Hdr @%04x=%08x, mthd %04x is about to be resized.\n",
        GetPushBufferOffset(), header,
        GetMethod()
        );

    // Change PushBuffer type to NON_INC
    header = FLD_SET_REF_NUM(LW_FIFO_DMA_SEC_OP, LW_FIFO_DMA_SEC_OP_NON_INC_METHOD, header);
    // Set count field
    header = FLD_SET_REF_NUM(LW_FIFO_DMA_METHOD_COUNT, 1, header);
    DebugPrintf(MSGID(), "New Hdr @%04x=%08x, mthd %04x. \n",
        GetPushBufferOffset(), header,
        GetMethod()
        );

    // Reserve a seat for method data.
    chunk.push_back(0);

    // Commit all changes
    SetMessageInfo(
        REF_VAL(LW_FIFO_DMA_METHOD_ADDRESS, header) << 2,
        REF_VAL(LW_FIFO_DMA_METHOD_SUBCHANNEL, header),
        1, chunk.size() - 1, chunk.size());

    ReplaceMessage(chunk);
}

void FermiMessage::SetPayloadImpl(UINT32 index, UINT32 data)
{
    if (m_ImmediateData
        //&& data > REF_VAL(LW_FIFO_DMA_IMMD_DATA, ~0x0U)//0x1fff
        && data > DRF_MASK(LW_FIFO_DMA_IMMD_DATA))
    {
        // Input data > max Immediate Data, we colwert the PB.
        // Fixs the bug:
        //     "1693219 ARB insert_class_method not overriding all bits in the method"
        ColwertMethodToNonIncr();
    }

    if (m_ImmediateData)
    {
        if (SetValue(0, FLD_SET_DRF_NUM(_FIFO, _DMA, _IMMD_DATA, data, GetValue(0))))
        {
            DebugPrintf(MSGID(), "Data@%04x (method %x) changed to %08x\n",
                GetPushBufferOffset(),
                GetMethod(),
                data);
        }
    }
    else
    {
        if (SetValue(1+index, data))
        {
            DebugPrintf(MSGID(), "Data@%04x (method %x) changed to %08x\n",
                GetPushBufferOffset() + sizeof(UINT32)*(1+index),
                GetMethodOffset(index),
                data);
        }
    }
}

UINT32 FermiMessage::SelectSubdev(UINT32 subdevIndex) const
{
    return
        REF_DEF(LW_FIFO_DMA_SET_SUBDEVICE_MASK_OPCODE, _VALUE) |
        REF_NUM(LW_FIFO_DMA_SET_SUBDEVICE_MASK_VALUE,  1 << subdevIndex);
}

UINT32 FermiMessage::SelectAllSubdevs() const
{
    return
        REF_DEF(LW_FIFO_DMA_SET_SUBDEVICE_MASK_OPCODE, _VALUE) |
        REF_NUM(LW_FIFO_DMA_SET_SUBDEVICE_MASK_VALUE, Channel::AllSubdevicesMask);
}

