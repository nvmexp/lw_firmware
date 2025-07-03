/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cgtu10x.c
 * @brief  Lpwr Clock Gating related functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fuse.h"
#include "dev_fbpa.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objfuse.h"
#include "pmu_objlpwr.h"
#include "dev_trim.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @Brief : Gate IST clock for non-automotive SKUs.
 *
 * @param[in] - bIstCgSupport - LW_TRUE  -> Gate the IST clock.
 *                            - LW_FALSE -> Ungate the IST clock.
 *
 * @return  FLCN_OK     On success.
 */
FLCN_STATUS
lpwrCgIstPostInit_TU10X(LwBool bIstCgSupport)
{
    LwU32 regval = 0;

    if (bIstCgSupport)
    {
        regval = fuseRead(LW_FUSE_OPT_IST_DISABLE);

        // Check to identify the SKU as non-automotive.
        if (FLD_TEST_DRF(_FUSE, _OPT_IST, _DISABLE_DATA, _YES, regval))
        {
            regval = REG_RD32(FECS, LW_PTRIM_SYS_IST_CTRL);
            // Gate the IST clock.
            regval = FLD_SET_DRF(_PTRIM, _SYS_IST_CTRL, _IST_CLK_ROOT_CG_EN, _NO, regval);
            REG_WR32(FECS, LW_PTRIM_SYS_IST_CTRL, regval);
        }
    }

    return FLCN_OK;
}

/*!
 * @Brief : Disable the SLCG on FBPA Domain
 *
 * Details are present on the Bug: 2316162
 */
void
lpwrCgFbpaSlcgDisable_TU10X(void)
{
    // Disable FBPA-SLCG
    REG_WR32(BAR0, LW_PFB_FBPA_CG1, LW_PFB_FBPA_CG1_SLCG_DISABLED);
}
