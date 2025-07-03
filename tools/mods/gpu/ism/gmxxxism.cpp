/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2015, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gmxxxism.h"
#include "maxwell/gm108/dev_ism.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GMxxxGpuIsm::GetTable()
namespace {

    #include "gm107_ism.cpp"
    #include "gm108_ism.cpp"
}

//------------------------------------------------------------------------------
GMxxxGpuIsm::GMxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* GMxxxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
    switch(pGpu->DeviceId())
    {
        case Gpu::GM107:
            if (GM107SpeedoChains.empty())
            {
                InitializeGM107SpeedoChains();
            }
            return &GM107SpeedoChains;
        case Gpu::GM108:
            if (GM108SpeedoChains.empty())
            {
                InitializeGM108SpeedoChains();
            }
            return &GM108SpeedoChains;
        default:
            MASSERT(!"Unknown GMxxx ISM Table");
            return NULL;
    }
    return NULL;
}

CREATE_GPU_ISM_FUNCTION(GMxxxGpuIsm);
