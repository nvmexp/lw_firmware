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
 * @file    thrmbagm10x.c
 * @brief   Thermal PMU HAL functions for GM10X. thermal BA functionality
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_therm.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Check the HW BA window index is out of range
 *
 * @param[in]  provHwBaWinIdx    Index of HW BA window
 *
 * @return FLCN_ERR_NOT_SUPPORTED    if invalid HW BA window index specified
 *
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
thermCheckHwBaWindowIdx_GM10X
(
    LwU8    provHwBaWinIdx
)
{
    if (provHwBaWinIdx >= LW_THERM_PEAKPOWER_CONFIG1__SIZE_1)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
