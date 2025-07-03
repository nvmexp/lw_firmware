/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmdevice_hbm2_combined.h
 * @brief @copydoc thrmdevice_hbm2_combined.c
 */

#ifndef THERMDEVICE_HBM2_COMBINED_H
#define THERMDEVICE_HBM2_COMBINED_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_HBM2_COMBINED THERM_DEVICE_HBM2_COMBINED;

/*!
 * Extends THERM_DEVICE providing attributes specific to HBM2_COMBINED device.
 */
struct THERM_DEVICE_HBM2_COMBINED
{
    /*!
     * THERM_DEVICE super class. This should always be the first member!
     */
    THERM_DEVICE    super;

    /*!
     * Total number of HBM sites.
     */
    LwU8    numSites;

    /*!
     * Buffer to hold non-floorswept FBPA indices for each HBM site.
     */
    LwU8   *pFbpaIdx;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
ThermDeviceTempValueGet (thermDeviceTempValueGet_HBM2_COMBINED)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet_HBM2_COMBINED");

BoardObjGrpIfaceModel10ObjSet       (thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_COMBINED)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_COMBINED");

/* ------------------------- Include Derived Types ------------------------- */

#endif // THERMDEVICE_HBM2_COMBINED_H
