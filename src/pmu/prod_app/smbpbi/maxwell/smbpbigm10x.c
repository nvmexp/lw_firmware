/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright    2018   by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    smbpbigm10x.x
 * @brief   PMU HAL functions for GM10X+ SMBPBI
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "dev_bus_addendum.h"

/* ------------------------ Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief   Evaluate response to the SMBPBI Get GPU reset required request
 *          based on the LW_PBUS_SW_SCRATCH(30) 0:0 value
 *
 * @param[out]  pData   SMBPBI data-out register value
 *
 * @return  LW_MSGBOX_CMD_STATUS_SUCCESS
 *          LW_MSGBOX_CMD_STATUS_ERR_MISC - internal error
 */
LwU8
smbpbiGetGpuResetRequiredFlag_GM10X
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

    val = FLD_TEST_DRF(_PBUS, _SW_SCRATCH30, _GPU_RESET_REQUIRED, _ON, reg) ?
            LW_MSGBOX_DATA_GPU_RESET_REQUIRED_ON :
            LW_MSGBOX_DATA_GPU_RESET_REQUIRED_OFF;

    *pData = FLD_SET_DRF_NUM(_MSGBOX, _DATA, _GPU_RESET_REQUIRED, val, *pData);

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}
