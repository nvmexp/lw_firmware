/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pbi_gm20x.c
 *          PBI HAL Functions for GM20X and later GPUs
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
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Update PBI status and clear interrupt bit.
 *
 * @param[in]   bSuccess     Indicate to update status with success.
 */
void
pmuUpdatePbiStatus_GM20X
(
    LwBool bSuccess
)
{
    LwU32 data;

    // Read the current value.
    data = REG_RD32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_COMMAND);

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

    REG_WR32(BAR0_CFG, LW_XVE_VENDOR_SPECIFIC_MSGBOX_COMMAND, data);
}
