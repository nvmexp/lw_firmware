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
 * @file    illum_device.h
 * @brief   Illumination Device related defines.
 *
 * Detailed documentation is located at:
 * https://confluence.lwpu.com/display/BS/RGB+illumination+control+-+RID+69692
 */

#ifndef ILLUM_DEVICE_H
#define ILLUM_DEVICE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct ILLUM_DEVICE ILLUM_DEVICE, ILLUM_DEVICE_BASE;

#include "pmgr/illum_zone.h"
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_ILLUM_DEVICE \
    (&Illum.devices.super.super)

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define ILLUM_DEVICE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(ILLUM_DEVICE, (_objIdx)))

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * Applies requested settings for a zone to HW via MLW.
 *
 * @param[in]  pIllumDevice
 *      Pointer to the Illumination Device controlling this zone.
 * @param[out] pIllumZone
 *      Pointer to the Illumination Zone.
 *
 * @return  FLCN_OK
 *      Requested settings were applied to HW successfully.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define IllumDeviceDataSet(fname) FLCN_STATUS (fname)(ILLUM_DEVICE *pIllumDevice, ILLUM_ZONE *pIllumZone)

/*!
 * Applies synchronization settings to the MLW.
 *
 * @param[in]  pIllumDevice
 *      Pointer to the Illumination Device controlling this zone.
 * @param[out] pSyncData
 *      Pointer to the synchronization data.
 *
 * @return  FLCN_OK
 *      Requested synchronization settings were applied to HW successfully.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define IllumDeviceSync(fname) FLCN_STATUS (fname)(ILLUM_DEVICE *pIllumDevice, RM_PMU_ILLUM_DEVICE_SYNC *pSyncData)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all Illumination Devices.
 */
struct ILLUM_DEVICE
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ    super;

    /*!
     * Supported control modes for this illumination device.
     */
    LwU32 ctrlModeMask;

    /*!
     * Structure containing the synchronization data for the illumination device.
     */
    RM_PMU_ILLUM_DEVICE_SYNC
          syncData;
};

/*!
 * Collection of all ILLUM_DEVICE objects and related information.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32 super;
} ILLUM_DEVICES;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler   (illumDeviceBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibBoardObj", "illumDeviceBoardObjGrpIfaceModel10Set");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (illumDeviceGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllumConstruct", "illumDeviceGrpIfaceModel10ObjSetImpl_SUPER");

// ILLUM_DEVICE interfaces.
IllumDeviceDataSet      (illumDeviceDataSet)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllum", "illumDeviceDataSet");
IllumDeviceSync         (illumDeviceSync)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllum", "illumDeviceSync");

/* ------------------------ Include Derived Types --------------------------- */
#include "pmgr/illum_device_mlwv10.h"
#include "pmgr/illum_device_gpio_pwm_single_color_v10.h"
#include "pmgr/illum_device_gpio_pwm_rgbw_v10.h"

#endif // ILLUM_DEVICE_H
