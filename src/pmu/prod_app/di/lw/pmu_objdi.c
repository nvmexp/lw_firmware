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
 * @file   pmu_objdi.c
 * @brief  Deep Idle
 *
 * It handles PG_ENG_CTRL based Deep Idle and Common Deep Idle HW entry/exit
 * sequence.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_objdi.h"
#include "pmu_objpg.h"
#include "pmu_objdisp.h"
#include "dbgprintf.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

#include "config/g_di_private.h"

/* ------------------------ Application includes --------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
OBJDI Di;

/* ------------------------ Prototypes --------------------------------------*/
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * @brief Initialize OBJDI while bootstrapping PMU
 */
void
diPreInit(void)
{
    // All members are by default initialize to Zero.
    memset(&Di, 0, sizeof(Di));

    if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
    {
        // Allocate DI cache
        Di.pCache = lwosCallocResident(sizeof(DI_SEQ_CACHE));
        PMU_HALT_COND(Di.pCache != NULL);

        // Initialize list of clocks running from XTAL
        diSeqListClkRoutedToXtalGet_HAL(&Di, &Di.pClkRoutedToXtal,
                                        &Di.clkRoutedToXtalListSize);

        // Init PLL Caches
        diSeqPllListInitCorePll_HAL(&Di, &Di.pPllsCore,    &Di.pllCountCore);
        diSeqPllListInitXtal4x_HAL (&Di, &Di.pPllsXtal4x,  &Di.pllCountXtal4x);
        diSeqPllListInitRefmpll_HAL(&Di, &Di.pPllsRefmpll, &Di.pllCountRefmpll);
        diSeqPllListInitDrampll_HAL(&Di, &Di.pPllsDrampll, &Di.pllCountDrampll);
        diSeqPllListInitSpplls_HAL (&Di, &Di.pPllsSppll,   &Di.pllCountSppll);

        // Check whether the fuse related to FSM control of XTAL4XPLL is blown or not
        Di.bXtal4xFuseBlown = diXtal4xFuseIsBlown_HAL();
    }
}

/*!
 * @brief Send GPU_RDY/ENTRY_ABORT event to RM
 *
 * This function sends GPU_RDY on successful completion on DI cycles and
 * ENTRY_ABORT on abort.
 *
 * @param[in]   bSuccessfulEntry    LW_TRUE  when DI is engaged
 *                                  LW_FALSE when DI is aborted
 * @param[in]   abortReasonMaskSm   State machine specific abort mask
 */
void
diSeqGpuReadyEventSend
(
    LwBool bSuccessfulEntry,
    LwU32  abortReasonMaskSm
)
{
    PMU_RM_RPC_STRUCT_GCX_DIOS_GPU_RDY  rpc;
    DI_SEQ_CACHE                       *pCache = DI_SEQ_GET_CAHCE();
    FLCN_STATUS                         status;

    memset(&rpc, 0x00, sizeof(rpc));

    if (bSuccessfulEntry)
    {
        rpc.bAbort = LW_FALSE;
    }
    else
    {
        rpc.bAbort = LW_TRUE;
    }

    // Send the abort reason and PEX sleep state for both cases.
    rpc.abortReasonMaskCommon = Di.abortReasonMask;
    rpc.abortReasonMaskSm     = abortReasonMaskSm;
    rpc.pexSleepState         = Di.pexSleepState;
    rpc.intrStatus            = pCache->intrStatus;

    // Send the Event as an RPC.  RC-TODO Properly handle status here.
    PMU_RM_RPC_EXELWTE_BLOCKING(status, GCX, DIOS_GPU_RDY, &rpc);
}

/* ------------------------ Local Functions  ------------------------------- */

