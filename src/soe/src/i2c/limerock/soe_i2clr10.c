/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objpmgr.h"
#include "soe_obji2c.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_i2c_hal.h"
#include "dev_pmgr.h"

/*!
 * Pre-STATE_INIT for I2C
 */
void
i2cPreInit_LR10(void)
{
    // TODO: Add any early I2C init 
}

/*!
 * @brief   Service I2C interrupts
 */
void
i2cService_LR10(void)
{
    //
    // TODO: Remove once we have actual handling in place.
    // Clear the enables (mask) for any interrupts we get so we dont come back
    // if we somehow got here.
    //
    PMGR_ISR_REG_WR32(LW_PMGR_PMU_INTR_MSK_I2C, 0);
}
