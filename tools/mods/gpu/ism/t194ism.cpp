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
#include "core/include/utility.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "t19x/t194/address_map.h"
#include "cheetah/include/tegclk.h"
#include "t194ism.h"

//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------
T194Ism::T194Ism(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable)
     : TegraIsm(pGpu, pTable)
{
}

//-------------------------------------------------------------------------------------------------
// Return the proper frequency for callwlating measurement time.
// This is TUxxxIsm implementation
RC T194Ism::GetNoiseMeasClk(UINT32 *pClkKhz)
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

//------------------------------------------------------------------------------
#define LW_LA_ISM_NM_CTRL  0x00000700
RC T194Ism::TriggerISMExperiment()
{
    return GpuPtr()->SocRegWr32(LW_DEVID_LA, 0, LW_LA_ISM_NM_CTRL, 1);
}
CREATE_TEGRA_ISM_FUNCTION(T194Ism);
