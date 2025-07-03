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
 * @file   pmu_msgkxxxgpxxx.c
 * @brief  Kepler to Pascal specific MS object file
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_ltc.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objms.h"
#include "dbgprintf.h"

#include "config/g_ms_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief Disable L2 Cache auto flush/clean functionality if its enabled
 */
void
msDisableL2AutoFlush_GMXXX(void)
{
    LwU32 val;

    Ms.tstgAutoclean = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_AUTOCLEAN);
    val = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_TSTG_AUTOCLEAN,
                      _TIMEOUT, _DISABLED, Ms.tstgAutoclean);
    if (Ms.tstgAutoclean != val)
    {
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_AUTOCLEAN, val);
    }

    Ms.cbcAutoclean = REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_CBC_AUTOCLEAN);
    val = FLD_SET_DRF(_PLTCG, _LTCS_LTSS_CBC_AUTOCLEAN,
                      _TIMEOUT, _DISABLED, Ms.cbcAutoclean);
    if (Ms.cbcAutoclean != val)
    {
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_CBC_AUTOCLEAN, val);

        // Flush write before polling LTC and FBPA status
        REG_RD32(FECS, LW_PLTCG_LTCS_LTSS_CBC_AUTOCLEAN);
    }
}

/*!
 * @brief Restore setting of L2 Cache auto flush/clean.
 */
void
msRestoreL2AutoFlush_GMXXX(void)
{
    if (!FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_TSTG_AUTOCLEAN,
                      _TIMEOUT, _DISABLED, Ms.tstgAutoclean))
    {
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_TSTG_AUTOCLEAN, Ms.tstgAutoclean);
    }

    if (!FLD_TEST_DRF(_PLTCG, _LTCS_LTSS_CBC_AUTOCLEAN,
                      _TIMEOUT, _DISABLED, Ms.cbcAutoclean))
    {
        REG_WR32(FECS, LW_PLTCG_LTCS_LTSS_CBC_AUTOCLEAN, Ms.cbcAutoclean);
    }
}
