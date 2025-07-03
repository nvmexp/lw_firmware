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
#include "gbxxxism.h"

//------------------------------------------------------------------------------
// anonymous namespace to force usage of GBxxxGpuIsm::GetTable()
namespace
{
#if LWCFG(GLOBAL_GPU_IMPL_GB100)
    #include "gb100_ism.cpp"
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GB102)
    #include "gb102_ism.cpp"
#endif

}

//------------------------------------------------------------------------------
GBxxxGpuIsm::GBxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

//------------------------------------------------------------------------------
/* static */
vector<Ism::IsmChain>* GBxxxGpuIsm::GetTable(GpuSubdevice *pGpu)
{
// Adding ifdef outside the switch condition to avoid build errors in 
// gpu_drv_integ_stage_rel 
#if LWCFG(GLOBAL_GPU_IMPL_GB100)  || \
    LWCFG(GLOBAL_GPU_IMPL_GB102)
    switch (pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GB100)
        case Gpu::GB100:
            if (GB100SpeedoChains.empty())
            {
                InitializeGB100SpeedoChains();
            }
            return &GB100SpeedoChains;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GB102)
        case Gpu::GB102:
            if (GB102SpeedoChains.empty())
            {
                InitializeGB102SpeedoChains();
            }
            return &GB102SpeedoChains;
#endif
        default:
            MASSERT(!"Unknown GBxxx ISM Table. Check gpulist.h");
            break;
    }
#endif
    return nullptr;
}

RC GBxxxGpuIsm::GetISMClkFreqKhz(UINT32* pFreqkHz)
{
    MASSERT(pFreqkHz);
    return GetGpuSub()->GetClockSourceFreqKHz(LW2080_CTRL_CLK_SOURCE_XTAL, pFreqkHz);
}

CREATE_GPU_ISM_FUNCTION(GBxxxGpuIsm);

