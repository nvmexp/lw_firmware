/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   cmdmgmt_vgpu.c
 * @brief  Provides an interface for configuring LWLink related functions
 *
 * NOTE: The CMDMGMT task is a just a generic task for forwarding commands
 *       to the respective tasks. This feature has no specific
 *       task it fits under so using CMDMGMT as a WAR for the time being.
 *       This feature has only a couple of checks and reg writes in the PMU
 *       so there shouldn't be much impact. But, if further extension is required,
 *       please create a standalone task.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_vgpu_hal.h"
#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Exelwtes generic RM_PMU_RPC_STRUCT_CMDMGMT_VGPU_VF_BAR1_SIZE_UPDATE RPC
 *
 * @param[in]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_VGPU_VF_BAR1_SIZE_UPDATE
 */
FLCN_STATUS
pmuRpcCmdmgmtVgpuVfBar1SizeUpdate
(
    RM_PMU_RPC_STRUCT_CMDMGMT_VGPU_VF_BAR1_SIZE_UPDATE *pParams
)
{
    return vgpuVfBar1SizeUpdate_HAL(NULL,
                                    pParams->vfBar1SizeMB,
                                    &(pParams->numVfs));
}
