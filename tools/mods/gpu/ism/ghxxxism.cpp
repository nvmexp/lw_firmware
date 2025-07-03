
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "ghxxxism.h"
#include "core/include/utility.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GPxxxGpuIsm::GetTable()
namespace
{
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
    #include "gh100_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GH202)
    #include "gh202_ism.cpp"
#endif

}
//------------------------------------------------------------------------------
GHxxxGpuIsm::GHxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* GHxxxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
    switch (pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
        case Gpu::GH100:
            if (GH100SpeedoChains.empty())
            {
                InitializeGH100SpeedoChains();
            }
            return &GH100SpeedoChains;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH202)
        case Gpu::GH202:
            if (GH202SpeedoChains.empty())
            {
                InitializeGH202SpeedoChains();
            }
            return &GH202SpeedoChains;
#endif
        default:
             MASSERT(!"Unknown GHxxx ISM Table");
             return NULL;
    }
    return NULL;
}

//------------------------------------------------------------------------------------------------
RC GHxxxGpuIsm::GetISMClkFreqKhz(UINT32* pFreqkHz)
{
    MASSERT(pFreqkHz);
    return GetGpuSub()->GetClockSourceFreqKHz(LW2080_CTRL_CLK_SOURCE_XTAL, pFreqkHz);
}

//------------------------------------------------------------------------------------------------
// Return the proper frequency for callwlating measurement time.
RC GHxxxGpuIsm::GetNoiseMeasClk(UINT32 *pClkKhz)
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

INT32 GHxxxGpuIsm::GetISMOutDivForFreqCalc(PART_TYPES partType, INT32 outDiv)
{
    // On GH100 the IMON output divider does follow the normal rules. There are 3 bits
    // however the most significant bit is a field select and not a divider value, so 
    // mask it out.
    if (partType == IMON)
    {
        return outDiv & 0x3;
    }
    else
    {
        return outDiv;
    }
}

CREATE_GPU_ISM_FUNCTION(GHxxxGpuIsm);
