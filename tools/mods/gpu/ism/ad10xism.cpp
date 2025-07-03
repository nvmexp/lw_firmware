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
#include "gpu/include/gpusbdev.h"
#include "ad10xism.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GA10xGpuIsm::GetTable()
namespace
{
#if LWCFG(GLOBAL_GPU_IMPL_AD102) || LWCFG(GLOBAL_GPU_IMPL_AD102F)
    #include "ad102_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_AD103)
    #include "ad103_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD104)
    #include "ad104_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD106)
    #include "ad106_ism.cpp"
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD107)
    #include "ad107_ism.cpp"
#endif

}

//------------------------------------------------------------------------------
AD10xGpuIsm::AD10xGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* AD10xGpuIsm::GetTable(GpuSubdevice *pGpu)
{
    switch (pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_AD102)
        case Gpu::AD102:
            if (AD102SpeedoChains.empty())
            {
                InitializeAD102SpeedoChains();
            }
            return &AD102SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD102F)
        case Gpu::AD102F:
            if (AD102SpeedoChains.empty())
            {
                InitializeAD102SpeedoChains();
            }
            return &AD102SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD103)
        case Gpu::AD103:
            if (AD103SpeedoChains.empty())
            {
                InitializeAD103SpeedoChains();
            }
            return &AD103SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD104)
        case Gpu::AD104:
            if (AD104SpeedoChains.empty())
            {
                InitializeAD104SpeedoChains();
            }
            return &AD104SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD106)
        case Gpu::AD106:
            if (AD106SpeedoChains.empty())
            {
                InitializeAD106SpeedoChains();
            }
            return &AD106SpeedoChains;
#endif

#if LWCFG(GLOBAL_GPU_IMPL_AD107)
        case Gpu::AD107:
            if (AD107SpeedoChains.empty())
            {
                InitializeAD107SpeedoChains();
            }
            return &AD107SpeedoChains;
#endif
        default:
            MASSERT(!"Unknown AD10x ISM Table. Check gpulist.h");
            break;
    }

    return nullptr;
}

RC AD10xGpuIsm::GetISMClkFreqKhz(UINT32* pFreqkHz)
{
    MASSERT(pFreqkHz);

    // On AD10X jtag_reg_clk is sourced directly from xtal_clk
    return GetGpuSub()->GetClockSourceFreqKHz(LW2080_CTRL_CLK_SOURCE_XTAL, pFreqkHz);
}

CREATE_GPU_ISM_FUNCTION(AD10xGpuIsm);

