/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    smbpbiga100.c
 * @brief   PMU HAL functions for GA100 SMBPBI
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "dev_bus_addendum.h"
#include "pmu_objsmbpbi.h"

/* ------------------------ Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"
#include "config/g_bif_private.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- External Definitions -------------------------- */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Static Functions ------------------------------ */

/* ------------------------- Public Functions ------------------------------ */
LwU8
smbpbiGetSmcStates_GA100
(
    LwBool *pbSmcSupported,
    LwBool *pbSmcEnabled,
    LwBool *pbSmcPending
)
{
    LwU32 reg  = REG_RD32(BAR0, LW_PBUS_SW_SCRATCH(1));

    *pbSmcSupported = FLD_TEST_DRF(_PBUS, _SW_SCRATCH1_INFOROM_SEN,
                      _PRESENT, _TRUE, reg);
    *pbSmcEnabled   = FLD_TEST_DRF(_PBUS, _SW_SCRATCH1_SMC,
                      _MODE, _ON, reg);
    *pbSmcPending   = FLD_TEST_DRF(_PBUS, _SW_SCRATCH1_INFOROM_SEN,
                     _SMC, _ENABLE, reg);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief   Evaluate response to the SMBPBI Get GPU drain and reset recommended
 *          request based on the LW_PBUS_SW_SCRATCH(30) 1:1 value
 *
 * @param[out]  pData   SMBPBI data-out register value
 *
 * @return  LW_MSGBOX_CMD_STATUS_SUCCESS
 *          LW_MSGBOX_CMD_STATUS_ERR_MISC - internal error
 */
LwU8
smbpbiGetGpuDrainAndResetRecommendedFlag_GA100
(
    LwU32   *pData
)
{
    LwU32       reg;
    unsigned    val;

    if (NULL == pData)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_MISC;
    }

    reg = REG_RD32(BAR0, LW_PBUS_SW_SCRATCH(30));

    val = FLD_TEST_DRF(_PBUS, _SW_SCRATCH30, _GPU_DRAIN_AND_RESET_RECOMMENDED, _YES, reg) ?
            LW_MSGBOX_DATA_GPU_DRAIN_AND_RESET_RECOMMENDED_YES :
            LW_MSGBOX_DATA_GPU_DRAIN_AND_RESET_RECOMMENDED_NO;

    *pData = FLD_SET_DRF_NUM(_MSGBOX, _DATA, _GPU_DRAIN_AND_RESET_RECOMMENDED, val, *pData);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

