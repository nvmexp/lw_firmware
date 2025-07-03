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

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef RANGE_PICKER
#define RANGE_PICKER

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_MEMTYPES_H
#include "core/include/memtypes.h"
#endif
#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif
#ifndef INCLUDED_MEMORYIF_H
#include <memory>
#endif
#ifndef INCLUDED_STL_DEQUE
#define INCLUDED_STL_DEQUE
#include <deque>
#endif
#ifndef INCLUDED_RANDOM_H
#include "random.h"
#endif

#define RANGEPAIR pair<UINT32, UINT32>
// Range Picker - returns range of free regions; Used lwrrently with memory copy to
// avoid using the same region of memory for both source and destination.
class RangePicker
{
public:

    RangePicker():m_FreeRanges(0),m_Alignment(0),Full(false){};
    RangePicker(UINT32 milwal, UINT32 maxVal, UINT32 Alignment)
    {AllocRange(milwal, maxVal, Alignment);};
    ~RangePicker(){m_FreeRanges.clear();};
    void InitSeed(UINT32 seed) { m_Rand.SeedRandom(seed); m_Initialized = true; }

    //! Randomly pick a subrange from the list
    bool Pick(UINT32 *start, UINT32 *end);

    //! Randomly pick 2 subranges, making sure there is no overlap
    bool PickTwoFromSameRange
    (
        UINT32 *aStart,
        UINT32 *aEnd,
        UINT32 *bStart,
        UINT32 *bEnd
    );

    //! Remove a range of values from free range list; keep list sorted
    bool ExcludeRange(UINT32 inStart, UINT32 inEnd);
    //! Clear and set minimum and maximum value for a range
    void AllocRange(UINT32 milwal, UINT32 maxVal, UINT32 Alignment);

    bool IsEmpty(){return (m_FreeRanges.size() == 0);};

    bool IsFull() {return Full;};

private:

    deque<RANGEPAIR> m_FreeRanges; // Deque of free range pairs (begin, end)
    UINT32 m_Alignment;  // Alignment of memory range; Stored as power of 2.
    bool Full;  // Bool indicating if all of the range has been used
    bool m_Initialized = false;
    Random m_Rand;
};
#endif
