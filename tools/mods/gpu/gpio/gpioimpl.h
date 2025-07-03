/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "device/interface/gpio.h"
#include "gpu/utility/gpuerrcount.h"

#include <map>

class GpioImpl : public Gpio
{
protected:
    friend class GpioChecker;
    class GpioChecker : public ErrCounter
    {
    public:
        struct GpioCheckInfo
        {
            UINT32 maxAllowed;
            bool   disableWhenMaxReached;
        };

        GpioChecker(Gpio* pGpio);

        RC Initialize(const map<UINT32, GpioCheckInfo>& checks);
        RC Update(UINT32 Key);
    protected:
        RC OnError(const ErrorData *pData) override;
        RC ReadErrorCount(UINT64 *pCount) override;
    private:
        Gpio*               m_pGpio = nullptr;
        bool                m_Active = false;
        // Error counter uses pCount as a array for a list of counters. Our m_Count
        // and m_MaxAllowed need to match the index in pCount. Use m_KeyToIdx as a
        // way to map the Key to an index into m_Count & m_MaxAllowed
        map<UINT32, UINT32> m_KeyToIdx;     // Key, Index pair
        vector<UINT32>      m_Count;
        vector<UINT32>      m_MaxAllowed;
        vector<UINT32>      m_ToDisable;
    };

    struct GpioActivity
    {
        UINT32    pinNum = 0;
        UINT32    rmEventId = 0;
        Direction dir = FALLING;
        Gpio*     pGpio = nullptr;
    };

    GpioImpl() = default;
    virtual ~GpioImpl() = default;

    // this is for look up
    static UINT32 ActivityToKey(UINT32 pinNum, Direction dir);
    static void KeyToActivity(UINT32 key, GpioActivity* pActivity);

protected:
    virtual TestDevice* GetDevice() = 0;
    virtual const TestDevice* GetDevice() const = 0;
};
