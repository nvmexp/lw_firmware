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
 * @file   lpwrga10x.c
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
#include "pmu_objfifo.h"
#include "pmu_bar0.h"
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
 * @brief Function to map the engine to Esched Idle signal
 *
 * @param[in]     PMU engine Id
 * @param[in/out] pIdleMask pointer to idleMask array
 */
void
lpwrEngineToEschedIdleMask_AD10X
(
    LwU8   pmuEngId,
    LwU32 *pIdleMask
)
{
    // Get the RunlistId based on PMU Eng Id
    LwU8 runlistId = GET_ENG_RUNLIST(pmuEngId);

    //
    // For reference we can find the mapping of runlist and engine here:
    // https://sc-refmanuals.lwpu.com/~gpu_refmanuals/ad102/dev_top.ref#1173
    //
    switch (runlistId)
    {
        case 0:
        {
            pIdleMask[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_ESCHED0,
                                       _ENABLED, pIdleMask[0]);
            break;
        }
        case 1:
        {
            pIdleMask[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_ESCHED1,
                                       _ENABLED, pIdleMask[0]);
             break;
        }
        case 2:
        {
            pIdleMask[0] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _HOST_ESCHED2,
                                       _ENABLED, pIdleMask[0]);
            break;
        }
        case 3:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED3,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 4:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED4,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 5:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED5,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 6:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED6,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 7:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED7,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 8:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED8,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 9:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED9,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 10:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED10,
                                       _ENABLED, pIdleMask[1]);
             break;
        }
        case 11:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED11,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 12:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED12,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 13:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED13,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 14:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED15,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 15:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED15,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        case 16:
        {
            pIdleMask[1] = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _HOST_ESCHED16,
                                       _ENABLED, pIdleMask[1]);
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }
}

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
 */
void
lpwrFsmDependencyInit_AD10X
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

        case PG_ENG_IDX_ENG_5:
        {
            regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(childFsmIdx));

            regVal = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_2,
                                 _ENG5_OFF_OR, _ENABLE, regVal);
            regVal = FLD_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_2,
                                 _ENG5_OFF_GROUP, _0, regVal);
            REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_2(childFsmIdx), regVal);

            break;
        }

        case PG_ENG_IDX_ENG_6:
        case PG_ENG_IDX_ENG_7:
        case PG_ENG_IDX_ENG_8:
        case PG_ENG_IDX_ENG_9:
        {
            regVal = REG_RD32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_3(childFsmIdx));

            regVal = FLD_IDX_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_3,
                                     _ENG_OFF_OR, parentFsmIdx, _ENABLE, regVal);
            regVal = FLD_IDX_SET_DRF(_CPWR_PMU, _PG_ON_TRIGGER_MASK_3,
                                     _ENG_OFF_GROUP, parentFsmIdx, _0, regVal);
            REG_WR32(CSB, LW_CPWR_PMU_PG_ON_TRIGGER_MASK_3(childFsmIdx), regVal);
            break;
        }

        default:
        {
            PMU_BREAKPOINT();
        }
    }
}

/* ------------------------ Private Functions  ------------------------------ */
