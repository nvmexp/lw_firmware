/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_acrgh100.c
 * @brief  ACR interfaces for GH100 and later chips.
 */

/* ------------------------ System includes -------------------------------- */
#include "sec2sw.h"
#include "sec2_bar0_hs.h"
#include "sec2_posted_write.h"
#include "lwos_dma_hs.h"

/* ------------------------ Application includes --------------------------- */
#include "sec2_objsec2.h"
#include "sec2_objacr.h"
#include "dev_graphics_nobundle.h"
#include "hwproject.h"
#include "dev_smcarb_addendum.h"
#include "config/g_acr_private.h"
#include "config/g_sec2_hal.h"
#include "g_acrlib_hal.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*
 * Setup FECS registers before bootstrapping it. More details in bug 2627329
 */
FLCN_STATUS
acrSetupCtxswRegisters_GH100
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32       data       = 0;
    LwU32       addr       = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_CTXSW_GFID, pFlcnCfg->falconInstance);
    FLCN_STATUS flcnStatus = FLCN_OK;

    //
    // FECS ucode reads GFID value from LW_PGRAPH_PRI_FECS_CTXSW_GFID, at the time of instance bind,
    // as the instance bind request will be sent using this GFID. The value of this register is 0 out of reset however
    // HW wants RM to explicitly program the GFID to 0.
    //
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(addr, &data));
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_GFID, _V, _ZERO, data);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(addr, data));

    //
    // During the FECS ucode load, the FECS DMA engine is triggered by ACR and that ilwolves writing the FECS arbiter registers.
    // Although the reset value of ENGINE_ID field for this register (ie LW_PGRAPH_PRI_FECS_ARB_ENGINE_CTL) is BAR2_FN0, the one
    // which we program here, but in the future if ACR is triggers the ucode load after these values are at the non-reset values,
    // then we might run into issues and thus to avoid this we explicitly program them.
    //
    addr = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_ARB_ENGINE_CTL, pFlcnCfg->falconInstance);
    CHECK_FLCN_STATUS(BAR0_REG_RD32_HS_ERRCHK(addr, &data));
    data = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_ENGINE_CTL, _ENGINE_ID, _BAR2_FN0, data);
    CHECK_FLCN_STATUS(BAR0_REG_WR32_HS_ERRCHK(addr, data));

ErrorExit:
    return flcnStatus;
}
