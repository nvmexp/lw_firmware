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
 * @file    pbi_gm10x.c
 *          PBI HAL Functions for GM10X and later GPUs
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_lw_xve.h"

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
//
// Move pmuPpbiSendAck_GM10X to resident code if MSCG onehost mode is supported.
// When MSCG Oneshot mode is supported, this function can be used by ISR,
// so we need to keep this function in resident code.
//
#if PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1)
void pmuPbiSendAck_GM10X(LwU8)
     GCC_ATTRIB_SECTION("imem_resident", "pmuPbiSendAck_GM10X");
#else
void pmuPbiSendAck_GM10X(LwU8)
     GCC_ATTRIB_SECTION("imem_cmdmgmtMisc", "pmuPbiSendAck_GM10X");
#endif // PMUCFG_FEATURE_ENABLED(PMU_MS_OSM_1)

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Read PBI request from message box registers.
 *
 * @param[out]  pRequest    Structure holding PBI request registers.
 */
void
pmuPbiRequestRd_GM10X
(
    POSTBOX_REGISTERS *pRequest
)
{
    pRequest->cmd =
        REG_RD32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_COMMAND);
    pRequest->cmn.dataIn =
        REG_RD32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_DATA_IN);
    pRequest->cmn.clientId =
        REG_RD32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_MUTEX);
}

/*!
 * @brief   Update the provided PBI command with given status
 *
 * @param[in]   dataOut     Value to be written to DATA_OUT register.
 */
void
pmuPbiStatusUpdate_GM10X
(
    LwU32 command,
    LwU8 status
)
{
    command = FLD_SET_DRF_NUM(_PBI, _COMMAND, _STATUS, status, command);
    REG_WR32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_COMMAND, command);
}

/*!
 * @brief   Write the PBI DATA_OUT register.
 *
 * @param[in]   dataOut     Value to be written to DATA_OUT register.
 */
void
pmuPbiDataOutWr_GM10X
(
    LwU32 dataOut
)
{
    REG_WR32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_DATA_OUT, dataOut);
}

/*!
 * @brief   Relay PBI interrupt to the RM.
 */
void
pmuPbiIrqRelayToRm_GM10X(void)
{
    REG_FLD_WR_DRF_DEF(BAR0_CFG, _XVE, _PRIV_MISC_1,
               _CYA_ROUTE_MSGBOX_CMD_INTR_TO_PMU, _DISABLE);
}

/*!
 * This function is used to acknowledge PBI command completion, and also handle
 * post command processing tasks like system or driver notify.
 *
 * @param[in]  status  Defines the pbi command status
 */
void
pmuPbiSendAck_GM10X
(
    LwU8 status
)
{
    LwU32 data;

    // update status first
    data = REG_RD32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_COMMAND);
    data = FLD_SET_DRF_NUM(_PBI, _COMMAND, _STATUS, status, data);
    REG_WR32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_COMMAND, data);

    // If driver notification is set interrupt RM to take appropriate action
    if (FLD_TEST_DRF(_PBI, _COMMAND, _DRV_NOTIFY, _TRUE, data))
    {
        //
        // NJ-TODO: This is either horribly wrong design or horribly wrong
        //          implementation. Aside from PMU deciding to forward request
        //          to the RM caller can force this forwarding leading to a case
        //          in which both RM and PMU might respond to the request?
        //
        REG_WR32(BAR0_CFG, LW_XVE_PRIV_MISC_1,
            FLD_SET_DRF(_XVE, _PRIV_MISC_1, _CYA_ROUTE_MSGBOX_CMD_INTR_TO_PMU, _DISABLE,
                REG_RD32(BAR0_CFG, LW_XVE_PRIV_MISC_1)));
    }
}

/* ------------------------- Private Functions ------------------------------ */
