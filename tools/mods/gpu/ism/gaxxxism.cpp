
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gaxxxism.h"
#include "core/include/utility.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GAxxxGpuIsm::GetTable()
namespace
{
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
    #include "ga100_ism.cpp"
#endif
}
//------------------------------------------------------------------------------
GAxxxGpuIsm::GAxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* GAxxxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
    switch (pGpu->DeviceId())
    {
        case Gpu::GA100:
            if (GA100SpeedoChains.empty())
            {
                InitializeGA100SpeedoChains();
            }
            return &GA100SpeedoChains;
        default:
            break;
    }
#endif

    MASSERT(!"Unknown GAxxx ISM Table");
    return nullptr;
}

//------------------------------------------------------------------------------------------------
// Return the proper frequency for callwlating measurement time.
RC GAxxxGpuIsm::GetNoiseMeasClk(UINT32 *pClkKhz)
{
    RC rc;
    LwU32 gpcClkKHz = 0;
    LwU32 sysClkKHz = 0;
    CHECK_RC(GetGpuSub()->GetClockDomainFreqKHz(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &gpcClkKHz));
    CHECK_RC(GetGpuSub()->GetClockDomainFreqKHz(LW2080_CTRL_CLK_DOMAIN_SYSCLK, &sysClkKHz));

    // If either of these clocks reports zero than something is wrong!
    if (gpcClkKHz == 0 || sysClkKHz == 0)
    {
        Printf(Tee::PriError, "Unable to  get GPC and/or SYS clk frequencies.");
        return RC::SOFTWARE_ERROR;
    }

    *pClkKhz = min(gpcClkKHz, sysClkKHz);
    return rc;
}

CREATE_GPU_ISM_FUNCTION(GAxxxGpuIsm);
