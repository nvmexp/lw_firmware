/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_acr.c
 * @brief  PMU ACR Task
 *
 * ACR test takes cares of access control regions
 *
 * <more detailed description to follow>
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"
#include "unit_api.h"
#include "lwostask.h"
#include "pmu_objpmu.h"
#include "dbgprintf.h"
#include "flcnifcmn.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "lib_lw64.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
#include "acr.h"
#include "g_acrlib_hal.h"
#include "acr_objacrlib.h"
#else // (PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
#include "acr/pmu_acr.h"
#endif // (PMUCFG_FEATURE_ENABLED(PMU_ACR_TEGRA_PROFILE))
#include "acr/task_acr.h"
#include "acr/pmu_objacr.h"
#include "rmlsfm.h"
#include "acr_status_codes.h"
#include "config/g_acr_private.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

lwrtosTASK_FUNCTION(task_acr, pvParameters);

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the ACR Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
acrPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_CREATE(status, PMU, ACR);
    if (status != FLCN_OK)
    {
        goto acrPreInitTask_exit;
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, ACR), 4, sizeof(DISPATCH_ACR));
    if (status != FLCN_OK)
    {
        goto acrPreInitTask_exit;
    }

acrPreInitTask_exit:
    return status;
}
/*!
 * Entry point for the ACR task. This task does not receive any parameters.
 */
lwrtosTASK_FUNCTION(task_acr, pvParameters)
{
    DISPATCH_ACR dispatch;

    LWOS_TASK_LOOP_NO_CALLBACKS(LWOS_QUEUE(PMU, ACR), &dispatch, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;

        switch (dispatch.hdr.unitId)
        {
            case RM_PMU_UNIT_ACR:
            {
                status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

                switch (dispatch.hdr.eventType)
                {
                    case DISP2UNIT_EVT_RM_RPC:
                    {
                        status = pmuRpcProcessUnitAcr(&(dispatch.rpc));
                        break;
                    }
                }
                break;
            }
        }
    }
    LWOS_TASK_LOOP_END(status);
}

FLCN_STATUS
pmuRpcAcrBootstrapFalcon
(
    RM_PMU_RPC_STRUCT_ACR_BOOTSTRAP_FALCON *pParams
)
{
    return acrBootstrapFalcon(pParams->falconId, pParams->bReset, LW_FALSE, NULL);
}

/*!
 * Initiazes WPR region details. WPR is a client of ACR and we can have multiple
 * clients. WPR region is used to store the details of LS falcon and after PMU
 * Boot, we need to tell PMU ACR task about the start address, WPR region ID,
 * and WPR offset.
 */
FLCN_STATUS
pmuRpcAcrInitWprRegion
(
    RM_PMU_RPC_STRUCT_ACR_INIT_WPR_REGION *pParams
)
{
    return acrInitWprRegion(pParams->wprRegionId, pParams->wprOffset);
}

/*!
 * Falcon's UC is selwred in ACR region and only LS clients like the PMU can
 * access it. During resetting an engine, RM needs to contact PMU to reset and
 * boot strap GR falcons.
 */
FLCN_STATUS
pmuRpcAcrBootstrapGrFalcons
(
    RM_PMU_RPC_STRUCT_ACR_BOOTSTRAP_GR_FALCONS *pParams
)
{
    return acrBootstrapGrFalcons(pParams->falconIdMask, pParams->bReset,
                                 pParams->falcolwAMask, pParams->wprBaseVirtual);
}

/*!
 * CBC base register is priv protected so RM needs to send the CBC base address
 * to PMU to write it. We will sanity check the CBC base address against VPR
 * range before writing it to the chip.
 */
FLCN_STATUS
pmuRpcAcrWriteCbcBase
(
    RM_PMU_RPC_STRUCT_ACR_WRITE_CBC_BASE *pParams
)
{
    return acrWriteCbcBase_HAL(&Acr, pParams->cbcBase);
}
