/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Range Picker Class Implementation

#include "gpu/include/rngpickr.h"
#include "core/include/utility.h"

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void RangePicker::AllocRange(UINT32 milwal, UINT32 maxVal, UINT32 Alignment)
{
    m_Alignment = Alignment;
    m_FreeRanges.push_back(RANGEPAIR(ALIGN_DOWN(milwal, m_Alignment),
                                     ALIGN_UP(maxVal, m_Alignment)));
}

//------------------------------------------------------------------------------

bool RangePicker::Pick(UINT32 *start, UINT32 *end)
{
    if (m_FreeRanges.size())
    {
        MASSERT(m_Initialized);
        RANGEPAIR tmpRange =
            m_FreeRanges[m_Rand.GetRandom(0, (UINT32)m_FreeRanges.size()-1)];
        *start = tmpRange.first;
        *end = tmpRange.second;
        return true;
    }
    else
        return false;
}

//------------------------------------------------------------------------------

bool RangePicker::PickTwoFromSameRange
(
    UINT32 *aStart,
    UINT32 *aEnd,
    UINT32 *bStart,
    UINT32 *bEnd
)
{
    if (!m_FreeRanges.size())
        return false;
    RANGEPAIR tmpRange = m_FreeRanges[0];
    if (m_FreeRanges.size() > 1)
    {
        MASSERT(m_Initialized);
        *aStart = tmpRange.first;
        *aEnd = tmpRange.second;
        tmpRange =
            m_FreeRanges[m_Rand.GetRandom(1, (UINT32)m_FreeRanges.size()-1)];
        *bStart = tmpRange.first;
        *bEnd = tmpRange.second;
        return true;
    }
    else
    {
        *aStart = tmpRange.first;
        *bEnd = tmpRange.second;
        UINT32 rSize = *bEnd - *aStart;
        *aEnd = ALIGN_DOWN(*aStart + (rSize / 2 - m_Alignment), m_Alignment);
        *bStart = ALIGN_UP(*aStart + (rSize / 2 + m_Alignment), m_Alignment);
        return false;
    }
}

//------------------------------------------------------------------------------

bool RangePicker::ExcludeRange(UINT32 inStart, UINT32 inEnd)
{
    UINT32 start = ALIGN_DOWN(inStart, m_Alignment);
    UINT32 end = ALIGN_UP(inEnd, m_Alignment);
    if (!m_FreeRanges.size())
        return false;
    if (start > end)
        return false;
    deque<RANGEPAIR >::iterator itMin = m_FreeRanges.begin();

    while ((itMin->second < start) && (itMin != m_FreeRanges.end()))
        ++itMin;
    if (itMin == m_FreeRanges.end())
        return false;
    if (itMin->second < end)
        return false;

    if ((itMin->first == start) && (itMin->second == end))
    {
        m_FreeRanges.erase(itMin);
        if (IsEmpty())
            Full = true;
        return true;
    }
    if (itMin->first == start)
    {
        itMin->first = end+m_Alignment;
        if (itMin->first > itMin->second)
            m_FreeRanges.erase(itMin);
        return true;
    }
    if (itMin->second == end)
    {
        itMin->second = start-m_Alignment;
        if (itMin->first > itMin->second)
            m_FreeRanges.erase(itMin);
        return true;
    }
    // Split current range into two ranges
    deque<RANGEPAIR >::iterator ittemp =
        m_FreeRanges.insert(itMin, RANGEPAIR(itMin->first,
                                             ALIGN_DOWN(start-1,m_Alignment)));
    (ittemp+1)->first = ALIGN_UP(end+1,m_Alignment);
    return true;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
