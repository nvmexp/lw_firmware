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
 * @file thrmdevice_hbm2_site.h
 * @brief @copydoc thrmdevice_hbm2_site.c
 */

#ifndef THERMDEVICE_HBM2_SITE_H
#define THERMDEVICE_HBM2_SITE_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_HBM2_SITE THERM_DEVICE_HBM2_SITE;

/*!
 * Extends THERM_DEVICE providing attributes specific to HBM2_SITE device.
 */
struct THERM_DEVICE_HBM2_SITE
{
    /*!
     * THERM_DEVICE super class. This should always be the first member!
     */
    THERM_DEVICE    super;

    /*!
     * Index that selects the corresponding HBM2 site for this HBM2_SITE THERM device.
     */
    LwU8    siteIdx;

    /*!
     * Index of the first non floorswept FBPA within a HBM2 site
     */
    LwU8    fbpaIdx;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
ThermDeviceTempValueGet (thermDeviceTempValueGet_HBM2_SITE)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet_HBM2_SITE");

BoardObjGrpIfaceModel10ObjSet       (thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_SITE)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_SITE");

/* ------------------------- Include Derived Types ------------------------- */

#endif // THERMDEVICE_HBM2_SITE_H
