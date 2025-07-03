/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_digpxxx_only.c
 * @brief  Pascal specific DI object file
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_trim.h"
#include "dev_pwr_csb.h"
#include "dev_fuse.h"
/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objdi.h"
#include "dbgprintf.h"

#include "config/g_di_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * @brief PEX sleep state engaged in current use-case
 *
 * There are total 3 PEX sleep states
 * 1) DeepL1
 * 2) L1.1
 * 3) L1.2
 * GPU should be in at least one PEX sleep state to engage DI.
 *
 * Details on HW implementation:
 * PG_STAT_SPECIFIC is sticky register. It updates on PG_ENG reset and on PG_ON.
 * It will not change after raising PG_ON interrupt. PG_OVERRIDE doesn't directly
 * affects PG_STAT_SPECIFIC.
 *
 * There is special case where PG_ENG will not check for PEX sleep states if ALL
 * PEX sleep states (i.e. DeepL1, L1.1 and L1.2) are override through PG_OVERRIDE.
 * This functionality was added to remove PEX from PG_ENG(DI) entry equation.
 * Ignoring all PEX sleep states is not POR for GP10X. Thus, as per POR, PG_ENG(DI)
 * will raise PG_ON interrupt only when one of PEX sleep state is not override in
 * PG_OVERRIDE and same PEX state got engaged.
 *
 * @param[out]  pPexSleepState  PEX sleep state - DIDLE_SUBTYPE_xyz
 *
 * @return  PEX sleep state - DI_PEX_SLEEP_STATE_xyz
 */
LwU32
diPgEngPexSleepStateGet_GP10X(void)
{
    LwU32 pgStat;

    // All PEX states are disabled. Return early.
    if (!PG_IS_SF_ENABLED(Di, DI, DEEP_L1) &&
        !PG_IS_SF_ENABLED(Di, DI, L1SS))
    {
        DI_PG_ENG_ABORT_REASON_UPDATE_NAME(PEX_SLEEP_STATE);
        return DI_PEX_SLEEP_STATE_NONE;
    }

    pgStat = REG_RD32(CSB, LW_CPWR_PMU_PG_STAT_SPECIFIC);

    if (FLD_TEST_DRF(_CPWR_PMU, _PG_STAT_SPECIFIC,
                     _DEEPL1_ENGAGED, _TRUE, pgStat))
    {
        PMU_HALT_COND(PG_IS_SF_ENABLED(Di, DI, DEEP_L1));
        return DI_PEX_SLEEP_STATE_DEEP_L1;
    }

    if (FLD_TEST_DRF(_CPWR_PMU, _PG_STAT_SPECIFIC,
                     _L1_1_ENGAGED, _TRUE, pgStat))
    {
        PMU_HALT_COND(PG_IS_SF_ENABLED(Di, DI, L1SS));
        return DI_PEX_SLEEP_STATE_L1_1;
    }

    if (FLD_TEST_DRF(_CPWR_PMU, _PG_STAT_SPECIFIC,
                     _L1_2_ENGAGED, _TRUE, pgStat))
    {
        PMU_HALT_COND(PG_IS_SF_ENABLED(Di, DI, L1SS));
        return DI_PEX_SLEEP_STATE_L1_2;
    }

    // We cant be here.
    PMU_HALT();

    return DI_PEX_SLEEP_STATE_ILWALID;
}

/*!
 * @Brief Returns wheather the fuse related to FSM control of XTAL4XPLL is blown or not
 *
 * @return LW_TRUE  -> If XTAL4XPLL fuse is blown
 *         LW_FALSE -> If XTAL4XPLL fuse is not blown
 */
LwBool
diXtal4xFuseIsBlown_GP10X()
{
    LwU32 regVal = REG_RD32(FECS, LW_FUSE_OPT_XTAL16X_AUTO_ENABLE);

    return (FLD_TEST_DRF(_FUSE_OPT, _XTAL16X_AUTO_ENABLE, _DATA, _YES, regVal));
}

