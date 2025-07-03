/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include <cstddef>
#include <functional>

#include "core/include/types.h"

#include "inbitstream.h"
#include "vc1syntax.h"

namespace VC1
{
const UINT08 pQuantForImplicitQuantizer[32] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    19, 20, 21, 22, 23, 24, 25, 27, 29, 31
};

size_t BDU::FindNextStartCode(
    const UINT08 *bufStart,
    size_t searchFromOffset,
    size_t bufSize
)
{
    const UINT08 *p = bufStart + searchFromOffset;
    UINT32 val;

    if (p[0] == 0 && p[1] == 0 && p[2] == 1)
    {
        p += 3;
        searchFromOffset += 3;
    }
    val = 0xffffffff;
    while (searchFromOffset < bufSize - 3)
    {
        val <<= 8;
        val |= *p++;
        searchFromOffset++;
        if ((val & 0x00ffffff) == 1)
        {
            return searchFromOffset - 3;
        }
    }
    return 0;
}

bool BDU::Read(const UINT08 *bufStart, size_t searchFromOffset, size_t bufSize)
{
    m_startOffset = searchFromOffset;
    size_t nextBduOffset = FindNextStartCode(
        bufStart,
        searchFromOffset,
        bufSize
    );
    bool hasNext = 0 != nextBduOffset;
    if (!hasNext)
    {
        nextBduOffset = bufSize;
    }
    m_ebduDataSize = nextBduOffset - searchFromOffset;

    // BDU type is the start code suffix, which is always at the offset 3
    m_bduType = static_cast<BDUType>(bufStart[searchFromOffset + 3]);

    size_t rbspStart = 3 + 1; // start code size plus start code suffix

    m_rbduData.resize(m_ebduDataSize - rbspStart);
    memcpy(
        &m_rbduData[0],
        bufStart + searchFromOffset + rbspStart,
        m_rbduData.size()
    );

    // remove trailing zeroes as the standard requests
    vector<UINT08>::reverse_iterator it = std::find_if(
        m_rbduData.rbegin(),
        m_rbduData.rend(),
        [](UINT08 value) -> bool { return value != 0; }
    );
    m_rbduData.erase(it.base(), m_rbduData.end());

    if (m_rbduData.size() > 3)
    {
        // remove start code emulation prevention data, see E.2 of Annex E of
        // SMPTE 421M-2006
        UINT08 *finish = &m_rbduData[0] + m_rbduData.size() - 2;
        for (UINT08 *p = &m_rbduData[0]; p < finish; ++p)
        {
            if (p[0] == 0 && p[1] == 0 && p[2] == 3)
            {
                ++p;
                memmove(p + 1, p + 2, finish - p);
                --finish;
            }
        }
        m_rbduData.resize(finish + 2 - &m_rbduData[0]);
    }

    return hasNext;
}

void BitPlane::ClearColumn(size_t x)
{
    Container::iterator it = m_data.begin() + x;
    for (size_t y = 0; m_height > y; ++y, it += m_width)
    {
        *it = 0;
    }
}

void BitPlane::ClearRow(size_t fromCol, size_t y)
{
    Container::iterator it = m_data.begin() + fromCol + y * m_width;
    for (size_t x = fromCol; m_width > x; ++x)
    {
        *it++ = 0;
    }
}

} // namespace VC1
