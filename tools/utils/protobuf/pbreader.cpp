/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pbreader.h"
#include "pbcommon.h"

using namespace std;

//------------------------------------------------------------------------------
// Extract VarInt which is a field header
ProtobufReader::FieldHdr ProtobufReader::GetPBFieldHdr(PBInput& input)
{
    FieldHdr hdr;
    hdr.pos = input.pos;

    const UINT32 value = GetPBData<UINT32>(input);

    hdr.index = static_cast<int>(value >> 3);
    hdr.wire  = static_cast<ProtobufCommon::Wire>(value & 7U);

    return hdr;
}

//------------------------------------------------------------------------------
ProtobufReader::PBInput::PBInput(PBInput& input, size_t hdrPos)
: pParent(&input),
  pMessageData(input.pMessageData),
  pFileData(input.pFileData),
  pos(input.pos),
  endPos(input.pos)
{
    const size_t entrySize = input.GetPBSize(hdrPos);
    if (input.error)
    {
        error = true;
    }
    else
    {
        pos       = input.pos;
        endPos    = pos + entrySize;
        input.pos = endPos;
    }
}

//------------------------------------------------------------------------------
ProtobufReader::PBInput::~PBInput()
{
    if (pParent)
    {
        if (error)
        {
            pParent->error = true;
        }
        pParent->corrupted += corrupted;
        pParent->unknown   += unknown;
    }
}

//------------------------------------------------------------------------------
ProtobufReader::PBInput::Iterator::Iterator(PBInput* pInput)
: m_pInput(pInput)
{
    if (pInput && (pInput->pos < pInput->endPos))
    {
        m_Hdr = GetPBFieldHdr(*pInput);
    }
    else
    {
        m_Hdr = FieldHdr();
    }
}

//------------------------------------------------------------------------------
void ProtobufReader::PBInput::Iterator::operator++()
{
    LWDASSERT(m_pInput != nullptr);
    if (m_pInput->pos < m_pInput->endPos)
    {
        m_Hdr = GetPBFieldHdr(*m_pInput);
    }
}

//------------------------------------------------------------------------------
bool ProtobufReader::PBInput::Iterator::operator!=(const Iterator&) const
{
    LWDASSERT(m_pInput != nullptr);
    return (m_pInput->pos < m_pInput->endPos) && !m_pInput->error;
}

//------------------------------------------------------------------------------
// Extract VarInt which is used as size for Bytes
size_t ProtobufReader::PBInput::GetPBSize(size_t initialPos)
{
    initialPos = initialPos;
    const size_t size = GetPBData<size_t>(*this);
    if (error)
    {
        return 0;
    }
    if (size > endPos - pos)
    {
        LwDiagUtils::Printf(errorPri,
            "%s : Invalid field size %zu exceeds buffer size (pos = %zu)\n",
            __FUNCTION__, size, initialPos);
        ++corrupted;
        error = true;
    }
    return size;
}

//------------------------------------------------------------------------------
void ProtobufReader::PBInput::SetParsingError(size_t errorPos)
{
    LwDiagUtils::Printf(errorPri, "%s : Error parsing data (pos = %zu)\n",
                        __FUNCTION__, errorPos);
    ++pos;
    ++corrupted;
    error = true;
}

//------------------------------------------------------------------------------
// Check wire type of the current entry and report error if it's incorrect
bool ProtobufReader::CheckWire
(
    const char*          functionName,
    const char*          parentName,
    PBInput&             input,
    const FieldHdr&      hdr,
    ProtobufCommon::Wire expected
)
{
    if (hdr.wire == expected)
    {
        return true;
    }

    LwDiagUtils::Printf(LwDiagUtils::PriError,
        "%s : Unrecognized %s message field %d wire %d (expected wire %d) at pos %zu\n",
        functionName,
        parentName,
        hdr.index,
        static_cast<int>(hdr.wire),
        static_cast<int>(expected),
        hdr.pos);

    input.corrupted++;
    input.error = true;
    return false;
}
