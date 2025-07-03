/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2015,2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
// This implementation file is only different from GMxxxism.cpp because it requires
// the inclusion of the dev_ism.h from the gm204 directory. This header file
// defines a new ISM macro that is not available in GM10x and older parts.
// Note: The dev_ism.h files in the gm200/gm204/gm206 directories are identical.
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "gm2xxism.h"
#include "maxwell/gm204/dev_ism.h"
#include "core/include/utility.h"
#include "lwmisc.h"
//------------------------------------------------------------------------------
// anonymous namespace to force usage of GM2xxGpuIsm::GetTable()
namespace {
    #include "gm200_ism.cpp"
    #include "gm204_ism.cpp"
    #include "gm206_ism.cpp"
}
//------------------------------------------------------------------------------
GM2xxGpuIsm::GM2xxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain> *pTable)
    : GpuIsm(pGpu, pTable)
{
}

//--------------------------------------------------------------------------------------------------
RC GM2xxGpuIsm::GetNoiseMeasClk(UINT32 *pClkKhz)
{
    RC rc;
    LwU32 gpcClkKHz = 0;
    LwU32 sysClkKHz = 0;
    CHECK_RC(GetGpuSub()->GetClockSourceFreqKHz(LW2080_CTRL_CLK_SOURCE_GPCPLL,&gpcClkKHz));
    CHECK_RC(GetGpuSub()->GetClockSourceFreqKHz(LW2080_CTRL_CLK_SOURCE_SYSPLL,&sysClkKHz));
    MASSERT((gpcClkKHz != 0) && (sysClkKHz != 0));
    *pClkKhz = min(gpcClkKHz, sysClkKHz);
    return rc;
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain> * GM2xxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
    switch(pGpu->DeviceId())
    {
        case Gpu::GM200:
            if (GM200SpeedoChains.empty())
            {
                InitializeGM200SpeedoChains();
            }
            return &GM200SpeedoChains;
        case Gpu::GM204:
            if (GM204SpeedoChains.empty())
            {
                InitializeGM204SpeedoChains();
            }
            return &GM204SpeedoChains;
        case Gpu::GM206:
            if (GM206SpeedoChains.empty())
            {
                InitializeGM206SpeedoChains();
            }
            return &GM206SpeedoChains;
        default:
            MASSERT(!"Unknown GM2xx ISM Table");
            return NULL;
    }
    return NULL;
}

CREATE_GPU_ISM_FUNCTION(GM2xxGpuIsm);
