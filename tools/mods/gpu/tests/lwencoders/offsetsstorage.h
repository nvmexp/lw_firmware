/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef OFFSETSSTORAGE_H
#define OFFSETSSTORAGE_H

#include <memory.h>

#include <vector>

#include "core/include/platform.h"
#include "core/include/types.h"
#include "core/include/utility.h"

template <unsigned int NSingle, unsigned int NMultiple>
class OffsetsStorage
{
private:
    typedef vector<UINT32> OfsCont;

public:
    typedef OfsCont::const_iterator const_iterator;

    template <unsigned int N>
    void Set(UINT32 offset)
    {
        MASSERT(NUMELEMS(single) > N);
        single[N] = offset;
    }

    template <unsigned int N>
    UINT32 Get() const
    {
        MASSERT(NUMELEMS(single) > N);
        return single[N];
    }

    template <unsigned int N>
    void Add(UINT32 offset)
    {
        MASSERT(NUMELEMS(multiple) > N);
        multiple[N].push_back(offset);
    }

    template <unsigned int N>
    void Clear()
    {
        MASSERT(NUMELEMS(multiple) > N);
        return multiple[N].clear();
    }

    template <unsigned int N>
    size_t Size() const
    {
        MASSERT(NUMELEMS(multiple) > N);
        return multiple[N].size();
    }

    template <unsigned int N>
    const_iterator Begin() const
    {
        MASSERT(NUMELEMS(multiple) > N);
        return multiple[N].begin();
    }

    template <unsigned int N>
    const_iterator End() const
    {
        MASSERT(NUMELEMS(multiple) > N);
        return multiple[N].end();
    }

private:
    UINT32  single[NSingle];
    OfsCont multiple[NMultiple];
};

template <unsigned int NSingle, unsigned int NMultiple>
class OffsetsNilStorage
{
public:
    struct const_iterator
    {
        void operator++() {}
        unsigned int operator*() { return 0; }
    };

    OffsetsNilStorage()
    {
        memset(&multiple[0], 0, sizeof(multiple));
    }

    template <unsigned int N>
    void Set(UINT32 offset) {}

    template <unsigned int N>
    UINT32 Get() const { return 0; }

    template <unsigned int N>
    void Add(UINT32 offset)
    {
        MASSERT(NUMELEMS(multiple) > N);
        ++multiple[N];
    }

    template <unsigned int N>
    void Clear()
    {
        MASSERT(NUMELEMS(multiple) > N);
        return multiple[N] = 0;
    }

    template <unsigned int N>
    size_t Size() const
    {
        MASSERT(NUMELEMS(multiple) > N);
        return multiple[N];
    }

    template <unsigned int N>
    const_iterator Begin() const
    {
        return const_iterator();
    }

    template <unsigned int N>
    const_iterator End() const
    {
        return const_iterator();
    }

private:
    size_t multiple[NMultiple];
};

class OffsetTracker
{
public:
    OffsetTracker()
      : m_lwrOffset(0)
    {}

    UINT32 Align(UINT32 alignment)
    {
        m_lwrOffset = Utility::AlignUp(m_lwrOffset, alignment);
        return m_lwrOffset;
    }

    UINT32 Reserve(UINT32 howManyBytes, UINT32 alignment)
    {
        UINT32 alignedStart = Align(alignment);
        m_lwrOffset += howManyBytes;

        return alignedStart;
    }

    void Reset()
    {
        m_lwrOffset = 0;
    }

    UINT32 GetOffset() const
    {
        return m_lwrOffset;
    }

private:
    UINT32 m_lwrOffset;
};

class MemoryWriter
{
public:
    MemoryWriter()
      : m_startPtr(nullptr)
    {}

    void SetStartAddress(UINT08 *startPtr)
    {
        m_startPtr = startPtr;
        m_offsetTracker.Reset();
    }

    UINT32 GetOffset() const
    {
        return m_offsetTracker.GetOffset();
    }

    UINT32 Reserve(UINT32 howManyBytes, UINT32 alignment)
    {
        return m_offsetTracker.Reserve(howManyBytes, alignment);
    }

    UINT32 Write(void const *what, UINT32 howManyBytes, UINT32 alignment)
    {
        UINT32 alignedStart = m_offsetTracker.Reserve(howManyBytes, alignment);
        Platform::VirtualWr(m_startPtr + alignedStart, what, howManyBytes);

        return alignedStart;
    }

private:
    OffsetTracker m_offsetTracker;
    UINT08       *m_startPtr;
};

class SizeCounter
{
public:
    void SetStartAddress(UINT08 *)
    {
        m_offsetTracker.Reset();
    }

    UINT32 GetOffset() const
    {
        return m_offsetTracker.GetOffset();
    }

    UINT32 Reserve(UINT32 howManyBytes, UINT32 alignment)
    {
        return m_offsetTracker.Reserve(howManyBytes, alignment);
    }

    UINT32 Write(void const *what, UINT32 howManyBytes, UINT32 alignment)
    {
        UINT32 alignedStart = m_offsetTracker.Reserve(howManyBytes, alignment);

        return alignedStart;
    }

private:
    OffsetTracker m_offsetTracker;
};

class NilEncryptor
{
public:
    static const size_t blockSize  = 1;

    void Encrypt(const UINT08* pt, UINT08* ct, const UINT32 byteCount)
    {
        memcpy(ct, pt, byteCount);
    }

    void SetContentKey(const UINT32 [AES::DW])
    {}

    const UINT32* GetContentKey() const
    {
        return nullptr;
    }

    void SetInitializatiolwector(const UINT32 [AES::DW])
    {}

    const UINT32* GetInitializatiolwector() const
    {
        return nullptr;
    }

    void StartOver()
    {}
};

#endif
