/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gvxxxism.h"
#include "core/include/utility.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GPxxxGpuIsm::GetTable()
namespace
{
    #include "gv100_ism.cpp"

    //--------------------------------------------------------------------------
    // GV000 ISM Table
    //--------------------------------------------------------------------------
    vector<GpuIsm::IsmChain> GV000SpeedoChains;
}
//------------------------------------------------------------------------------
GVxxxGpuIsm::GVxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* GVxxxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
    switch (pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_G000) || LWCFG(GLOBAL_CHIP_T194)
#if LWCFG(GLOBAL_GPU_IMPL_G000)
        case Gpu::G000:
#endif
#if LWCFG(GLOBAL_CHIP_T194)
        case Gpu::GV11B:
#endif
            return &GV000SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GV100)
        case Gpu::GV100:
            if (GV100SpeedoChains.empty())
            {
                InitializeGV100SpeedoChains();
            }
            return &GV100SpeedoChains;
#endif

        default:
             MASSERT(!"Unknown GVxxx ISM Table");
             return NULL;
    }
    return NULL;
}


//-------------------------------------------------------------------------------------------------
// Return the proper frequency for callwlating measurement time.
RC GVxxxGpuIsm::GetNoiseMeasClk(UINT32 *pClkKhz)
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
CREATE_GPU_ISM_FUNCTION(GVxxxGpuIsm);
