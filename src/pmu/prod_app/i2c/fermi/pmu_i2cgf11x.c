/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_i2cgf11x.c
 * @brief  Contains all I2C routines specific to GF11X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pmgr.h"
#include "dev_pwr_csb.h"
#include "pmu_objpmu.h"
#include "pmu_obji2c.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_i2c_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Determine the GPIO associated with the given port's SCL line.
 *
 * @param[in] port      The physical port to look up.
 *
 * @return Bit position of the SCL line within the GPIO registers.
 */
LwU32
i2cGetSclGpioBit_GMXXX
(
    LwU8 port
)
{
    return DRF_BASE(LW_CPWR_PMU_GPIO_OUTPUT_I2C_SCL_IDX(port));
}

/*!
 * Determine the GPIO associated with the given port's SDA line.
 *
 * @param[in] port      The physical port to look up.
 *
 * @return Bit position of the SDA line within the GPIO registers.
 */
LwU32
i2cGetSdaGpioBit_GMXXX
(
    LwU8 port
)
{
    return DRF_BASE(LW_CPWR_PMU_GPIO_OUTPUT_I2C_SDA_IDX(port));
}

/*!
 * @brief Initialize the GPIOs for I2C.
 *
 * By initializing the GPIOs for I2C to 1, we can turn on the PMU override for
 * each bus without worrying about glitches.
 */
void
i2cInitI2cGpios_GMXXX(void)
{
     //
    // By default, the PMU GPIOs are set to CLEAR for all of the I2C lines.
    // It is much better for the primary to not interfere with the bus
    // initially when we allow the GPIOs to operate.  So, before any I2C
    // transactions are handled, set all lines to HIGH.
    //

     REG_WR32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_SET,
        DRF_SHIFTMASK(LW_CPWR_PMU_GPIO_OUTPUT_SET_I2C_SCL) |
        DRF_SHIFTMASK(LW_CPWR_PMU_GPIO_OUTPUT_SET_I2C_SDA));
}

/*!
 * @brief Is the bus associated with 'port' ready for a bus transaction?
 *
 * Check to see if SCL and SDA is lwrrently high (ie. not being driven low).
 * If either (or both) lines are low, consider the bus as "busy" and therefore
 * not ready to accept the next transaction.
 *
 * @param[in]  port  The physical port for the bus to consider
 *
 * @return LW_TRUE
 *     Both SCL and SDA are high. The primary may commence the next transaction.
 *
 * @return LW_FALSE
 *     One (or both) of the I2C lines is being pulled/driven low.
 */
LwBool
i2cIsBusReady_GMXXX
(
    LwU32 port
)
{
    LwU32 reg32 = REG_RD32(BAR0, LW_PMGR_I2C_OVERRIDE(port));

    // ensure that neither line is being driven low (by primary or secondary(s))
    return FLD_TEST_DRF(_PMGR, _I2C_OVERRIDE, _SCLPAD_IN, _ONE, reg32) &&
           FLD_TEST_DRF(_PMGR, _I2C_OVERRIDE, _SDAPAD_IN, _ONE, reg32);
}
