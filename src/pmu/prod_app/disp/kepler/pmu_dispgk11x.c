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
 * @file   pmu_dispgk11x.c
 * @brief  HAL-interface for the GK11x Display Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_disp.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

/*!
 * Get the number of sors.
 *
 * @return Number of sors.
 *
 * @note
 *     This HAL is always safe to call.
 */
LwU8
dispGetNumberOfSors_GMXXX(void)
{
    return DISP_IS_PRESENT() ?
        DRF_VAL(_PDISP, _CLK_REM_MISC_CONFIGA, _NUM_SORS,
            REG_RD32(BAR0, LW_PDISP_CLK_REM_MISC_CONFIGA)) : 0;
}

