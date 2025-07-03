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

#include "gpu/include/pcicfg.h"
class GpuSubdevice;

//------------------------------------------------------------------------------
class PciCfgSpaceGpu : public PciCfgSpace
{
public:
    explicit PciCfgSpaceGpu(const GpuSubdevice* pSubdev) : m_pSubdev(pSubdev) {}
    RC Save() override;
private:
    const GpuSubdevice* m_pSubdev;
};
