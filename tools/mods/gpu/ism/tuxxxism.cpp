
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
#include "tuxxxism.h"
#include "core/include/utility.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GPxxxGpuIsm::GetTable()
namespace
{
#if LWCFG(GLOBAL_GPU_IMPL_TU102)
    #include "tu102_ism.cpp"
#else
    #include "turing/tu102/dev_ism.h"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU104)
    #include "tu104_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU106)
    #include "tu106_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU116)
    #include "tu116_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_TU117)
    #include "tu117_ism.cpp"
#endif

    //--------------------------------------------------------------------------
    // TUxxx empty ISM Tables
    //--------------------------------------------------------------------------
}
//------------------------------------------------------------------------------
TUxxxGpuIsm::TUxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* TUxxxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
    switch (pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_TU102)
        case Gpu::TU102:
            if (TU102SpeedoChains.empty())
            {
                InitializeTU102SpeedoChains();
            }
            return &TU102SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_TU104)
        case Gpu::TU104:
            if (TU104SpeedoChains.empty())
            {
                InitializeTU104SpeedoChains();
            }
            return &TU104SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_TU106)
        case Gpu::TU106:
            if (TU106SpeedoChains.empty())
            {
                InitializeTU106SpeedoChains();
            }
            return &TU106SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_TU116)
        case Gpu::TU116:
            if (TU116SpeedoChains.empty())
            {
                InitializeTU116SpeedoChains();
            }
            return &TU116SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_TU117)
        case Gpu::TU117:
            if (TU117SpeedoChains.empty())
            {
                InitializeTU117SpeedoChains();
            }
            return &TU117SpeedoChains;
#endif

        default:
             MASSERT(!"Unknown TUxxx ISM Table");
             return NULL;
    }
    return NULL;
}

//-------------------------------------------------------------------------------------------------
// Return the proper frequency for callwlating measurement time.
RC TUxxxGpuIsm::GetNoiseMeasClk(UINT32 *pClkKhz)
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

CREATE_GPU_ISM_FUNCTION(TUxxxGpuIsm);
