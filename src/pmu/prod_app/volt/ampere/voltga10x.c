/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    voltga10x.c
 * @brief   PMU HAL functions related to voltage devices for GA10X+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_gc6_island.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "volt/voltdev.h"

#include "config/g_volt_private.h"

#define SCI_PGOOD_TIMEOUT_NS      2500000

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Polls the status of PGOOD detector signal to be ON/OFF.
 *
 * @param[in]    bPollOn      Boolean indicating the desired signal value to poll on.
 *                            LW_TRUE  - Poll on PGOOD_STATUS to be ON.
 *                            LW_FALSE - Poll on PGOOD_STATUS to be OFF.
 *
 * @return FLCN_OK            PGOOD signal is settled to the desired value.
 * @return FLCN_ERR_TIMEOUT   PGOOD signal did not settle to the desired value in the specified time.
 */
FLCN_STATUS
voltPollPgoodStatus_GA10X
(
    LwBool bPollOn
)
{
    FLCN_STATUS status = FLCN_OK;

    if (bPollOn)
    {
        if (!PMU_REG32_POLL_NS(
                 LW_PGC6_SCI_PGOOD,
                 DRF_SHIFTMASK(LW_PGC6_SCI_PGOOD_STATUS),
                 DRF_DEF(_PGC6, _SCI_PGOOD, _STATUS, _TRUE),
                 SCI_PGOOD_TIMEOUT_NS,
                 PMU_REG_POLL_MODE_BAR0_EQ))
        {
            status = FLCN_ERR_TIMEOUT;
            PMU_BREAKPOINT();
            goto voltPollPgoodStatus_GA10X_exit;
        }
    }
    else
    {
        if (!PMU_REG32_POLL_NS(
                 LW_PGC6_SCI_PGOOD,
                 DRF_SHIFTMASK(LW_PGC6_SCI_PGOOD_STATUS),
                 DRF_DEF(_PGC6, _SCI_PGOOD, _STATUS, _FALSE),
                 SCI_PGOOD_TIMEOUT_NS,
                 PMU_REG_POLL_MODE_BAR0_EQ))
        {
            status = FLCN_ERR_TIMEOUT;
            PMU_BREAKPOINT();
            goto voltPollPgoodStatus_GA10X_exit;
        }
    }

voltPollPgoodStatus_GA10X_exit:
    return status;
}
