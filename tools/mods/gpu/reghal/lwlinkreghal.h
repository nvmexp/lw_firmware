/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "reghal.h"
#include "mods_reg_hal.h"

class LwLinkRegHal : public RegHal
{
public:
    LwLinkRegHal() = default;
    LwLinkRegHal(TestDevice* pTestDevice);
    virtual ~LwLinkRegHal() = default;

    // Non-copyable
    LwLinkRegHal(const LwLinkRegHal& rhs)            = delete;
    LwLinkRegHal& operator=(const LwLinkRegHal& rhs) = delete;

    UINT32 Read32
    (
        UINT32 domainIndex,
        ModsGpuRegAddress,
        ArrayIndexes regIndexes
    ) const override;
    void Write32
    (
        UINT32             domainIndex,
        RegSpace           space,
        ModsGpuRegAddress  regAddr,
        ArrayIndexes       regIndexes,
        UINT32             value
    ) override;
    void Write32Broadcast
    (
        UINT32             domainIndex,
        RegSpace           space,
        ModsGpuRegAddress  regAddr,
        ArrayIndexes       regIndexes,
        UINT32             value
    ) override;
    void Write32Sync
    (
        UINT32            domainIndex,
        ModsGpuRegAddress regAddr,
        ArrayIndexes      regIndexes,
        UINT32            value
    ) override;
    void Write32
    (
        UINT32          domainIndex,
        RegSpace        space,
        ModsGpuRegField regField,
        ArrayIndexes    regIndexes,
        UINT32          value
    ) override;
    void Write32Sync
    (
        UINT32          domainIndex,
        ModsGpuRegField regField,
        ArrayIndexes    regIndexes,
        UINT32          value
    ) override;

    UINT64 Read64
    (
        UINT32 domainIndex,
        ModsGpuRegAddress,
        ArrayIndexes regIndexes
    ) const override;
    void Write64
    (
        UINT32             domainIndex,
        RegSpace           space,
        ModsGpuRegAddress  regAddr,
        ArrayIndexes       regIndexes,
        UINT64             value
    ) override;
    void Write64Broadcast
    (
        UINT32             domainIndex,
        RegSpace           space,
        ModsGpuRegAddress  regAddr,
        ArrayIndexes       regIndexes,
        UINT64             value
    ) override;
    void Write64Sync
    (
        UINT32            domainIndex,
        ModsGpuRegAddress regAddr,
        ArrayIndexes      regIndexes,
        UINT64            value
    ) override;
    void Write64
    (
        UINT32          domainIndex,
        RegSpace        space,
        ModsGpuRegField regField,
        ArrayIndexes    regIndexes,
        UINT64          value
    ) override;
    void Write64Sync
    (
        UINT32          domainIndex,
        ModsGpuRegField regField,
        ArrayIndexes    regIndexes,
        UINT64          value
    ) override;
};
