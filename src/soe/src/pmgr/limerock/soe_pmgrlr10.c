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
#include "soe_objgpio.h"
#include "soe_objspi.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_pmgr_hal.h"
#include "config/g_i2c_hal.h"
#include "config/g_gpio_hal.h"
#include "config/g_spi_hal.h"
#include "dev_pmgr.h"

/*!
 * Pre-STATE_INIT for PMGR
 */
void
pmgrPreInit_LR10(void)
{
    // TODO: Add misc PMGR init 
}

/*!
 * @brief   Service PMGR interrupts
 */
void
pmgrService_LR10(void)
{
    LwU32 irqs;

    // Top level status, no masks available at this level
    irqs = PMGR_ISR_REG_RD32(LW_PMGR_PMU_INTR_DISPATCH);
    
    // I2C devices
    if (FLD_TEST_DRF(_PMGR_PMU, _INTR_DISPATCH, _I2C, _PENDING, irqs))
    {
        i2cService_HAL();
    }

    // GPIOs
    if ((FLD_TEST_DRF(_PMGR_PMU, _INTR_DISPATCH, _GPIO_LIST_1, _PENDING, irqs) ||
        (FLD_TEST_DRF(_PMGR_PMU, _INTR_DISPATCH, _GPIO_LIST_2, _PENDING, irqs))))
    {
        gpioService_HAL();
    }

    // SPI devices
    if (FLD_TEST_DRF(_PMGR_PMU, _INTR_DISPATCH, _SPI, _PENDING, irqs))
    {
        spiService_HAL();
    }
}
