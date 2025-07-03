/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "ga10xism.h"


//------------------------------------------------------------------------------
// anonymous namespace to force usage of GA10xGpuIsm::GetTable()
namespace
{
#if LWCFG(GLOBAL_GPU_IMPL_GA102) || LWCFG(GLOBAL_GPU_IMPL_GA102F)
    #include "ga102_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GA103)
    #include "ga103_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GA104)
    #include "ga104_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GA106)
    #include "ga106_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GA107)
    #include "ga107_ism.cpp"
#endif

    //--------------------------------------------------------------------------
    // Empty ISM Tables for nonimplemented GPUs
    //--------------------------------------------------------------------------
#if LWCFG(GLOBAL_GPU_IMPL_GH202)
    vector<GpuIsm::IsmChain> EmptySpeedoChains;
#endif
}

//------------------------------------------------------------------------------
GA10xGpuIsm::GA10xGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GAxxxGpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* GA10xGpuIsm::GetTable(GpuSubdevice *pGpu)
{
#if LWCFG(GLOBAL_GPU_IMPL_GA102)  || \
    LWCFG(GLOBAL_GPU_IMPL_GA102F) || \
    LWCFG(GLOBAL_GPU_IMPL_GA103)  || \
    LWCFG(GLOBAL_GPU_IMPL_GA104)  || \
    LWCFG(GLOBAL_GPU_IMPL_GA106)  || \
    LWCFG(GLOBAL_GPU_IMPL_GA107)  || \
    LWCFG(GLOBAL_GPU_IMPL_GH202)

    switch (pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GA102)
        case Gpu::GA102:
            if (GA102SpeedoChains.empty())
            {
                InitializeGA102SpeedoChains();
            }
            return &GA102SpeedoChains;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA102F)
        case Gpu::GA102F:
            if (GA102SpeedoChains.empty())
            {
                InitializeGA102SpeedoChains();
            }
            return &GA102SpeedoChains;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA103)
        case Gpu::GA103:
            if (GA103SpeedoChains.empty())
            {
                InitializeGA103SpeedoChains();
            }
            return &GA103SpeedoChains;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA104)
        case Gpu::GA104:
            if (GA104SpeedoChains.empty())
            {
                InitializeGA104SpeedoChains();
            }
            return &GA104SpeedoChains;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA106)
        case Gpu::GA106:
            if (GA106SpeedoChains.empty())
            {
                InitializeGA106SpeedoChains();
            }
            return &GA106SpeedoChains;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA107)
        case Gpu::GA107:
            if (GA107SpeedoChains.empty())
            {
                InitializeGA107SpeedoChains();
            }
            return &GA107SpeedoChains;
#endif            
#if LWCFG(GLOBAL_GPU_IMPL_GH202)
        case Gpu::GH202:
            return &EmptySpeedoChains; // return empty table until dev_ism.h defines are correct.
#endif            
        default:
            break;
    }
#endif

    MASSERT(!"Unknown GA10x ISM Table. Check gpulist.h");
    return nullptr;
}

RC GA10xGpuIsm::GetISMClkFreqKhz(UINT32* pFreqkHz)
{
    MASSERT(pFreqkHz);

    // On GA10X jtag_reg_clk is sourced directly from xtal_clk
    return GetGpuSub()->GetClockSourceFreqKHz(LW2080_CTRL_CLK_SOURCE_XTAL, pFreqkHz);
}

CREATE_GPU_ISM_FUNCTION(GA10xGpuIsm);

