/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objdisp.c
 * @brief  Top-level display object for display state and HAL infrastructure.
 *
 * @section _vblankHeadMask VBLANK Signal Head Mask
 *
 * VBLANK interrupts are handled within 'icServiceMiscIO'. To keep track of
 * which head(s) the interrupt originated from, a bit mask is kept in the form
 * of 'headMask'. Each bit represents a head and will be set if a VBLANK was
 * recorded from that head. Heads are enumerated from bit 0 to # of heads - 1.
 * The number of heads can be acquired from 'dispGetNumHeads()'.
 *
 * From 'icServiceMiscIO', 'icServiceMiscIOVblank' sends a VBLANK command
 * signal to the command dispatcher. The command dispatcher then services the
 * signal by calling the HAL 'dispServiceVblank'. Here, the VBLANK is finally
 * sent to any task that requires it. Along this entire path the 'headMask'
 * is passed along.
 *
 * @endsection
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objdisp.h"
#include "pmu_objpg.h"
#include "pmu_disp.h"

#include "config/g_disp_private.h"
#if(DISP_MOCK_FUNCTIONS_GENERATED)
#include "config/g_disp_mock.c"
#endif

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/*!
 *  This bytes acts as 'semaphore' in case of multiple enable/disabled requests.
 */
LwU8    DispVblankControlSemaphore[RM_PMU_DISP_NUMBER_OF_HEADS_MAX] = {0};

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJDISP Disp;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void s_dispOsmPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "s_dispOsmPreInit");
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct the DISP object.  This sets up the HAL interface used by the
 * DISP module.
 *
 * @return  FLCN_OK     On success
 * @return  Others      Error return value from failing child function
 */
FLCN_STATUS
constructDisp(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Create semaphore to manage MSCG enable/disable.
    status = lwrtosSemaphoreCreateBinaryGiven(&Disp.mscgSemaphore, OVL_INDEX_OS_HEAP);
    if (status != FLCN_OK)
    {
        goto constructDisp_exit;
    }

    Disp.mscgDisableReasons = RM_PMU_DISP_MSCG_DISABLE_REASON_INIT;

constructDisp_exit:
    return status;
}

/*!
 * Initialize the DISP object while bootstrapping PMU
 */
void
dispPreInit(void)
{
    LwBool bFuseDispPresent = LW_FALSE;

    dispPreInit_HAL(&Disp, &bFuseDispPresent);

    //
    // RM has provided the disp state via PMU booting argument so that
    // common mutex infra can init the mutex ID mapping at the earliest
    // point. But RM is considered an inselwre component, thus here we need
    // to compare the RM info with the real info read from fuse to ensure RM
    // provided correct info.
    //
    PMU_HALT_COND(DISP_IS_PRESENT() == bFuseDispPresent);

    Disp.numHeads = dispGetNumberOfHeads_HAL(&Disp);
    Disp.numSors  = dispGetNumberOfSors_HAL(&Disp);

    // Initialize the OneShotMode related data structures
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_OSM) &&
        DISP_IS_PRESENT())
    {
        s_dispOsmPreInit();
    }
}

/*!
 * @brief Handle Oneshot state change (Enable/Disable) rpc
 *
 * @param[in]     pParams   Pointer to RM_PMU_RPC_STRUCT_LPWR_ONESHOT
 *
 */
LwBool
dispRpcLpwrOsmStateChange
(
    RM_PMU_RPC_STRUCT_LPWR_ONESHOT *pParams
)
{
    DISP_RM_ONESHOT_CONTEXT *pRmOneshotCtx = DispContext.pRmOneshotCtx;

    LwBool bImmediateOsmResponse = LW_TRUE;

    if (pRmOneshotCtx != NULL)
    {
        // We should not get duplicate commands
        PMU_HALT_COND(pRmOneshotCtx->bRmAllow != pParams->bRmAllow);

        pRmOneshotCtx->bRmAllow = pParams->bRmAllow;

        //
        // Update head state only if RM allows oneshot mode
        //
        // Do not override head settings in case RM has just disabled oneshot
        // mode. Old setting will be used to disable interrupts.
        //
        if (dispRmOneshotStateUpdate_HAL(&Disp))
        {
            //
            // Enable ISO_STUTTER sub feature.
            //
            // There might be a scenario that ISO_STUTTER is already
            // enabled. In such scenario there will be no change in
            // subfeature mask and this API will return a LW_FALSE.
            //
            // In such case, we should not block the RM and we should
            // send the ack immediately.
            //
            // If there is a valid change in subfeature mask, this API will
            // return a LW_TRUE, then ack will be send by SFM API itself.
            //
            if (pgCtrlSubfeatureMaskRequest(RM_PMU_LPWR_CTRL_ID_MS_MSCG,
                                            LPWR_CTRL_SFM_CLIENT_ID_OSM, LW_U32_MAX))
            {
                // Don't send the ACK now, ack will send by subfeature API.
                bImmediateOsmResponse = LW_FALSE;
            }
        }
    }

    return bImmediateOsmResponse;
}

/* ------------------------ Private Functions  ------------------------------ */

/*!
 * Initialize the One shot mode data structure/HW state
 */
static void
s_dispOsmPreInit(void)
{
    // Allocate DMEM space for RM_ONESHOT context
    DispContext.pRmOneshotCtx = lwosCallocResident(sizeof(DISP_RM_ONESHOT_CONTEXT));
    PMU_HALT_COND(DispContext.pRmOneshotCtx != NULL);
}
