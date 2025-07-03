/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgaxxx.c
 * @brief  HAL-interface for the GA10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_smcarb.h"
#include "dev_ltc.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objfifo.h"
#include "pmu_objic.h"

#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Interface to enable/disable SMCARB free running timestamp
 *
 * SMCARB sends periodic data to CXBAR_GXC unit which will cause the
 * XBAR to report non idle and MSCG will keep on aborting.
 * So we need to disable/enable it during MSCG
 *
 * TODO-pankumar: We can define a unify infra so that both GPC-RG and MSCG
 * can use single function for disable/enable the SMCARB time stamp.
 *
 * @param[in] bDisable   boolean to specify enablement or disablement
 */
void
msSmcarbTimestampDisable_GA10X
(
    LwBool bDisable
)
{
    LwU32 regVal;

    if (bDisable)
    {
        // Read and cache the smcarb timestamp value
        Ms.smcarbTimestamp = regVal = REG_RD32(FECS, LW_PSMCARB_TIMESTAMP_CTRL);

        // Disable free running timestamp
        regVal = FLD_SET_DRF(_PSMCARB, _TIMESTAMP_CTRL, _DISABLE_TICK, _TRUE, regVal);

        // Write the disabled value
        REG_WR32(FECS, LW_PSMCARB_TIMESTAMP_CTRL, regVal);
    }
    else
    {
        // Restore the cached value
        REG_WR32(FECS, LW_PSMCARB_TIMESTAMP_CTRL, Ms.smcarbTimestamp);
    }

    // Force read to flush the write
    REG_RD32(FECS, LW_PSMCARB_TIMESTAMP_CTRL);
}

/*!
 * @brief Disable L2 Cache auto flush/clean functionality if its enabled
 */
void
msDisableL2AutoFlush_GA10X(void)
{
    LwU32 val;

    Ms.tstgAutoclean = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_AUTOCLEAN);
    val = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_AUTOCLEAN,
                      _TIMEOUT, _DISABLED, Ms.tstgAutoclean);
    if (Ms.tstgAutoclean != val)
    {
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_AUTOCLEAN, val);
    }
}

/*!
 * @brief Restore setting of L2 Cache auto flush/clean.
 */
void
msRestoreL2AutoFlush_GA10X(void)
{
    if (!FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_TSTG_AUTOCLEAN,
                      _TIMEOUT, _DISABLED, Ms.tstgAutoclean))
    {
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_AUTOCLEAN, Ms.tstgAutoclean);
    }
}
