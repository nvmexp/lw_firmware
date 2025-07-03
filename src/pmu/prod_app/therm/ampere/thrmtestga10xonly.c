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
 * @file    thrmtestga10xonly.c
 * @brief   PMU HAL functions related to therm tests for GA10X Only.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_therm.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"

#include "config/g_therm_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_GPC_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_INDEX_MAX + 1)
#define LW_BJT_MAX          (LW_THERM_GPC_TSENSE_INDEX_GPC_BJT_INDEX_MAX + 1)
#define LW_LWL_MAX          (LW_THERM_LWL_TSENSE_INDEX_INDEX_MAX + 1)

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Programs the GPC index in the GPC_TSENSE_CONFIG_COPY_INDEX register.
 *         Read and write Increment will be disabled.
 *
 *         It should be re-triggered when any of following configuration
 *         registers are changed:
 *            - GPC_TSENSE
 *            - GPC_TS_ADJ
 *            - GPC_RAWCODE_OVERRIDE
 *            - GPC_TS_AUTO_CONTROL
 *
 * @param[in]   idxGpc      GPC index to select.
 */
void
thermTestGpcTsenseConfigCopyIndexSet_GA10X
(
    LwU8    idxGpc
)
{
    LwU32 reg32 = 0;

    // Set GPC Index for GPC_INDEX register.
    reg32 = FLD_SET_DRF_NUM(_CPWR_THERM, _GPC_TSENSE_CONFIG_COPY_INDEX, _INDEX,
                                idxGpc, reg32);

    // Disable Read Increment.
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _READINCR, _DISABLED, reg32);
    reg32 = FLD_SET_DRF(_CPWR_THERM, _GPC_INDEX, _WRITEINCR, _DISABLED, reg32);

    REG_WR32(CSB, LW_CPWR_THERM_GPC_TSENSE_CONFIG_COPY_INDEX, reg32);
}

/*!
 * @brief  Get mask of BJTs that are enabled for GPC. 1: enabled, 0: disabled.
 *
 * @param[in]   idxGpc  GPC TSENSE index.
 *
 * @return      Enabled BJT-s mask. Returns BJT mask for all GPCs for idxGpc LW_U8_MAX.
 */
LwU32
thermTestBjtEnMaskForGpcGet_GA10X
(
    LwU8 idxGpc
)
{
    LwU32 bjtMask;
    LwU32 maxBjtMask;

    //
    // BJT Mask Fuse value interpretation -
    // b0: GPC0BJT0, b1: GPC0BJT1, b2: GPC0BJT2;
    // b3: GPC1BJT0, b4: GPC1BJT1, b5: GPC0BJT2; similarly it continues.
    //
    bjtMask = DRF_VAL(_FUSE_OPT, _TS_DIS_MASK, _DATA, fuseRead(LW_FUSE_OPT_TS_DIS_MASK));
    maxBjtMask = ((1 << LW_BJT_MAX) - 1);

    return ((idxGpc == LW_U8_MAX) ? (~bjtMask) : (~(bjtMask >> (idxGpc * LW_BJT_MAX)) & maxBjtMask));
}
