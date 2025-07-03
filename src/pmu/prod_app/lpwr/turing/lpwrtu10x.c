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
 * @file   lpwrtu10x.c
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
#include "pmu/pmumailboxid.h"
#include "pmu_objlpwr.h"

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
lpwrFsmDependencyInit_TU10X
(
    LwU8 parentFsmIdx,
    LwU8 childFsmIdx
)
{
    LwU32 regVal = 0;

    // Make sure parent and child ctrl Id are withing the HW limit
    if ((parentFsmIdx >= LW_CPWR_PMU_PG_ON_TRIGGER_MASK__SIZE_1) ||
        (childFsmIdx >= LW_CPWR_PMU_PG_ON_TRIGGER_MASK__SIZE_1))
    {
        PMU_BREAKPOINT();
        return;
    }

    // Make sure parent and child index are not same
    if (parentFsmIdx == childFsmIdx)
    {
        PMU_BREAKPOINT();
        return;
    }

    //
    // Example of Parent child Dependencies:
    //
    // GR coupled MSCS: GR is pre-condtion for MSCG
    //
    // Parent HW ENG IDX: PG_ENG_IDX_ENG_GR or PG_ENG_IDX_ENG_GR_RG
    // Child HW ENG IDX : PG_ENG_IDX_ENG_MS
    //
    // So the final pre-condition logic for MSCG is =
    // (ENG_GR_PG engaged || ENG_GR_RG engaged || GR_PASSIVE engaged) && Other Conditions
    //
    // So we will move the ENG_GR_PG|GR_RG|GR_PASSIVE condition to
    // GROUP_0 and this group will be ANDED with other conditions
    //
    // i.e.
    // Final Condition = GROUP_0 && Other Conditions
    //
    // where GROUP_0 = ENG_GR_PG engaged || ENG_GR_RG engaged || GR_PASSIVE engaged
    //
    // So the current use cases are getting covered by GROUP_0 itself, hence we are
    // not programming other groups.
    //
    switch (parentFsmIdx)
    {
        case PG_ENG_IDX_ENG_0:
        case PG_ENG_IDX_ENG_1:
        case PG_ENG_IDX_ENG_2:
        case PG_ENG_IDX_ENG_3:
        {
            regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK(childFsmIdx));

            regVal = FLD_IDX_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK,
                                     _ENG_OFF_OR, parentFsmIdx, _ENABLE, regVal);
            regVal = FLD_IDX_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK,
                                     _ENG_OFF_GROUP, parentFsmIdx, _0, regVal);
            REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK(childFsmIdx), regVal);

            break;
        }

        case PG_ENG_IDX_ENG_4:
        {
            regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_1(childFsmIdx));

            regVal = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_1, _ENG4_OFF_OR,
                                 _ENABLE, regVal);
            regVal = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_1, _ENG4_OFF_GROUP,
                                 _0, regVal);
            REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_1(childFsmIdx), regVal);

            break;
        }

        default:
        {
            PMU_BREAKPOINT();
        }
    }
}

/* ------------------------ Private Functions  ------------------------------ */
