/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "tegraism.h"
#include "core/include/utility.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "cheetah/include/tegclk.h"
#include "core/include/xp.h"
#include "cheetah/include/cpuutil.h"
#include "core/include/version.h"
#include <cstring>
#include "gpu/include/gpuism.h"
//------------------------------------------------------------------------------
namespace
{
#if LWCFG(GLOBAL_CHIP_T194)
    #include "t194_ism.cpp"
#else
    #include "t19x/t194/dev_ism.h"
#endif
#if LWCFG(GLOBAL_CHIP_T234)
    #include "t234_ism.cpp"
#else
    #include "t23x/t234/dev_ism.h"
#endif
}

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
TegraIsm::TegraIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : GpuIsm(pGpu, pTable)
{
}

vector<Ism::IsmChain>* GetTegraIsmTable(GpuSubdevice *pGpu)
{
    switch (pGpu->DeviceId())
    {
#if LWCFG(GLOBAL_CHIP_T194)
        case Gpu::T194:
            if (T194SpeedoChains.empty())
            {
                InitializeT194SpeedoChains();
            }
            return &T194SpeedoChains;
#endif
#if LWCFG(GLOBAL_CHIP_T234)
        case Gpu::T234:
            if (T234SpeedoChains.empty())
            {
                InitializeT234SpeedoChains();
            }
            return &T234SpeedoChains;
#endif
        default:
            MASSERT(!"Unknown CheetAh ISM Table");
            return NULL;
    }
    return NULL;
}

//------------------------------------------------------------------------------
/* virtual */ TegraIsm::~TegraIsm()
{
    if (m_bISMsEnabled)
    {
        EnableISMs(false);
        m_GpuRailGate.Release();
    }
}

//------------------------------------------------------------------------------
RC TegraIsm::GetISMClkFreqKhz(UINT32 *clkSrcFreqKHz)
{
#ifdef SIM_BUILD
    RC rc;

    Xp::ClockManager* const pClockMgr = Xp::GetClockManager();
    if (pClockMgr)
    {
        UINT64 clkMHandle;
        UINT64 clkMRateHz;
        CHECK_RC(CheetAh::GetClockHandle(MODS_CLKDEV_TEGRA_CLK_M, 0, &clkMHandle));
        CHECK_RC(pClockMgr->GetClockRate(clkMHandle, &clkMRateHz));
        *clkSrcFreqKHz = static_cast<UINT32>((clkMRateHz + 500) / 1000);
    }
    else
    {
        *clkSrcFreqKHz = 12000;
    }

    return rc;
#else
    return CheetAh::SocPtr()->GetISMClockKHz(clkSrcFreqKHz);
#endif
}

//------------------------------------------------------------------------------
/* virtual */ RC TegraIsm::EnableISMs(bool bEnable)
{
    if (!CheetAh::SocPtr()->IsApb2JtagInitialized())
        return RC::UNSUPPORTED_FUNCTION;

    if (bEnable == m_bISMsEnabled)
        return OK;

    RC rc;

    Utility::CleanupOnReturn<TegraGpuRailGateOwner>
            enIsmCleanup(&m_GpuRailGate, &TegraGpuRailGateOwner::Release);

    // Leaving ISMs enabled at all times on cheetah conflicts with GPU railgating
    // and cause odd crashes and hangs.  Only enable ISMs during ISMs collection
    // and while ISMs are enabled claim and disable GPU railgating so that
    // railgating cannot occur when ISMs are enabled.
    enIsmCleanup.Cancel();
    if (bEnable)
    {
        CHECK_RC(m_GpuRailGate.Claim());
        enIsmCleanup.Activate();
        CHECK_RC(m_GpuRailGate.SetDelayMs(-1));
    }
    CHECK_RC(CheetAh::SocPtr()->EnableISMs(bEnable));
    if (bEnable)
        enIsmCleanup.Cancel();
    else
        m_GpuRailGate.Release();

    m_bISMsEnabled = bEnable;
    return rc;
}
