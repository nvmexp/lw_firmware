/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_i2gm20x.c
 * @brief  Contains all I2C specific code applicable to GM20X and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_bar0.h"

#include "dev_pmgr.h"
#include "dev_pmgr_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_obji2c.h"
#include "config/g_i2c_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Read all I2C Port's Priv Masks and set port index in the local mask if
 * Priv Sec is enabled for that port.
 */
LwU32
i2cPortsPrivMaskGet_GM20X(void)
{
    LwU32   i2cPortsPrivMask = 0;
    LwU8    portIdx;

    // Run the loop through all the I2C ports
    for (portIdx = 0; portIdx < LW_PMGR_I2C_PRIV_LEVEL_MASK__SIZE_1; portIdx++)
    {
        if (FLD_TEST_DRF(_PMGR, _I2C_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0,
                         _DISABLE,
                         REG_RD32(BAR0, LW_PMGR_I2C_PRIV_LEVEL_MASK(portIdx))))
        {
            i2cPortsPrivMask |= BIT(portIdx);
        }
    }

    return i2cPortsPrivMask;
}
