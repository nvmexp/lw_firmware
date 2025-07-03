/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    illum_zone_rgb.h
 * @brief   ILLUM_ZONE_RGB related defines.
 *
 * @copydoc illum_zone.h
 */

#ifndef ILLUM_ZONE_RGB_H
#define ILLUM_ZONE_RGB_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_zone.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct ILLUM_ZONE_RGB ILLUM_ZONE_RGB;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * ILLUM_ZONE_RGB child class providing attributes of
 * ILLUM_ZONE of type RGB.
 */
struct ILLUM_ZONE_RGB
{
    /*!
     * ILLUM_ZONE parent class.
     * Must be first element of the structure!
     */
    ILLUM_ZONE    super;

    /*!
     * Union containing control mode data.
     */
    LW2080_CTRL_PMGR_ILLUM_ZONE_CTRL_MODE_RGB ctrlModeParams;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (illumZoneGrpIfaceModel10ObjSetImpl_RGB)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllumConstruct", "illumZoneGrpIfaceModel10ObjSetImpl_RGB");

// ILLUM_ZONE_RGB interfaces.

/* ------------------------ Include Derived Types --------------------------- */

#endif // ILLUM_ZONE_RGB_H
