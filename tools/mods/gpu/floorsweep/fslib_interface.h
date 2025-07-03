/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "fs_lib.h"
#include "core/include/device.h"

namespace FsLib
{
    using GpcMasks = fslib::GpcMasks;
    using FbpMasks = fslib::FbpMasks;
    using FsMode   = fslib::FSmode;

    RC SetMode(FsMode mode);
    RC LwDeviceIdToChip(Device::LwDeviceId id, fslib::Chip *pChip);
    bool IsSupported(Device::LwDeviceId id);
    bool IsFloorsweepingValid
    (
        Device::LwDeviceId devId,
        const GpcMasks& gpcMasks,
        const FbpMasks& fbpMasks,
        string &errMsg
    );
    void PrintFsMasks
    (
        const GpcMasks& gpcMasks,
        const FbpMasks& fbpMasks,
        Tee::Priority pri
    );
}
