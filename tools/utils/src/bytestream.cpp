/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "inc/bytestream.h"
#include "lwdiagutils.h"
#include <algorithm>

void ByteStream::Push(const ByteStream& value)
{
    insert(end(), value.begin(), value.end());
}

void ByteStream::Push(const UINT08* data, size_t size)
{
    insert(end(), data, data + size);
}

void ByteStream::PushPB(UINT64 value)
{
    UINT08  tmp[10];
    UINT08* tmpEnd = tmp;
    do
    {
        const UINT08 byte = static_cast<UINT08>(value);
        value >>= 7;
        LWDASSERT(tmpEnd <= &tmp[sizeof(tmp) - 1]);
        *(tmpEnd++) = byte | (value ? 0x80U : 0U);
    } while (value);

    insert(end(), tmp, tmpEnd);
}

void ByteStream::PushPB(const UINT08* str, size_t size)
{
    PushPB(size);
    Push(str, size);
}

void ByteStream::Rotate(size_t first, size_t mid, size_t last)
{
    std::rotate(begin() + first, begin() + mid, begin() + last);
}

void ByteStream::Destroy()
{
    ByteStream().swap(*this);
}

ByteStream ByteStream::operator+(const std::vector<UINT08>& o) const
{
    ByteStream byteStream;
    byteStream += *this;
    byteStream += o;
    return byteStream;
}

ByteStream& ByteStream::operator+=(const std::vector<UINT08>& o)
{
    reserve(size() + o.size());
    insert(end(), o.begin(), o.end());
    return *this;
}

LwDiagUtils::EC ByteStream::UnpackOne(UINT32* pIndex, std::string* value) const
{
    const UINT32 index = *pIndex;
    if (index >= size())
    {
        return LwDiagUtils::CANNOT_GET_ELEMENT;
    }

    // Find NUL character which terminates the string
    const UINT08* const ptrToNul = static_cast<const UINT08*>(
                memchr(&(*this)[index], 0, size() - index));
    if (!ptrToNul)
    {
        return LwDiagUtils::CANNOT_GET_ELEMENT;
    }
    const UINT32 nulPos = static_cast<UINT32>(ptrToNul - &(*this)[0]);

    value->assign(reinterpret_cast<const char*>(&(*this)[index]), nulPos - index);
    *pIndex = nulPos + 1;

    return LwDiagUtils::OK;
}

LwDiagUtils::EC ByteStream::UnpackOnePB(const UINT08* addr, size_t size, size_t* pIndex, std::string* value)
{
    size_t len = 0;
    LwDiagUtils::EC ec;
    CHECK_EC(UnpackOnePB(addr, size, pIndex, &len));

    const size_t index = *pIndex;
    if (index + len > size)
    {
        return LwDiagUtils::CANNOT_GET_ELEMENT;
    }

    value->assign(reinterpret_cast<const char*>(addr + index), len);

    *pIndex = index + len;

    return LwDiagUtils::OK;
}

LwDiagUtils::EC ByteStream::UnpackOnePB(const UINT08* addr, size_t size, size_t* pIndex, float* value)
{
    const size_t index = *pIndex;
    if (index + sizeof(float) > size)
    {
        return LwDiagUtils::CANNOT_GET_ELEMENT;
    }

    memcpy(value, addr + index, sizeof(float));
    *pIndex = index + sizeof(float);
    return LwDiagUtils::OK;
}

LwDiagUtils::EC ByteStream::UnpackOnePBS64(const UINT08* addr, size_t size, size_t* pIndex, INT64* value)
{
    UINT64 uValue = 0;
    LwDiagUtils::EC ec;
    CHECK_EC(UnpackOnePBU64(addr, size, pIndex, &uValue));

    *value = (static_cast<INT64>(uValue << 63) >> 63) ^ static_cast<INT64>(uValue >> 1);

    return LwDiagUtils::OK;
}

LwDiagUtils::EC ByteStream::UnpackOnePBU64(const UINT08* addr, size_t size, size_t* pIndex, UINT64* value)
{
    size_t index = *pIndex;
    UINT64 v     = 0;
    int    shift = 0;
    UINT08 next;

    do
    {
        if (index >= size || shift >= 64)
        {
            return LwDiagUtils::CANNOT_GET_ELEMENT;
        }

        next = addr[index++];

        if (shift > 64 - 7)
        {
            LWDASSERT(shift < 64);
            const UINT08 sign = next >> (64 - shift);
            if ((sign != 0) || (next & 0x80U))
            {
                return LwDiagUtils::CANNOT_GET_ELEMENT;
            }
        }

        v += static_cast<UINT64>(next & 0x7FU) << shift;

        shift += 7;
    } while (next & 0x80U);

    *pIndex = index;
    *value  = v;

    return LwDiagUtils::OK;
}
