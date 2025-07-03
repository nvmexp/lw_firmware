/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2008, 2011, 2014-2016, 2019, 2021 by LWPU Corporation.  All 
 * rights reserved.  All information contained herein is proprietary and 
 * confidential to LWPU Corporation.  Any use, reproduction, or disclosure 
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "massage.h"
#include "class/cl9097.h" // FERMI_A
#include "mdiag/sysspec.h"
#include "core/include/massert.h"
#include "teegroups.h"

#define MSGID() T3D_MSGID(Massage)

PushBufferMessage::PushBufferMessage()
: m_PushBufferOffset(0),
  m_NewPushBufferOffset(0),
  m_Method(0),
  m_HeaderSize(0),
  m_PayloadSize(0),
  m_WholeSize(0),
  m_Replaced(false)
{
}

PushBufferMessage::~PushBufferMessage()
{
}

void PushBufferMessage::Init(
    UINT32 pushBufferOffset,
    UINT32 newPushBufferOffset,
    SurfaceReader& reader)
{
    m_PushBufferOffset = pushBufferOffset;
    m_NewPushBufferOffset = newPushBufferOffset;
    m_Method = 0;
    m_SubChannel = 0;
    m_HeaderSize = 0;
    m_PayloadSize = 0;
    m_WholeSize = 0;
    m_Replaced = false;
    m_MessageChunk.clear();
    m_OriginalChunk.clear();
    m_Frozen.clear();
    InitImpl(reader);
    m_Modified = false; // InitImpl modifies the message, so we must reset this flag
}

void PushBufferMessage::SetMessageInfo(UINT32 method, UINT32 subch, UINT32 headerSize, UINT32 payloadSize, UINT32 wholeSize)
{
    m_Method = method;
    m_SubChannel = subch;
    m_HeaderSize = headerSize;
    m_PayloadSize = payloadSize;
    m_WholeSize = wholeSize;
    m_Modified = true;
}

UINT32 PushBufferMessage::GetMethodOffset(UINT32 index) const
{
    if (m_Replaced || (index >= m_PayloadSize))
    {
        return 0;
    }
    return GetMethodOffsetImpl(index);
}

UINT32 PushBufferMessage::GetPayloadOffset(UINT32 index) const
{
    if (m_Replaced || (index >= m_PayloadSize))
    {
        return 0;
    }
    return m_PushBufferOffset + sizeof(UINT32)*GetPayloadOffsetImpl(index);
}

bool PushBufferMessage::SetMessageChunk(const vector<UINT32>& buf, bool original)
{
    if ((buf.size() != m_MessageChunk.size()) || (buf != m_MessageChunk))
    {
        m_MessageChunk = buf;
        if (original)
        {
            m_OriginalChunk = buf;
        }
        m_Frozen.clear();
        m_Frozen.resize(buf.size(), false);
        m_Modified = true;
        return true;
    }
    else
    {
        return false;
    }
}

void PushBufferMessage::RestoreOriginal()
{
    SetMessageChunk(m_OriginalChunk, false);
    m_Replaced = false;
}

void PushBufferMessage::FreezeValue(UINT32 index)
{
    MASSERT(index < m_MessageChunk.size());
    m_Frozen[index] = true;
}

bool PushBufferMessage::IsOffsetFrozen(UINT32 ofs)
{
    MASSERT(ofs >= m_PushBufferOffset);
    const UINT32 index = (ofs - m_PushBufferOffset) / sizeof(UINT32);
    MASSERT(index < m_Frozen.size());
    return m_Frozen[index];
}

bool PushBufferMessage::SetValue(UINT32 index, UINT32 value)
{
    MASSERT(index < m_MessageChunk.size());
    if (!m_Frozen[index] && (m_MessageChunk[index] != value))
    {
        m_MessageChunk[index] = value;
        m_Modified = true;
        return true;
    }
    else
    {
        return false;
    }
}

UINT32 PushBufferMessage::GetPayload(UINT32 index) const
{
    if (m_Replaced || (index >= m_PayloadSize))
    {
        return 0;
    }
    return GetPayloadImpl(index);
}

bool PushBufferMessage::SetPayload(UINT32 index, UINT32 data)
{
    if (m_Replaced || (index >= m_PayloadSize))
    {
        return false;
    }
    SetPayloadImpl(index, data);
    return true;
}

void PushBufferMessage::AddSubdevMessage(UINT32 subdevIndex, const vector<UINT32>& buf)
{
    // Skip empty message
    if (buf.empty())
    {
        return;
    }

    vector<UINT32> newChunk = m_MessageChunk;

    // Remove redundant "select all subdevices" method
    if ( ! newChunk.empty() &&
        (newChunk[newChunk.size()-1] == SelectAllSubdevs()))
    {
        newChunk.resize(newChunk.size() - 1);
    }

    // Add subdevice selection
    newChunk.push_back(SelectSubdev(subdevIndex));

    // Add new message
    newChunk.insert(newChunk.end(), buf.begin(), buf.end());

    // Select all subdevices
    newChunk.push_back(SelectAllSubdevs());

    // Finally replace the contents of the message
    ReplaceMessage(newChunk);
}

void PushBufferMessage::SetSubdevMessages(const vector< vector<UINT32> >& messages)
{
    ClearMessage();
    for (UINT32 subdev=0; subdev < messages.size(); subdev++)
    {
        AddSubdevMessage(subdev, messages[subdev]);
    }
}

PushBufferMessage* CreateMassagedMessage()
{
    DebugPrintf(MSGID(), "MassagePushBuffer: Using FermiMessage\n");
    return new FermiMessage();
}
