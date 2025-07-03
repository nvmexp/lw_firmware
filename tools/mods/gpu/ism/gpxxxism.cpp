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

#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "gpxxxism.h"
#include "core/include/utility.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GPxxxGpuIsm::GetTable()
namespace {

    //When the real ISM table becomes available, uncomment the appropriate #include
    //and remove the NULL version of the table below.
#if LWCFG(GLOBAL_GPU_IMPL_GP100)
    #include "gp100_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GP102)
    #include "gp102_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GP104)
    #include "gp104_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GP106)
    #include "gp106_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GP107)
    #include "gp107_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GP108)
    #include "gp108_ism.cpp"
#endif

    //--------------------------------------------------------------------------
    // GP107F ISM Table
    //--------------------------------------------------------------------------
}
//------------------------------------------------------------------------------
GPxxxGpuIsm::GPxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* GPxxxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
    switch(pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GP100)
        case Gpu::GP100:
            if (GP100SpeedoChains.empty())
            {
                InitializeGP100SpeedoChains();
            }
            return &GP100SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GP102)
        case Gpu::GP102:
            if (GP102SpeedoChains.empty())
            {
                InitializeGP102SpeedoChains();
            }
            return &GP102SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GP104)
        case Gpu::GP104:
            if (GP104SpeedoChains.empty())
            {
                InitializeGP104SpeedoChains();
            }
            return &GP104SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GP106)
        case Gpu::GP106:
            if (GP106SpeedoChains.empty())
            {
                InitializeGP106SpeedoChains();
            }
            return &GP106SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GP107)
        case Gpu::GP107:
            if (GP107SpeedoChains.empty())
            {
                InitializeGP107SpeedoChains();
            }
            return &GP107SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GP108)
        case Gpu::GP108:
            if (GP108SpeedoChains.empty())
            {
                InitializeGP108SpeedoChains();
            }
            return &GP108SpeedoChains;
#endif
        default:
             MASSERT(!"Unknown GPxxx ISM Table");
             return NULL;
    }
    return NULL;
}

//--------------------------------------------------------------------------------------------------
// Return the proper frequency for callwlating measurement time.
RC GPxxxGpuIsm::GetNoiseMeasClk(UINT32 *pClkKhz)
{
    RC rc;

    LwU32 gpcClkKHz = 0;
    LwU32 sysClkKHz = 0;

    CHECK_RC(GetGpuSub()->GetClockDomainFreqKHz(LW2080_CTRL_CLK_DOMAIN_GPC2CLK, &gpcClkKHz));
    CHECK_RC(GetGpuSub()->GetClockDomainFreqKHz(LW2080_CTRL_CLK_DOMAIN_SYS2CLK, &sysClkKHz));

    // If either of these clocks reports zero than something is wrong!
    if (gpcClkKHz == 0 || sysClkKHz == 0)
    {
        Printf(Tee::PriError, "Unable to  get GPC and/or SYS clk frequencies.");
        return RC::SOFTWARE_ERROR;
    }
    
    *pClkKhz = min(gpcClkKHz, sysClkKHz);
    return rc;
}

CREATE_GPU_ISM_FUNCTION(GPxxxGpuIsm);
