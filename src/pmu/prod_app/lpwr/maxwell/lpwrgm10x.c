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
 * @file   lpwrgm10x.c
 * @brief  PMU Lowpower Object
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "hwproject.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Manage the dependency between two FSMs
 *
 * Parent FSM should be preconditon for Child FSM
 *
 * Child FSM PG_ON interrupt can only come if Parent FSM
 * is in PWR_OFF State.
 *
 * @param[in]   parentFsmIdx  - HW CtrlId for Parent FSM
 * @param[in]   childFsmIdx   - HW CtrlId for child FSM
 *
 * TODO-pankumar: We need to colwert this API to accept
 * the SW CtrlId
 *
 */
void
lpwrFsmDependencyInit_GM10X
(
    LwU8 parentFsmIdx,
    LwU8 childFsmIdx
)
{
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       regVal   = 0;

    //
    // From Maxwell to Pascal, we only have dependencies between GR and MSCG feature.
    // By Default in HW, GR is pre-condition for MSCG. So here if needed, we have to
    // decouple them. This logic is changed from Turing and later GPUs.
    //
    // Check for GR-ELPG override
    //
    if (IS_GR_DECOUPLED_MS_SUPPORTED(pMsState))
    {
        regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_OVERRIDE);
        regVal = FLD_SET_DRF(_CPWR_PMU, _PG_OVERRIDE, _GR, _ENABLE, regVal);
        REG_WR32(CSB, LW_CPWR_PMU_PG_OVERRIDE, regVal);
    }
}

/* ------------------------ Private Functions  ------------------------------ */
