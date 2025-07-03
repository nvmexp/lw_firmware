/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/rc.h"
#include "gpu/pcie/turingpcicfg.h"

#include "turing/tu102/dev_lw_pcfg_xve_regmap.h"
#include "turing/tu102/dev_lw_xve3g_fn_xusb_regmap.h"
#include "turing/tu102/dev_lw_xve3g_fn_ppc_regmap.h"

namespace
{
    UINT32 s_XveRegWriteMapFn0[] = LW_PCFG_XVE_REGISTER_WR_MAP;
    UINT32 s_XveRegWriteMapFn2[] = LW_XVE_XUSB_REGISTER_WR_MAP;
    UINT32 s_XveRegWriteMapFn3[] = LW_XVE_PPC_REGISTER_WR_MAP;
}

RC TuringPciCfgSpace::UseXveRegMap(UINT32 xveMap)
{
    switch (xveMap)
    {
        case FUNC_GPU:
            m_XveRegMap = s_XveRegWriteMapFn0;
            m_XveRegMapLength = sizeof(s_XveRegWriteMapFn0)/sizeof(s_XveRegWriteMapFn0[0]);
            break;
        case FUNC_AZALIA:
            return PciCfgSpaceGpu::UseXveRegMap(xveMap);
        case FUNC_XUSB:
            m_XveRegMap = s_XveRegWriteMapFn2;
            m_XveRegMapLength = sizeof(s_XveRegWriteMapFn2)/sizeof(s_XveRegWriteMapFn2[0]);
            break;
        case FUNC_PPC:
            m_XveRegMap = s_XveRegWriteMapFn3;
            m_XveRegMapLength = sizeof(s_XveRegWriteMapFn3)/sizeof(s_XveRegWriteMapFn3[0]);
            break;
        default:
            MASSERT(!"Unknown index for xve regmap.");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    return RC::OK;
}
