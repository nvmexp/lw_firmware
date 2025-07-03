/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "dummygpu.h"

//------------------------------------------------------------------------------
DummyGpuSubdevice::DummyGpuSubdevice
(
    const char *Name,
    Gpu::LwDeviceId DeviceId,
    const PciDevInfo *pPciDevInfo
) : StubGpuSubdevice(Name, DeviceId, pPciDevInfo)
{
}

//-------------------------------------------------------------------------------
CREATE_GPU_SUBDEVICE_FUNCTION(DummyGpuSubdevice);
