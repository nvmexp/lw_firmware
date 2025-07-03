/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/include/gpuism.h"

//------------------------------------------------------------------------------
// Blackwell GPU ISM implementation.
class GBxxxGpuIsm : public GpuIsm
{
public:
    GBxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);
    ~GBxxxGpuIsm() override {}

    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);

    RC GetISMClkFreqKhz(UINT32* pFreqkHz) override;
};

