/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/ism/gaxxxism.h"

//------------------------------------------------------------------------------
// GA10X GPU ISM implementation.
class GA10xGpuIsm : public GAxxxGpuIsm
{
public:
    GA10xGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);
    ~GA10xGpuIsm() override {}

    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);

    RC GetISMClkFreqKhz(UINT32* pFreqkHz) override;
};

