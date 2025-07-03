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
 * @file    illum_zone.h
 * @brief   Illumination Zone related defines.
 *
 * Detailed documentation is located at:
 * https://confluence.lwpu.com/display/BS/RGB+illumination+control+-+RID+69692
 */

#ifndef ILLUM_ZONE_H
#define ILLUM_ZONE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct ILLUM_ZONE ILLUM_ZONE, ILLUM_ZONE_BASE;

#include "pmgr/illum_device.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_ILLUM_ZONE \
    (&Illum.zones.super.super)

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define ILLUM_ZONE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(ILLUM_ZONE, (_objIdx)))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all Illumination Zones.
 */
struct ILLUM_ZONE
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ    super;

    /*!
     * Provider index for representing logical to physical zone mapping.
     */
    LwU8             provIdx;

    /*!
     * Control Mode for this zone.
     */
    LwU8             ctrlMode;

    /*!
     * Pointer to the Illumination device that controls this zone.
     */
    ILLUM_DEVICE    *pIllumDevice;
};

/*!
 * Collection of all ILLUM_ZONE objects and related information.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32 super;
} ILLUM_ZONES;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler   (illumZoneBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibBoardObj", "illumZoneBoardObjGrpIfaceModel10Set");


// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (illumZoneGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllumConstruct", "illumZoneGrpIfaceModel10ObjSetImpl_SUPER");

// ILLUM_ZONE interfaces.

/* ------------------------ Include Derived Types --------------------------- */
#include "pmgr/illum_zone_rgb.h"
#include "pmgr/illum_zone_rgbw.h"
#include "pmgr/illum_zone_single_color.h"

#endif // ILLUM_ZONE_H
