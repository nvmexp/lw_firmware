/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msgpxxxtuxxx.c
 * @brief  HAL-interface for the GP10X to TU10X Memory System Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "hwproject.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_bar0.h"

#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/
/* ------------------------ Public Functions  ------------------------------- */
/*!
 * @brief check if HOST is idle or not
 *
 * Function checked the host idle signals to determine if Host is
 * idle or not
 *
 *
 * @return      LW_TURE   HOST is idle
 *              LW_FALSE  HOST is busy
 */
LwBool
msIsHostIdle_GP10X(void)
{
    LwU32  idleStatus;

    idleStatus = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);

    //
    // Host will be idle only if host is not reporting _EXT_ACTIVE &&
    // _INT_ACTIVE && _NOT_QUIESCENT.
    //
    if (FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _HOST_NOT_QUIESCENT,
                     _TRUE, idleStatus)                            &&
        FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _HOST_EXT_NOT_ACTIVE,
                     _TRUE, idleStatus)                            &&
        FLD_TEST_DRF(_CPWR_PMU, _IDLE_STATUS, _HOST_INT_NOT_ACTIVE,
                     _TRUE, idleStatus))
    {
        return LW_TRUE;
    }
    return LW_FALSE;
}
