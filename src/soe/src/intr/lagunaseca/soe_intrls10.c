/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
#include "dev_ctrl_ip.h"
#include "dev_therm.h"
#include "config/g_therm_private.h"
#include "config/g_gin_private.h"
#include "config/g_gin_hal.h"


#include "dev_minion_ip.h"
#include "dev_lwlsaw_ip.h"

#define ISR_REG_RD32(addr)        _soeBar0RegRd32_LR10(addr) 
#define ISR_REG_WR32(addr, data)  _soeBar0RegWr32NonPosted_LR10(addr, data)

/*!
 * Pre-STATE_INIT for GIN
 */
void
intrPreInit_LS10(void)
{
    if (SOECFG_ENGINE_ENABLED(GIN))
    {
        ginPreInit_HAL();
    }
}

/*!
 * @brief   Service SAW interrupts
 */
void
intrService_LS10(void)
{
    ginService_HAL();
}

/*!
 * @brief   Check if lwlipt is in reset.
 */
LwBool
intrIsLwlwInReset_LS10
(
    LwU32 lwliptIdx
)
{
    return ginIsLwlwInReset_HAL(&Gin, lwliptIdx);
}


/*!
 * @brief  Clear thermal alert interrupt in GIN block
 */
void
intrClearThermalAlert_LS10(void)
{
    ginClearThermalAlertIntr_HAL();
}


