/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pbi.c
 * @brief  Defines the routine for servicing PBI command request.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objgcx.h"
#include "pmu_objic.h"
#include "cmdmgmt/pbi.h"
#include "pmu_disp.h"
#include "pmu_objpmu.h"

#include "rmpbicmdif.h"

/* ------------------------- Private Functions ------------------------------ */

static void s_pbiGetCapabilities(LwU32 *pSupportedCaps)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "s_pbiGetCapabilities");
static void s_pbiServiceTriggerPmuModeSet(LwBool bSRExitOnly)
    GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "s_pbiServiceTriggerPmuModeSet");

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Read and parse PBI registers and service PBI commands.
 *
 * @return  'FLCN_OK' always succeeds
 */
FLCN_STATUS
pbiService(void)
{
    POSTBOX_REGISTERS  pbiReg;
    LwU8               status;

    // Re-enable the falling-edge PBI interrupt
    icEnablePBIInterrupt_HAL(&Ic);

    // Read PBI request from message box registers.
    pmuPbiRequestRd_HAL(&Pmu, &pbiReg);

    //
    // If PBI status is anything other than _UNDEFINED than request is allready
    // being handled or was submitted improperly.
    //
    if (!FLD_TEST_DRF(_PBI, _COMMAND, _STATUS, _UNDEFINED, pbiReg.cmd))
    {
        goto pbiService_exit;
    }

    //
    // Mark status as BUSY for clients to interpret PBI is lwrrently servicing
    // some request.
    //
    status = LW_PBI_COMMAND_STATUS_BUSY;
    pmuPbiStatusUpdate_HAL(&Pmu, pbiReg.cmd, status);

    // Service the requested command.
    switch (DRF_VAL(_PBI, _COMMAND, _FUNC_ID, pbiReg.cmd))
    {
        case LW_PBI_COMMAND_FUNC_ID_GET_CAPABILITIES:
        {
            s_pbiGetCapabilities(&(pbiReg.cmn.dataOut));
            pmuPbiDataOutWr_HAL(&Pmu, pbiReg.cmn.dataOut);
            status = LW_PBI_COMMAND_STATUS_SUCCESS;
            break;
        }
        case LW_PBI_COMMAND_FUNC_ID_CLEAR_EVENT:
        {
            // DataOut is reserved and should be always set to zero.
            pbiReg.cmn.dataOut = PBI_DATA_OUT_ZERO;
            pmuPbiDataOutWr_HAL(&Pmu, pbiReg.cmn.dataOut);
            // Never implemented so return success until fully deprecated.
            status = LW_PBI_COMMAND_STATUS_SUCCESS;
            break;
        }
        case LW_PBI_COMMAND_FUNC_ID_GET_LWRR_BACKLIGHT_LEVEL:
        case LW_PBI_COMMAND_FUNC_ID_SET_BACKLIGHT_LEVEL:
        case LW_PBI_COMMAND_FUNC_ID_HDCP_API_REQUEST:
        {
            // Relay the PBI interrupt to the RM.
            pmuPbiIrqRelayToRm_HAL(&Pmu);
            // Keep status == _BUSY
            break;
        }
        case LW_PBI_COMMAND_FUNC_ID_TRIGGER_PMU_MODESET:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PBI_MODE_SET))
            {
                // Relay the PBI request to the DISP task.
                s_pbiServiceTriggerPmuModeSet(LW_FALSE);
                // Keep status == _BUSY
            }
            break;
        }
        case LW_PBI_COMMAND_FUNC_ID_TRIGGER_SR_EXIT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_MODE_SET))
            {
                // Relay the PBI request to the DISP task.
                s_pbiServiceTriggerPmuModeSet(LW_TRUE); // SR_EXIT ONLY
                // Keep status == _BUSY
            }
            break;
        }
        case LW_PBI_COMMAND_FUNC_ID_ATTACH_DISPLAY_TO_MSCG:
            // Intercepted in the ISR and routed to the LPWR task.
            // If encountered here interpret it as an error.
        default:
        {
            // Common response for all unhandled requests.
            status = LW_PBI_COMMAND_STATUS_UNSPECIFIED_FAILURE;
            break;
        }
    }

    //
    // Status was set to _BUSY prior to the request handling. Update the status
    // for processed requests and keep it _BUSY for deferred ones (forwarded to
    // the RM or to the DISP tasks). Deferred request processing is responsible
    // to update the exelwtion status upon completion.
    //
    if (status != LW_PBI_COMMAND_STATUS_BUSY)
    {
        pmuPbiSendAck_HAL(&Pmu, status);
    }

pbiService_exit:
    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Computes the mask of supported PBI functions.
 *
 * @param[out]  pSupportedCaps  mask of supported PBI functions
 */
void
s_pbiGetCapabilities(LwU32 *pSupportedCaps)
{
    // first set the bits for the default capabilites
    *pSupportedCaps =
        (BIT(LW_PBI_COMMAND_FUNC_ID_GET_CAPABILITIES)           |
         BIT(LW_PBI_COMMAND_FUNC_ID_CLEAR_EVENT)                |
         BIT(LW_PBI_COMMAND_FUNC_ID_GET_LWRR_BACKLIGHT_LEVEL)   |
         BIT(LW_PBI_COMMAND_FUNC_ID_SET_BACKLIGHT_LEVEL)        |
         BIT(LW_PBI_COMMAND_FUNC_ID_HDCP_API_REQUEST)           |
         BIT(LW_PBI_COMMAND_FUNC_ID_ATTACH_DISPLAY_TO_MSCG));

    // if deep idle is supported then add it in supported capabilities.
    if (DIDLE_IS_INITIALIZED(Gcx))
    {
        *pSupportedCaps |= BIT(LW_PBI_COMMAND_FUNC_ID_DEEP_IDLE);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PBI_MODE_SET))
    {
        *pSupportedCaps |= BIT(LW_PBI_COMMAND_FUNC_ID_TRIGGER_PMU_MODESET);
        if (PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_MODE_SET))
        {
            *pSupportedCaps |= BIT(LW_PBI_COMMAND_FUNC_ID_TRIGGER_SR_EXIT);
        }
    }
}

/*!
 * This function service the PBI interrupt for TRIGGER_PMU_MODESET.
 *
 * @param[in]   bSRExitOnly     do an SR exit only
 */
static void
s_pbiServiceTriggerPmuModeSet
(
    LwBool bSRExitOnly
)
{
    DISPATCH_DISP dispDispatch;

    if (PMUCFG_FEATURE_ENABLED(PMUTASK_DISP))
    {
        // Trigger PMU modeset here for kernel panic for OS >= Windows 8
        dispDispatch.hdr.eventType =
            bSRExitOnly ? DISP_PBI_SR_EXIT_EVTID : DISP_PBI_MODESET_EVTID;
        lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, DISP),
                          &dispDispatch, sizeof(dispDispatch), 0);
    }
}
