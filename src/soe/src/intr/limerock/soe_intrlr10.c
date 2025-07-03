/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objsaw.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_saw_hal.h"
#include "dev_lwlsaw_ip.h"

/*!
 * Pre-STATE_INIT for SAW
 */
void
intrPreInit_LR10(void)
{
    if (SOECFG_ENGINE_ENABLED(SAW))
    {
        sawPreInit_HAL();
    }
}

/*!
 * @brief   Service SAW interrupts
 */
void
intrService_LR10(void)
{
    sawService_HAL();
}

/*!
 * @brief   Check if lwlipt is in reset.
 */
LwBool
intrIsLwlwInReset_LR10
(
    LwU32 lwliptIdx
)
{
    return sawIsLwlwInReset_HAL(&Saw, lwliptIdx);
}

/*!
 * @brief  Enable thermal alert interrupt in SAW block
 */
void
intrEnableThermalAlertIntr_LR10(void)
{
    sawEnableThermalAlertIntr_HAL();
}

/*!
 * @brief   Disable thermal alert interrupt in SAW block
 */
void
intrDisableThermalAlertIntr_LR10(void)
{
    sawDisableThermalAlertIntr_HAL();
}

/*!
 * @brief  Disable thermal overt interrupt in SAW block
 */
void
intrDisableThermalOvertIntr_LR10(void)
{
    sawDisableThermalOvertIntr_HAL();
}

/*!
 * @brief  Clear thermal alert interrupt in SAW block
 */
void
intrClearThermalAlert_LR10(void)
{
    sawClearThermalAlertIntr_HAL();
}

