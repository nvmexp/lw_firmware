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

#include "gpu/pcie/pcicfggpu.h"

//------------------------------------------------------------------------------
class TuringPciCfgSpace : public PciCfgSpaceGpu
{
public:
    explicit TuringPciCfgSpace(const GpuSubdevice* pSubdev) :
        PciCfgSpaceGpu(pSubdev) {}
    RC UseXveRegMap(UINT32 regMap) override;
};
