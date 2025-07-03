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

#pragma once

#include "lwdiagutils.h"

#include <cstring>
#include <string>
#include <vector>

class ByteStream : private std::vector<UINT08>
{
public:
    ByteStream() = default;

    ByteStream(const ByteStream&) = default;

    ByteStream(ByteStream&& data) : std::vector<UINT08>(std::move(data))
    {
    }

    ByteStream(const std::vector<UINT08>& data) : std::vector<UINT08>(data)
    {
    }

    ByteStream(std::vector<UINT08>&& data) : std::vector<UINT08>(std::move(data))
    {
    }

    ByteStream(std::initializer_list<UINT08> data)
    {
        insert(end(), data.begin(), data.end());
    }

    ~ByteStream() = default;

    ByteStream& operator=(const ByteStream& data)
    {
        std::vector<UINT08>::operator=(data);
        return *this;
    }

    ByteStream& operator=(ByteStream&& data)
    {
        std::vector<UINT08>::operator=(std::move(data));
        return *this;
    }

    ByteStream& operator=(const std::vector<UINT08>& data)
    {
        std::vector<UINT08>::operator=(data);
        return *this;
    }

    ByteStream& operator=(std::vector<UINT08>&& data)
    {
        std::vector<UINT08>::operator=(std::move(data));
        return *this;
    }

    using std::vector<UINT08>::begin;
    using std::vector<UINT08>::clear;
    using std::vector<UINT08>::data;
    using std::vector<UINT08>::empty;
    using std::vector<UINT08>::end;
    using std::vector<UINT08>::operator[];
    using std::vector<UINT08>::reserve;
    using std::vector<UINT08>::resize;
    using std::vector<UINT08>::size;
    using std::vector<UINT08>::swap;

    // Only implement for fundamental types.  We don't want to copy types
    // blindly, so each complex type which needs a Push needs to be implemented
    // explicitly as an overload as below
    template<typename T,
             std::enable_if_t<std::is_fundamental<T>::value, int> = 0>
    void Push(const T & value)
    {
        Push(value, false);
    }

    void Push(const std::string& value)
    {
        Push(reinterpret_cast<const UINT08*>(value.c_str()), value.size() + 1);
    }

    void Push(const char* value)
    {
        Push(reinterpret_cast<const UINT08*>(value), strlen(value) + 1);
    }

    void Push(const ByteStream& value);

    void Push(const UINT08* data, size_t size);

    // PB variants are for emitting protobuf-compatible primitive types

    template<typename T,
             std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value, int> = 0>
    void PushPBSigned(T value)
    {
        PushPBSigned(static_cast<INT64>(value));
    }

    template<typename T,
             std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
    void PushPB(T value)
    {
        PushPB(static_cast<UINT64>(value));
    }

    void PushPBSigned(INT64 value)
    {
        PushPB(static_cast<UINT64>((value << 1) ^ (value >> 63)));
    }

    void PushPB(UINT64 value);

    void PushPB(float value)
    {
        // Assume little endian
        Push(value);
    }

    void PushPB(double value)
    {
        // Assume little endian
        Push(value);
    }

    void PushPB(const UINT08* str, size_t size);

    void PushPB(const char* str)
    {
        PushPB(reinterpret_cast<const UINT08*>(str), strlen(str));
    }

    void PushPB(const std::string& str)
    {
        PushPB(reinterpret_cast<const UINT08*>(str.data()), str.size());
    }

    void PushPB(const ByteStream& bs)
    {
        PushPB(bs.data(), bs.size());
    }

    // Should be called PushAligned(), because it ensures that the value's offset is
    // aligned at the value's size boundary in the vector.
    template<class T>
    void PushPadded(const T & value)
    {
        Push(value, true);
    }

    // Swaps two adjacent ranges of bytes.
    // This is a helper for protobuf printing, which helps avoid creating
    // additional vectors just to callwlate their size.  We need to be able to
    // print variable-length size in front of a sequence of bytes which size
    // isn't know until they are printed.
    void Rotate(size_t first, size_t mid, size_t last);

    void Destroy();

    template<typename T, typename... Args>
    LwDiagUtils::EC Unpack(UINT32* pIndex, T* firstArg, Args*... args) const
    {
        // Unpack first value
        LwDiagUtils::EC ec;
        CHECK_EC(UnpackOne(pIndex, firstArg));

        // Relwrsively unpack remaining values
        return Unpack(pIndex, args...);
    }

    template<typename... Args>
    LwDiagUtils::EC UnpackAll(Args*... args) const
    {
        UINT32 index = 0;
        return Unpack(&index, args...);
    }

    template<typename T, typename... Args>
    LwDiagUtils::EC UnpackPB(size_t* pIndex, T* firstArg, Args*... args) const
    {
        return UnpackPBRange(data(), size(), pIndex, firstArg, args...);
    }

    template<typename... Args>
    LwDiagUtils::EC UnpackAllPB(Args*... args) const
    {
        return UnpackAllPBRange(data(), size(), args...);
    }

    template<typename T, typename... Args>
    static LwDiagUtils::EC UnpackPBRange(const UINT08* addr, size_t size,
                            size_t* pIndex, T* firstArg, Args*... args)
    {
        // Unpack first value
        LwDiagUtils::EC ec;
        CHECK_EC(UnpackOnePB(addr, size, pIndex, firstArg));

        // Relwrsively unpack remaining values
        return UnpackPBRange(addr, size, pIndex, args...);
    }

    template<typename... Args>
    static LwDiagUtils::EC UnpackAllPBRange(const UINT08* addr, size_t size, Args*... args)
    {
        size_t index = 0;
        return UnpackPBRange(addr, size, &index, args...);
    }

    ByteStream operator+(const ByteStream& o) const
    {
        return operator+(static_cast<const std::vector<UINT08>&>(o));
    }

    ByteStream operator+=(const ByteStream& o)
    {
        return operator+=(static_cast<const std::vector<UINT08>&>(o));
    }

    ByteStream operator+(const std::vector<UINT08>& o) const;

    ByteStream& operator+=(const std::vector<UINT08>& o);

private:
    template<class T>
    void Push(const T & value, bool pad)
    {
        constexpr size_t valSize = sizeof(T);
        size_t oldBufSz = this->size();
        if (pad)
        {
            // align up
            oldBufSz += valSize - 1;
            oldBufSz -= oldBufSz % valSize;
        }
        this->resize(oldBufSz + valSize);
        std::memcpy(&(*this)[oldBufSz], &value, valSize);
    }

    // This is not accessible, not implemented.
    // Users must call PushPBSigned() for singed integers.
    template<typename T,
             std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value, int> = 0>
    void PushPB(T value);

    // This is how Unpack terminates.
    // Keeping it private so the user must ilwoke the version with at least one arg.
    LwDiagUtils::EC Unpack(UINT32* /*pIndex*/) const
    {
        return LwDiagUtils::EC::OK;
    }

    // This is how UnpackPB terminates.
    // Keeping it private so the user must ilwoke the version with at least one arg.
    LwDiagUtils::EC UnpackPB(size_t* /*pIndex*/) const
    {
        return LwDiagUtils::EC::OK;
    }

    // This is how UnpackPBRange terminates.
    // Keeping it private so the user must ilwoke the version with at least one arg.
    static LwDiagUtils::EC UnpackPBRange(const UINT08* /*addr*/, size_t /*size*/, size_t* /*pIndex*/)
    {
        return LwDiagUtils::EC::OK;
    }

    LwDiagUtils::EC UnpackOne(UINT32* pIndex, std::string* value) const;

    template<typename T>
    LwDiagUtils::EC UnpackOne(UINT32* pIndex, T* value) const
    {
        const UINT32 index = *pIndex;
        constexpr size_t valSize = sizeof(T);
        if (index > size() || index + valSize > size())
        {
            return LwDiagUtils::CANNOT_GET_ELEMENT;
        }

        memcpy(value, &operator[](index), valSize);
        *pIndex = index + valSize;

        return LwDiagUtils::EC::OK;
    }

    static LwDiagUtils::EC UnpackOnePB(const UINT08* addr, size_t size, size_t* pIndex, std::string* value);

    template<typename T,
             std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value, int> = 0>
    static LwDiagUtils::EC UnpackOnePB(const UINT08* addr, size_t size, size_t* pIndex, T* value)
    {
        INT64 fullValue = *value;
        LwDiagUtils::EC ec;
        CHECK_EC(UnpackOnePBS64(addr, size, pIndex, &fullValue));

        *value = static_cast<T>(fullValue);

        if (*value != fullValue)
        {
            return LwDiagUtils::CANNOT_GET_ELEMENT;
        }

        return LwDiagUtils::OK;
    }

    template<typename T,
             std::enable_if_t<std::is_unsigned<T>::value, int> = 0>
    static LwDiagUtils::EC UnpackOnePB(const UINT08* addr, size_t size, size_t* pIndex, T* value)
    {
        UINT64 fullValue = *value;
        LwDiagUtils::EC ec;
        CHECK_EC(UnpackOnePBU64(addr, size, pIndex, &fullValue));

        *value = static_cast<T>(fullValue);

        if (*value != fullValue)
        {
            return LwDiagUtils::CANNOT_GET_ELEMENT;
        }

        return LwDiagUtils::OK;
    }

    static LwDiagUtils::EC UnpackOnePB(const UINT08* addr, size_t size, size_t* pIndex, float* value);
    static LwDiagUtils::EC UnpackOnePBS64(const UINT08* addr, size_t size, size_t* pIndex, INT64* value);
    static LwDiagUtils::EC UnpackOnePBU64(const UINT08* addr, size_t size, size_t* pIndex, UINT64* value);
};

