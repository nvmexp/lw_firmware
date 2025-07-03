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

#include "t234ism.h"
#include "t23x/t234/address_map.h"

T234Ism::T234Ism(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
    : TegraIsm(pGpu, pTable)
{
}

RC T234Ism::GetNoiseMeasClk(UINT32 *pClkKhz)
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

static constexpr UINT32 LW_LA_ISM_NM_CTRL = 0x00000700;

RC T234Ism::TriggerISMExperiment()
{
    return GpuPtr()->SocRegWr32(LW_DEVID_LA, 0, LW_LA_ISM_NM_CTRL, 1);
}

CREATE_TEGRA_ISM_FUNCTION(T234Ism);
