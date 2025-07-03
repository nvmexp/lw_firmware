/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   perf_mode.c
 * @brief  Function definitions for the PERF MODE SW module.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "dbgprintf.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "cmdmgmt/cmdmgmt.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief PERF RPC interface for performance modes control
 * @memberof PERF_MODE
 * @public
 *
 * @param[out]  pParams  RM_PMU_RPC_STRUCT_PERF_PERF_MODES_CONTROL pointer
 *
 * @return FLCN_OK if the the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcPerfPerfModesControl
(
    RM_PMU_RPC_STRUCT_PERF_PERF_MODES_CONTROL *pParams
)
{
    LwU8        idx;
    FLCN_STATUS status      = FLCN_OK;
    LwBool      bTriggerIlw = LW_FALSE;
    OSTASK_OVL_DESC vfeOvlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(vfeOvlDescList);

    FOR_EACH_INDEX_IN_MASK(32, idx, pParams->data.mask)
    {
        RM_PMU_PERF_VFE_VAR_VALUE varValue = {0};

        // Only Performance Mode Id 0 is supported.
        if (idx >= 1U)
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            goto pmuRpcPerfPerfModesControl_exit;
        }

        // Build globally specified vfe variable.
        varValue.varType                    =
            LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_GLOBALLY_SPECIFIED;
        varValue.globallySpecified.uniqueId =
            LW2080_CTRL_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_UID_PERFORMANCE_MODE;
        varValue.globallySpecified.varValue = (pParams->data.modes[idx].bEngaged ? 1 : 0);

        // Program globally specified vfe variable.
        status = vfeVarSingleGloballySpecifiedValueSet(&varValue, 1U, &bTriggerIlw);
        if (status != FLCN_OK)
        {
            goto pmuRpcPerfPerfModesControl_exit;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (bTriggerIlw)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeIlwalidate(),
            pmuRpcPerfPerfModesControl_exit);
    }

pmuRpcPerfPerfModesControl_exit:
OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(vfeOvlDescList);
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- End of File ------------------------------------ */
