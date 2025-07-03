/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_i2cgf10x.c
 * @brief  Contains all I2C routines specific to GF10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pmgr.h"
#include "pmu_objpmu.h"
#include "pmu_obji2c.h"
#include "pmu_objtimer.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_i2c_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */


/*!
 * Set the new operating mode, and return the previous mode.
 *
 * @param[in]  port    The port identifier.
 * @param[in]  bSwI2c
 *    The target mode (LW_TRUE = bit-bang, LW_FALSE = HWI2C).
 *
 * @param[out] pBWasSwI2c
 *    Optional (may be NULL) pointer written with the current I2C operating-
 *    mode when requested (LW_TRUE = SW bit-bang). Ignored when NULL.
 *
 * @return 'FLCN_OK'
 *      if the operation completed successfully.
 *
 * @return 'FLCN_ERR_I2C_BUSY'
 *      if the operation timed out waiting for the HW controller.
 *
 * @return 'FLCN_ERR_I2C_BUSY'
 *      if the current mode cannot be exited safely.
 *
 * @note
 *     This function does not deal with HW polling (which is broken, see bug
 *     671708) or stereo vision.
 *
 * @note
 *     This function does not check the current bus state before setting the
 *     mode. In cases where it matters, the caller is responsible for ensuring
 *     the bus is in a good state before calling this function.
 */
FLCN_STATUS
i2cSetMode_GMXXX
(
    LwU32   port,
    LwBool  bSwI2c,
    LwBool *pBWasSwI2c
)
{
    LwU32 override;

    // if requested, return/save the current operating-mode
    if (pBWasSwI2c != NULL)
    {
        *pBWasSwI2c = FLD_TEST_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _ENABLE,
            REG_RD32(BAR0, LW_PMGR_I2C_OVERRIDE(port)));
    }

    //
    // Set the I2C override register for the port to allow the PMU to primary
    // the bus when SW bit-banging mode is requested. Make sure it is disabled
    // when HWI2C mode is requested.
    //
    override = DRF_DEF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _DISABLE) |
               DRF_DEF(_PMGR, _I2C_OVERRIDE, _SDA    , _OUT_ONE) |
               DRF_DEF(_PMGR, _I2C_OVERRIDE, _SCL    , _OUT_ONE);
    if (bSwI2c)
    {
        override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _PMU, _ENABLE , override);
    }
    else
    {
        override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _PMU, _DISABLE, override);
    }
    REG_WR32(BAR0, LW_PMGR_I2C_OVERRIDE(port), override);

    //
    // When entering HWI2C-mode, we need to reset the controller.  Since we
    // don't necessarily do so every time we enter HWI2C-mode, reset it
    // regardless of the previous mode.
    //
    if (!bSwI2c)
    {
        return i2cHwReset_HAL(&I2c, port);
    }
    return FLCN_OK;
}

/*!
 * Restore the previous operating mode.
 *
 * @param[in] port              The port identifier.
 *
 * @param[in] bWasBb            The previous operating mode.
 *
 * @return void
 */
void
i2cRestoreMode_GMXXX
(
    LwU32 port,
    LwU32 bWasBb
)
{
    LwU32 override = DRF_DEF(_PMGR, _I2C_OVERRIDE, _PMU, _DISABLE) |
                     DRF_DEF(_PMGR, _I2C_OVERRIDE, _SCL, _OUT_ONE) |
                     DRF_DEF(_PMGR, _I2C_OVERRIDE, _SDA, _OUT_ONE);

    if (bWasBb)
    {
        override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _ENABLE,
                               override);
    }
    else // !bWasBb
    {
        override = FLD_SET_DRF(_PMGR, _I2C_OVERRIDE, _SIGNALS, _DISABLE,
                               override);
    }

    REG_WR32(BAR0, LW_PMGR_I2C_OVERRIDE(port), override);
}
