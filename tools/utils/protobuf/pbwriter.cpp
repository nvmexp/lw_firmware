/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pbwriter.h"

//------------------------------------------------------------------------------
void ProtobufWriter::DumperBase::Finish(unsigned fieldIndex, ByteStream* pBytes)
{
    LWDASSERT(m_Pos != finished);

    const size_t mid  = pBytes->size();
    const size_t size = mid - m_Pos;

    ProtobufPusher::PushFieldHeader(pBytes, fieldIndex, ProtobufCommon::Wire::Bytes);
    pBytes->PushPB(size);

    // Call std::rotate() to swap two conselwtive ranges of bytes in the
    // ByteStream vector.  We push the data first, then we push the header
    // containing the size (above) and then we just swap them using this
    // function.
    pBytes->Rotate(m_Pos, mid, pBytes->size());

    m_Pos = finished;
}

//------------------------------------------------------------------------------
bool ProtobufWriter::ProtobufPusher::s_enforceFieldVisibility = false;

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushFieldHeader
(
    ByteStream*          pBytes,
    unsigned             fieldIndex,
    ProtobufCommon::Wire wire
)
{
    pBytes->PushPB((fieldIndex << 3) | static_cast<unsigned>(wire));
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushField(unsigned fieldIndex, pb_uint64 value, Output how)
{
    if (value || (how == Output::Force))
    {
        PushFieldHeader(m_pBytes, fieldIndex, ProtobufCommon::Wire::VarInt);
        m_pBytes->PushPB(value);
    }
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushField(unsigned fieldIndex, pb_sint64 value, Output how)
{
    if (value || (how == Output::Force))
    {
        PushFieldHeader(m_pBytes, fieldIndex, ProtobufCommon::Wire::VarInt);
        m_pBytes->PushPBSigned(value);
    }
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushField(unsigned fieldIndex, pb_bool value, Output how)
{
    if (value || (how == Output::Force))
    {
        PushFieldHeader(m_pBytes, fieldIndex, ProtobufCommon::Wire::VarInt);
        m_pBytes->PushPB(static_cast<UINT08>(value));
    }
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushField(unsigned fieldIndex, pb_float value, Output how)
{
    if ((value != 0.0f) || (how == Output::Force))
    {
        PushFieldHeader(m_pBytes, fieldIndex, ProtobufCommon::Wire::Float);
        m_pBytes->PushPB(value);
    }
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushField(unsigned fieldIndex, pb_double value, Output how)
{
    if ((value != 0.0) || (how == Output::Force))
    {
        PushFieldHeader(m_pBytes, fieldIndex, ProtobufCommon::Wire::Double);
        m_pBytes->PushPB(value);
    }
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushField(unsigned fieldIndex, pb_string value, Output how)
{
    if (value.size() || (how == Output::Force))
    {
        PushFieldHeader(m_pBytes, fieldIndex, ProtobufCommon::Wire::Bytes);
        m_pBytes->PushPB(reinterpret_cast<const UINT08*>(value.data()), value.size());
    }
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushField
(
    unsigned               fieldIndex,
    const ProtobufPusher & value,
    Output                 how
)
{
    if (value.size() || (how == Output::Force))
    {
        PushFieldHeader(m_pBytes, fieldIndex, ProtobufCommon::Wire::Bytes);
        m_pBytes->PushPB(value.data(), value.size());
    }
}

namespace
{
    //--------------------------------------------------------------------------
    template<typename T, enable_if_t<!is_signed<T>::value || !is_integral<T>::value, int> = 0>
    void PushNumericValue(ByteStream* pBytes, T value)
    {
        pBytes->PushPB(value);
    }

    //--------------------------------------------------------------------------
    template<typename T, enable_if_t<is_signed<T>::value && is_integral<T>::value, int> = 0>
    void PushNumericValue(ByteStream* pBytes, T value)
    {
        static_assert(!is_same<T, bool>::value, "bool is a signed integer?");
        pBytes->PushPBSigned(value);
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void PushRepeated(ByteStream* pBytes, unsigned fieldIndex, const vector<T>& values)
    {
        if (!values.empty())
        {
            ProtobufWriter::ProtobufPusher::PushFieldHeader(pBytes,
                                                            fieldIndex,
                                                            ProtobufCommon::Wire::Bytes);
            const size_t begin = pBytes->size();
            for (T value : values)
            {
                PushNumericValue(pBytes, value);
            }
            const size_t mid = pBytes->size();
            pBytes->PushPB(mid - begin);
            pBytes->Rotate(begin, mid, pBytes->size());
        }
    }
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned              fieldIndex,
    const vector<UINT16>& values
)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned                 fieldIndex,
    const vector<pb_uint32>& values
)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned                 fieldIndex,
    const vector<pb_sint32>& values
)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned fieldIndex,
    const vector<pb_uint64>& values
)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned fieldIndex,
    const vector<pb_sint64>& values
)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned               fieldIndex,
    const vector<pb_bool>& values
)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned                fieldIndex,
    const vector<pb_float>& values)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}

//------------------------------------------------------------------------------
void ProtobufWriter::ProtobufPusher::PushRepeatedField
(
    unsigned fieldIndex,
    const vector<pb_double>& values
)
{
    PushRepeated(m_pBytes, fieldIndex, values);
}
