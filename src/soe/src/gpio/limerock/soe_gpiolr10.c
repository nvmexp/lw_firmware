/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
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
#include "soe_objgpio.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_gpio_hal.h"
#include "dev_pmgr.h"

/*!
 * Pre-STATE_INIT for GPIO
 */
void
gpioPreInit_LR10(void)
{
    // TODO: Add any early GPIO init 
}

/*!
 * @brief   Service GPIO interrupts
 */
#if (SOECFG_FEATURE_ENABLED(SOE_ONLY_LR10))
void
gpioService_LR10(void)
{
    //
    // TODO: Remove once we have actual handling in place.
    // Clear the enables (mask) for any interrupts we get so we dont come back
    // if we somehow got here.
    //
    PMGR_ISR_REG_WR32(LW_PMGR_PMU_INTR_MSK_GPIO_LIST_1, 0);
    PMGR_ISR_REG_WR32(LW_PMGR_PMU_INTR_MSK_GPIO_LIST_2, 0);
}
#endif
