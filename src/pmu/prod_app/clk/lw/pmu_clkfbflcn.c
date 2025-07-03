/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkfbflcn.c
 * @brief  FB falcon related code
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "pmu_fbflcn_if.h"

/* ------------------------- Application Includes --------------------------- */
#include "clk/pmu_clkfbflcn.h"
#include "task_perf_daemon.h"
#include "perf/perf_daemon_fbflcn.h"
#include "pmu_objdisp.h"

#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#if PMUCFG_FEATURE_ENABLED(PMU_DISP_MSCG_LOW_FPS_WAR_200637376)
//
// The worst case mclk switch time is:
// 43478 us (frame delay for LWRS_FRAME_TIME setting to take effect) +
// 43478 us (frame delay for ok_to_switch to assert for the first mclk switch) +
// 43478 us (frame delay for ok_to_switch to assert for the second mclk switch)
// = ~140 us (rounded up with some margin)
// "43478 us" is callwlated from a lowest refresh rate of 23 Hz.
// The FB falcon may actually execute two back-to-back mclk switches in order to
// accommodate FBVDD DVS, so we allocate time for both switches in the above
// equation.
// Mclk switch also has FB stop time, but it is only few hundred microseconds, so
// it is ignored here (covered by the margin we are adding).
// The equation above callwlates the maximum time an mclk switch sent to the FB
// falcon is expected to take. However, the FB falcon uses a timeout value of
// 91 us for each of the two FBVDD DVS mclk switches, for a maximum possible
// timeout of 182 us. The PMU will halt if it times out waiting for the FB falcon
// to complete an mclk switch, so to reduce the chance of a PMU halt in a
// production environment, we set the PMU timeout larger than the total FB falcon
// timeout.
//

#define PMU_MCLK_WAIT_FOR_FBFLCN_US 185000
#else
//
// The worst case wait for vblank/ok-to-switch signal would that be of a frame
// time (23Hz is 43478us + 2ms for VBLANK and margin = 45478us). This would make
// the MCLK switch timeout to be: 45.478 * 2 ms (ok-to-switch timeout duration) +
// 221 us * 2 (FB stop time) + some buffer ~=  95 ms.
// We're considering twice of ok-to-switch timeout and FB stop time to accommodate
// for FBVDD DVS.
//
#define PMU_MCLK_WAIT_FOR_FBFLCN_US 95000
#endif

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
static FLCN_STATUS s_clkFbflcnMClkRpcExec(LwU32 targetFreqKHz, LwU32 flags, LwU32 *pFbStopTimeUs, FLCN_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_perfDaemon", "s_clkFbflcnMClkRpcExec");

/* ------------------------- Default Text Section --------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief       Trigger MCLK switch on FB falcon
 *
 * @details     On GV100, RM evaluates the pre/post MCLK switch display settings
 *              needed and bundles the steps in the form of sequencer scripts.
 *              These scripts are sent as a part of the MCLK switch request
 *              command that RM sends to PMU. This function handles this command
 *              from RM by exelwting the pre and post display scripts and triggering
 *              MCLK switch on FB falcon.
 *
 * @param[in]   pMClkSwitchRequest   Structure describing the target frequency, flags
 *                                  required for MCLK switch.
 * @param[out]  pFbStopTimeUs        Return the fb stop time to caller.
 * @param[out]  pStatus              Return status to the caller.
 *
 * @return      FLCN_OK             MCLK switch request successfully completed.
 * @return      FLCN_ERR_TIMEOUT    If mempool reductions could not be completed
 *                                  within the timeout period.
 * @return      FLCN_ERROR          Fbfalcon was not able to complete the mclk
 *                                  switch successfully.
 */
FLCN_STATUS
clkTriggerMClkSwitchOnFbflcn
(
    RM_PMU_STRUCT_CLK_MCLK_SWITCH   *pMClkSwitchRequest,
    LwU32                           *pFbStopTimeUs,
    FLCN_STATUS                     *pStatus
)
{
    FLCN_STATUS     status              = FLCN_OK;

#if PMUCFG_FEATURE_ENABLED(PMU_DISP_IMP_INFRA)
    FLCN_STATUS     dispPreMClkStatus   = FLCN_OK;
    FLCN_STATUS     dispPostMClkStatus  = FLCN_OK;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispImp)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList); // NJ??

    //
    // Program the pre MCLK switch display settings here
    //
    dispPreMClkStatus = dispProgramPreMClkSwitchImpSettings_HAL(&Disp);
#endif

    //
    // Trigger MCLK switch on FB falcon
    //
    status = s_clkFbflcnMClkRpcExec(pMClkSwitchRequest->targetFreqKHz,
                                    pMClkSwitchRequest->flags,
                                    pFbStopTimeUs,
                                    pStatus);
    if (status == FLCN_OK)
    {
        //
        // If we've successfully got the acknowledgement from the FB falcon
        // check if it was able to complete the MCLK switch successfully.
        //
        if (*pStatus != FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_OK)
        {
            if (*pStatus == FBFLCN_PMU_MCLK_SWITCH_CMD_STATUS_FREQ_NOT_SUPPORTED)
            {
                status = FLCN_ERR_FREQ_NOT_SUPPORTED;
            }
            else
            {
                status = FLCN_ERROR;
            }
        }
    }

#if PMUCFG_FEATURE_ENABLED(PMU_DISP_IMP_INFRA)

    //
    // Program the post MCLK switch display settings here. We basically
    // restore the display settings programmed above.
    //

    dispPostMClkStatus = dispProgramPostMClkSwitchImpSettings_HAL(&Disp);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
#endif

    //
    // If status from either dispPreMClkStatus, MCLK switch status or
    // dispPostMClkStatus is not FLCN_OK we should return the error for the first
    // error detected
    //
#if PMUCFG_FEATURE_ENABLED(PMU_DISP_IMP_INFRA)
    if (dispPreMClkStatus != FLCN_OK)
    {
        return dispPreMClkStatus;
    }
#endif

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

#if PMUCFG_FEATURE_ENABLED(PMU_DISP_IMP_INFRA)
    if (dispPostMClkStatus != FLCN_OK)
    {
        return dispPostMClkStatus;
    }
#endif

    return FLCN_OK;
}

/*!
 * @brief       Trigger tRRD mode switch.
 *
 * @details     On GA10x, change sequencer will trigger this interface to switch the
 *              tRRD mode between engage vs disengage to trigger the performance
 *              slowdown.
 *
 * @param[in]   tFAW            Current tFAW value to be programmed in HW
 * @param[in]   mclkFreqKHz     MCLK frequency.
 *
 * @return      FLCN_OK         tRRD mode switch successfully completed.
 * @return      other           Error propagation from clkFbflcnTrrdProgram_HAL
 */
FLCN_STATUS
clkTriggerMClkTrrdModeSwitch
(
    LwU8    tFAW,
    LwU32   mclkFreqKHz
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED))
    {
        status = clkFbflcnTrrdSet_HAL(&Clk, tFAW, mclkFreqKHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkTriggerMClkTrrdModeSwitch_exit;
        }
    }

clkTriggerMClkTrrdModeSwitch_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper to execute MClk switch FBFLCN RPC.
 */
static FLCN_STATUS
s_clkFbflcnMClkRpcExec
(
    LwU32           targetFreqKHz,
    LwU32           flags,
    LwU32          *pFbStopTimeUs,
    FLCN_STATUS    *pStatus
)
{
    FLCN_STATUS status;
    LwU16       data16;
    LwU32       data32;

    // Setup the request parameters.
    data32 = targetFreqKHz;
    // TODO-Akshata: downsize flags to 16 bits or less.
    data16 = (LwU16)flags;

    status = pmuFbflcnRpcExelwte_HAL(&Pmu, PMU_FBFLCN_CMD_ID_MCLK_SWITCH,
                                     &data16, &data32, PMU_MCLK_WAIT_FOR_FBFLCN_US);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkFbflcnMClkRpcExec_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_TRRD_PROGRAM_SUPPORTED))
    {
        //
        // FBFLCN will always reset the tRRD settings during MCLK switch.
        // Update the reset value of tRRD in to PMU global cache.
        //
        clkFbflcnTrrdValResetSet(clkFbflcnTrrdGet_HAL(&Clk));
    }

    // Extract the response parameters.
    *pFbStopTimeUs = data32;
    *pStatus       = (LwU8)data16;

s_clkFbflcnMClkRpcExec_exit:
    return status;
}
