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
 * @file   cmdmgmt_lwlink.c
 * @brief  Provides an interface for configuring LWLink related functions
 *
 * NOTE: The cmdmgmt task is a just a generic task for forwarding commands
 *       to the respective tasks. LWLink and Hshub do not have any related
 *       tasks in Pascal and Volta. Hence, the Hshub registers are updated
 *       through the cmdmgmt task. This practice however should be avoided.
 *       On Turing, the BIF task handles the Hshub register updates.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objlwlink.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Exelwtes generic CMDMGMT_HSHUB_UPDATE RPC
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_HSHUB_UPDATE
 */
FLCN_STATUS
pmuRpcCmdmgmtHshubUpdate
(
    RM_PMU_RPC_STRUCT_CMDMGMT_HSHUB_UPDATE *pParams
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_LWLINK_HSHUB))
    {
        status = lwlinkHshubUpdate_HAL(&Lwlink,
                                       pParams->hshubConfig0,
                                       pParams->hshubConfig1,
                                       pParams->hshubConfig2,
                                       pParams->hshubConfig6);
    }

    return status;
}
