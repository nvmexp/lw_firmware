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
 * @file    pbi_gh100.c
 *          PBI HAL Functions for GH100 and later GPUs
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_xtl_ep_pcfg_gpu.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "cmdmgmt/pbi.h"
#include "pmu_objpmu.h"

#include "rmpbicmdif.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Read PBI request from message box registers.
 *
 * @param[out]  pRequest    Structure holding PBI request registers.
 */
void
pmuPbiRequestRd_GH100
(
    POSTBOX_REGISTERS *pRequest
)
{
    pRequest->cmd =
        REG_RD32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND);
    pRequest->cmn.dataIn =
        REG_RD32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_DATA_IN);
    pRequest->cmn.clientId =
        REG_RD32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_MUTEX);
}

/*!
 * @brief   Update the provided PBI command with given status
 *
 * @param[in]   dataOut     Value to be written to DATA_OUT register.
 */
void
pmuPbiStatusUpdate_GH100
(
    LwU32 command,
    LwU8 status
)
{
    command = FLD_SET_DRF_NUM(_PBI, _COMMAND, _STATUS, status, command);
    REG_WR32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND, command);
}

/*!
 * @brief   Write the PBI DATA_OUT register.
 *
 * @param[in]   dataOut     Value to be written to DATA_OUT register.
 */
void
pmuPbiDataOutWr_GH100
(
    LwU32 dataOut
)
{
    REG_WR32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_DATA_OUT, dataOut);
}

/*!
 * @brief   Relay PBI interrupt to the RM.
 */
void
pmuPbiIrqRelayToRm_GH100(void)
{
    // NJ-TODO: Problem?
    // REG_FLD_WR_DRF_DEF(BAR0, _XVE, _PRIV_MISC_1,
    //            _CYA_ROUTE_MSGBOX_CMD_INTR_TO_PMU, _DISABLE);
}

/*!
 * @brief Update PBI status and clear interrupt bit.
 *
 * @param[in]   bSuccess     Indicate to update status with success.
 */
void
pmuUpdatePbiStatus_GH100
(
    LwBool bSuccess
)
{
    LwU32 data;

    // Read the current value.
    data = REG_RD32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND);

    // Clear the interrupt bit.
    data = FLD_SET_DRF_NUM(_PBI, _COMMAND, _INTERRUPT,
        LW_PBI_COMMAND_INTERRUPT_FALSE, data);

    // Update status
    if (bSuccess)
    {
        data = FLD_SET_DRF_NUM(_PBI, _COMMAND, _STATUS,
            LW_PBI_COMMAND_STATUS_SUCCESS, data);
    }
    else
    {
        data = FLD_SET_DRF_NUM(_PBI, _COMMAND, _STATUS,
            LW_PBI_COMMAND_STATUS_UNSPECIFIED_FAILURE, data);
    }

    REG_WR32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND, data);
}

/*!
 * This function is used to acknowledge PBI command completion, and also handle
 * post command processing tasks like system or driver notify.
 *
 * @param[in]  status  Defines the pbi command status
 */
void
pmuPbiSendAck_GH100
(
    LwU8 status
)
{
    LwU32 data;

    // update status first
    data = REG_RD32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND);
    data = FLD_SET_DRF_NUM(_PBI, _COMMAND, _STATUS, status, data);
    REG_WR32(BAR0, LW_EP_PCFG_GPU_VENDOR_SPECIFIC_MSGBOX_COMMAND, data);
}

/* ------------------------- Private Functions ------------------------------ */
