/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  syncpoint.h
 * @brief Abstraction representing a syncpoint and a syncpoint base (CheetAh)
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_SYNCPOINT_H
#define INCLUDED_SYNCPOINT_H

#include "core/include/types.h"
#include "core/include/rc.h"
#include "core/include/lwrm.h"

class SyncPoint
{
public:
    SyncPoint();
    ~SyncPoint();

    //! Attempt to force syncpoint index (by default any free index will be chosen).
    //! This function must be called before calling Alloc().
    void ForceIndex(UINT32 index) { m_ForceIndex = index; }

    //! Allocate syncpoint. GPU subdevice must be passed.
    //! @sa SetForceIndex, SetValue, SetThreshold
    RC Alloc(GpuDevice* pGpuDev, LwRm* pLwRm=0);
    //! Free syncpoint.
    void Free();

    //! Returns allocated syncpoint's index.
    UINT32 GetIndex() const { return m_Index; }
    //! Sets a new syncpoint value.
    //! This function can be called before and after the syncpoint is allocated.
    RC SetValue(UINT32 value);
    //! Returns value and threshold of the syncpoint.
    RC GetValue(UINT32* pValue, UINT32* pThreshold=0) const;
    //! Set threshold and threshold type.
    //! This function can be called before and after Alloc.
    RC SetThreshold(UINT32 threshold, bool autoIncrement);
    //! Indicates whether the threshold is auto incrementing.
    bool IsAutoIncrementing() const { return m_AutoIncrement; }
    //! Resets the syncpoint to value 0 and resets threshold to 1 for autoincrementing syncpoints.
    RC Reset();
    //! Waits for the syncpoint to reach a certain value.
    RC Wait(UINT32 Value, FLOAT64 Timeout) const;

private:
    // Non-copyable
    SyncPoint(const SyncPoint&);
    SyncPoint& operator=(const SyncPoint&);

    struct SyncPointWaitArgs
    {
        const SyncPoint* pSyncPoint;  //!< Pointer to syncpoint class
        UINT32           value;       //!< Value to wait for
        RC               pollRc;      //!< RC from polling function
    };
    static bool WaitValue(void * pvWaitArgs);

    LwRm* m_pLwRm;
    LwRm::Handle m_Handle;
    UINT32 m_InitValue;
    UINT32 m_ForceIndex;
    bool m_AutoIncrement;
    UINT32 m_Threshold;
    UINT32 m_Index;
};

class SyncPointBase
{
public:
    SyncPointBase();
    ~SyncPointBase();

    //! Attempt to force syncpoint base index (by default any free index will be chosen).
    //! This function must be called before calling Alloc().
    void ForceIndex(UINT32 index) { m_ForceIndex = index; }

    //! @sa SetForceIndex, SetValue
    RC Alloc(GpuDevice* pGpuDev, LwRm* pLwRm=0);
    //! Free syncpoint base.
    void Free();

    //! Returns allocated syncpoint base's index.
    UINT32 GetIndex() const { return m_Index; }
    //! Sets a new syncpoint base value.
    //! This function can be called before and after the syncpoint base is allocated.
    RC SetValue(UINT32 value);
    //! Returns value of the syncpoint base.
    RC GetValue(UINT32* pValue) const;
    //! Reset the value of the syncpoint base to 0;
    RC Reset();

private:
    // Non-copyable
    SyncPointBase(const SyncPointBase&);
    SyncPointBase& operator=(const SyncPointBase&);

    LwRm* m_pLwRm;
    LwRm::Handle m_Handle;
    UINT32 m_InitValue;
    UINT32 m_ForceIndex;
    UINT32 m_Index;
};

#endif // INCLUDED_SYNCPOINT_H
