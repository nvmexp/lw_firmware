/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    therm_detach.c
 * @brief   Interfaces to execute PMU therm detach logic
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_oslayer.h"
#include "therm/objtherm.h"
#include "fan/objfan.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   THERM specific PMU DETACH processing.
 */
void
thermDetachProcess(void)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_DETACH_PMU_COMPLETED rpc;
    FLCN_STATUS     status;

    //
    // Manually acquire the message queue mutex to ensure that its not held by
    // any task that may become DMA suspended. Required to ensure that this
    // task will be able to send the detach acknowledgement to the RM (below).
    //
    osCmdmgmtRmQueueMutexAcquire();

    //
    // Since we need to suspend all DMA traffic, we forcefully
    // preload all necessary lib overlays and mark as detached.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_2X_AND_LATER) &&
        Fan.bFan2xAndLaterEnabled)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_FAN_CTRL_COMMON_BASE
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, thermLibFanCommon)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwm)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        Fan.bTaskDetached = LW_TRUE;
    }

    // Elevate our priority.
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY_DMA_SUSPENDED);

    //
    // Make sure the PMU hardware (interrupts for instance)
    // is configured properly for detached mode operation.
    //
    pmuDetach_HAL(&Pmu);

    //
    // Inform the RM that the PMU is now detached.
    //
    // Do not use: osCmdmgmtRmQueueMutexReleaseAndPost(&hdr, NULL);
    // instead of the subsequent two lines since quoted macro uses
    // a critical section that would mess up interrupt enablement
    // state set by the pmuDetach_HAL() few lines above.
    //
    lwrtosSemaphoreGive(OsCmdmgmtQueueDescriptorRM.mutex);
    PMU_RM_RPC_EXELWTE_NON_BLOCKING(status, CMDMGMT, DETACH_PMU_COMPLETED, &rpc);

    // Suspend DMA by ignoring DMA lock.  Assert if DMA suspension failed.
    (void)lwosDmaSuspend(LW_TRUE);
}
