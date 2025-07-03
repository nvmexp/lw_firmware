/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmdevice_gddr6_x_combined.h
 * @brief @copydoc thrmdevice_gddr6_x_combined.c
 */

#ifndef THERMDEVICE_GDDR6_X_COMBINED_H
#define THERMDEVICE_GDDR6_X_COMBINED_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @brief Wrapper interface for @ref thermDeviceGrpIfaceModel10ObjSetImpl_SUPER()
 */
#define thermDeviceGrpIfaceModel10ObjSetImpl_GDDR6_X_COMBINED(pModel10, ppBoardObj, size, pDesc)   \
            thermDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pDesc)

/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_GDDR6_X_COMBINED THERM_DEVICE_GDDR6_X_COMBINED;

/*!
 * Extends THERM_DEVICE providing attributes specific to GDDR6_X_COMBINED device.
 */
struct THERM_DEVICE_GDDR6_X_COMBINED
{
    /*!
     * THERM_DEVICE super class. This should always be the first member!
     */
    THERM_DEVICE super;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
FLCN_STATUS thermDeviceLoad_GDDR6_X_COMBINED(THERM_DEVICE *pDev)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceLoad_GDDR6_X_COMBINED");

ThermDeviceTempValueGet (thermDeviceTempValueGet_GDDR6_X_COMBINED)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet_GDDR6_X_COMBINED");

/* ------------------------- Include Derived Types ------------------------- */

#endif // THERMDEVICE_GDDR6_X_COMBINED_H
