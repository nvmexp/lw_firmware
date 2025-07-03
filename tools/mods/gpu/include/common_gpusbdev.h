/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef COMMON_GPUSBDEV_H_INCLUDED
#define COMMON_GPUSBDEV_H_INCLUDED

#include "gpusbdev.h"

namespace GT21xGpuSubdevice
{
    string ChipIdCookedImpl(GpuSubdevice *pGpuSubdevice);
    UINT32 GetEngineStatusImpl(GpuSubdevice *pGpuSubdevice,
                               GpuSubdevice::Engine EngineName);
    RC GetKFuseStatusImpl(GpuSubdevice *pGpuSubdevice,
                          GpuSubdevice::KFuseStatus *pStatus,
                          FLOAT64 TimeoutMs);
}

namespace GT20xGpuSubdevice
{
    RC SetIntThermCalibrationImpl(GpuSubdevice *pGpuSubdevice,
                                  UINT32 ValueA,
                                  UINT32 ValueB);
    RC SetDebugStateImpl(GpuSubdevice *pGpuSubdevice,
                         string        Name,
                         UINT32        Value);
}

#endif // COMMON_GPUSBDEV_H_INCLUDED
