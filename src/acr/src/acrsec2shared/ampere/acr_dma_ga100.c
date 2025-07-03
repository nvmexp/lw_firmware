/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_dma_ga100.c 
 */
// Includes
//
#include "acr.h"
#include "dev_graphics_nobundle.h"
#include "dev_smcarb.h"
#include "dev_smcarb_addendum.h"
#include "hwproject.h"

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif
/*!
 * @brief Program ARB to allow physical transactions for FECS as it doesn't have FBIF.
 * Not required for GPCCS as it is priv loaded.
 * Ampere also has SMC support so program correct FECS instance if SMC is enabled.
 */
ACR_STATUS
acrlibSetupFecsDmaThroughArbiter_GA100
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwBool           bIsPhysical
)
{
    LwU32 data;
    LwU32 regFecsArbWpr;
    LwU32 regFecsCmdOvr;

    //
    // SEC always uses extended address space to bootload FECS-0 of a GR. And thus we also skip the check of
    // unprotected pric SMC_INFO which is vulnerable to attacks. Tracked in bug 2565457.
    //
    regFecsArbWpr = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_WPR, pFlcnCfg->falconInstance);
    regFecsCmdOvr = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_CMD_OVERRIDE, pFlcnCfg->falconInstance);

    if (bIsPhysical)
    {
        data = ACR_REG_RD32(BAR0, regFecsArbWpr);
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_WPR, _CMD_OVERRIDE_PHYSICAL_WRITES, _ALLOWED, data);
        ACR_REG_WR32(BAR0, regFecsArbWpr, data);
    }

    data = ACR_REG_RD32(BAR0, regFecsCmdOvr);
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _CMD, _PHYS_VID_MEM, data);

    if (bIsPhysical)
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _ON, data);
    }
    else
    {
        data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_CMD_OVERRIDE, _ENABLE, _OFF, data);
    }

    ACR_REG_WR32(BAR0, regFecsCmdOvr, data);

    return ACR_OK;
}
