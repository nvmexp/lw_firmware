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
 * @file    cmdmgmt_detach.c
 * @brief   Contains logic for PMU detach command
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwoswatchdog.h"
#include "lwostask.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_detach.h"
#if (PMUCFG_FEATURE_ENABLED(PMUTASK_ACR))
#include "acr/pmu_objacr.h"
#endif
#include "pmu_objpmu.h"
#include "pmu_objic.h"
#include "task_therm.h"
#if UPROC_RISCV
#include <drivers/drivers.h>
#include <shlib/syscall.h>
#endif // UPROC_RISCV

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Under Construction Variables ------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * Set by RPC to defer DETACH processing after RPC request has been completed.
 */
static LwBool CmdmgmtBDetachReceived = LW_FALSE;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Exelwtes generic DETACH_PMU RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_THERM_DETACH_PMU
 */
FLCN_STATUS
pmuRpcCmdmgmtDetachPmu
(
    RM_PMU_RPC_STRUCT_CMDMGMT_DETACH_PMU *pParams
)
{
#if (PMUCFG_FEATURE_ENABLED(PMUTASK_ACR))
        // Reset the CBC_BASE register since the PMU is being detached.
        acrResetCbcBase_HAL(&Acr);
#endif // PMUCFG_FEATURE_ENABLED(PMUTASK_ACR)

    // On RISCV priv level is lowered inside appShutdown
#ifdef UPROC_FALCON
    if (PMUCFG_FEATURE_ENABLED(PMU_LOWER_PMU_PRIV_LEVEL))
    {
        // Lower the priv level of secure reset so that RM can reset PMU.
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPrivSec)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            pmuLowerPrivLevelMasks_HAL(&Pmu);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
#endif // UPROC_FALCON

    if (OSTASK_ENABLED(THERM) &&
        PMUCFG_FEATURE_ENABLED(PMU_THERM_DETACH_REQUEST))
    {
        // Notify the THERM task of further DETACH processing.
        DISP2THERM_CMD disp2Therm = {{ 0 }};

        disp2Therm.therm.hdr.unitId = RM_PMU_UNIT_THERM;
        disp2Therm.therm.hdr.eventType = THERM_DETACH_REQUEST;

        (void)lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, THERM), &disp2Therm,
                                        sizeof(disp2Therm));

        pParams->bWaitForDetachResponse = LW_TRUE;
    }
    else
    {
        CmdmgmtBDetachReceived = LW_TRUE;

        pParams->bWaitForDetachResponse = LW_FALSE;
    }

    return FLCN_OK;
}

/*!
 * @brief   Deferred DETACH processing.
 */
void
cmdmgmtDetachDeferredProcessing(void)
{
    if (CmdmgmtBDetachReceived)
    {
        CmdmgmtBDetachReceived = LW_FALSE;

        //
        // When THERM is disabled, the whole detach process is not required.
        // DMA must be suspended while in detached state so ignore DMA lock.
        //
        (void)lwosDmaSuspend(LW_TRUE);

#if UPROC_RISCV
        // Shutdown PMU - No return expected
        sysShutdown();
#else
        if (PMUCFG_FEATURE_ENABLED(PMU_HALT_ON_DETACH))
        {
            //
            // PMU will get reset after it unloads. If we don't halt, PMU
            // tasks can continue to access external interfaces like PRI. If
            // the PMU is reset while a PRI transaction is in progress, all the
            // credits on that interface won't have been returned, and that is
            // an unsupported reset from HW perspective. It will cause asserts
            // on fmodel/RTL and can cause the chip to go into bad state on
            // silicon/emulation, and can only be recovered by a fullchip reset
            //
            icHaltIntrDisable_HAL();
            PMU_HALT();
        }
#endif // UPROC_RISCV
    }
}
