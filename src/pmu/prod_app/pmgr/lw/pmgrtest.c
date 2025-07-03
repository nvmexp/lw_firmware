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
 * @file    pmgrtest.c
 * @brief   PMU_PMGR_RPC_TEST_EXELWTE related code
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ OpenRTOS Includes ------------------------------ */
/* ------------------------ Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_selwrity.h"
#include "pmu_objpmgr.h"

#include "config/g_pmgr_hal.h"
#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Execute Pmgr Tests
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE
 */
FLCN_STATUS
pmuRpcPmgrTestExelwte
(
    RM_PMU_RPC_STRUCT_PMGR_TEST_EXELWTE *pParams
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, libPmgrTest)
    };

    pParams->outStatus = LW2080_CTRL_PMGR_GENERIC_TEST_NOT_IMPLEMENTED;
    pParams->outData   = 0;

    if (FLD_TEST_DRF(_VSB, _REG0, _RM_TEST_EXELWTE, _NO, PmuVSBCache))
    {
       pParams->outStatus =
           LW2080_CTRL_PMGR_GENERIC_TEST_INSUFFICIENT_PRIVILEGE;
       goto pmuRpcPmgrTestExelwte_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        switch (pParams->index)
        {
            case LW2080_CTRL_PMGR_GENERIC_TEST_ID_ADC_INIT:
            {
                status = pmgrTestAdcInit_HAL(&Pmgr, pParams);
                break;
            }
            case LW2080_CTRL_PMGR_GENERIC_TEST_ID_ADC_CHECK:
            {
                status = pmgrTestAdcCheck_HAL(&Pmgr, pParams);
                break;
            }
            case LW2080_CTRL_PMGR_GENERIC_TEST_ID_HI_OFFSET_MAX_CHECK:
            {
                status = pmgrTestHiOffsetMaxCheck_HAL(&Pmgr, pParams);
                break;
            }
            case LW2080_CTRL_PMGR_GENERIC_TEST_ID_IPC_PARAMS_CHECK:
            {
                status = pmgrTestIPCParamsCheck_HAL(&Pmgr, pParams);
                break;
            }
            case LW2080_CTRL_PMGR_GENERIC_TEST_ID_BEACON_CHECK:
            {
                status = pmgrTestBeaconCheck_HAL(&Pmgr, pParams);
                break;
            }
            case LW2080_CTRL_PMGR_GENERIC_TEST_ID_OFFSET_CHECK:
            {
                status = pmgrTestOffsetCheck_HAL(&Pmgr, pParams);
                break;
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pmuRpcPmgrTestExelwte_exit:
    return status;
}
