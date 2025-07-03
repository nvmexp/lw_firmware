/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_hwi2cgh100.c
 * @brief  Contains all I2C routines specific to GH100.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"
#include "pmu_obji2c.h"
#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "flcnifi2c.h"
#include "config/g_i2c_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * The I2C specification does not specify any timeout conditions for clock
 * stretching, i.e. any device can hold down SCL as long as it likes so this
 * value needs to be adjusted on case by case basis.
 */
#define I2C_SCL_CLK_TIMEOUT_1200US  1200

#define I2C_SCL_CLK_TIMEOUT_400KHZ (I2C_SCL_CLK_TIMEOUT_100KHZ * 4)
#define I2C_SCL_CLK_TIMEOUT_300KHZ (I2C_SCL_CLK_TIMEOUT_100KHZ * 3)
#define I2C_SCL_CLK_TIMEOUT_200KHZ (I2C_SCL_CLK_TIMEOUT_100KHZ * 2)
#define I2C_SCL_CLK_TIMEOUT_100KHZ (I2C_SCL_CLK_TIMEOUT_1200US / 10)

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Set the speed of the HW I2C controller on a given port.
 *
 * @param[in] port          The port identifying the controller.
 *
 * @param[in] speedMode     The speed mode to run at.
 */
void
i2cHwSetSpeedMode_GH100
(
    LwU32 port,
    LwU32 speedMode
)
{
    LwU32 timing = DRF_DEF(_PMGR, _I2C_TIMING, _IGNORE_ACK, _DISABLE) |
                   DRF_DEF(_PMGR, _I2C_TIMING, _TIMEOUT_CHECK, _ENABLE);

    switch (speedMode)
    {
        // Default should not be hit if above layers work correctly.
        default:
        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_100KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _100KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_100KHZ, timing);
            break;

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_200KHZ))
        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_200KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _200KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_200KHZ, timing);
            break;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_200KHZ)

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_300KHZ))
        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_300KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _300KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_300KHZ, timing);
            break;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_300KHZ)

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_400KHZ))
        case LW_RM_FLCN_I2C_FLAGS_SPEED_MODE_400KHZ:
            timing = FLD_SET_DRF(_PMGR, _I2C_TIMING, _SCL_PERIOD, _400KHZ,
                                 timing);
            timing = FLD_SET_DRF_NUM(_PMGR, _I2C_TIMING, _TIMEOUT_CLK_CNT,
                                     I2C_SCL_CLK_TIMEOUT_400KHZ, timing);
            break;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_I2C_400KHZ)
    }

    REG_WR32(BAR0, LW_PMGR_I2C_TIMING(port), timing);
}
