/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  thrmdevice_gddr6_x_combined.c
 * @brief THERM Thermal Device Specific to GDDR6_X_COMBINED device class.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/thrmdevice.h"
#include "therm/objtherm.h"
#include "pmu_objfb.h"

/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief   Loads GDDR6_X_COMBINED THERM_DEVICE SW state.
 *
 * @return FLCN_OK
 *      Operation completed successfully.
 * @return FLCN_ERR_FEATURE_NOT_ENABLED
 *      GDDR6/GDDR6X thermal sensor isn't enabled.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
thermDeviceLoad_GDDR6_X_COMBINED
(
    THERM_DEVICE   *pDev
)
{
    LwBool      bEnabled    = LW_FALSE;
    FLCN_STATUS status      = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY))
    {
        PMU_HALT_OK_OR_GOTO(status,
            fbSensorGDDR6XCombinedIsEnabled_HAL(&Fb, &bEnabled),
            thermDeviceLoad_GDDR6_X_COMBINED_exit);

        // Make sure GDDR6/GDDR6X thermal sensor is enabled by this time.
        if (!bEnabled)
        {
            status = FLCN_ERR_FEATURE_NOT_ENABLED;
            PMU_BREAKPOINT();
            goto thermDeviceLoad_GDDR6_X_COMBINED_exit;
        }
    }

thermDeviceLoad_GDDR6_X_COMBINED_exit:
    return status;
}

/*!
 * Return the internal temperature for GDDR6_X_COMBINED device class.
 * @copydoc ThermDeviceTempValueGet()
 */
FLCN_STATUS
thermDeviceTempValueGet_GDDR6_X_COMBINED
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    return fbGDDR6XCombinedTempGet_HAL(&Fb, thermDevProvIdx, pLwTemp);
}

/* ------------------------- Private Functions ------------------------------ */
