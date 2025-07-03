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
#include "soe_objpmgr.h"
#include "soe_objgpio.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_gpio_hal.h"
#include "dev_pmgr.h"

/*!
 * Pre-STATE_INIT for GPIO
 */
void
gpioPreInit_LS10(void)
{
    // TODO: Add any early GPIO init 
}

/*!
 * @brief   Service GPIO interrupts
 */
void
gpioService_LS10(void)
{
    // TODO: Service GPIO interrupts
}

