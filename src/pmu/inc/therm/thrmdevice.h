/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmdevice.h
 * @brief @copydoc thrmdevice.c
 */

#ifndef THRMDEVICE_H
#define THRMDEVICE_H

#include "g_thrmdevice.h"
#ifndef G_THRMDEVICE_H
#define G_THRMDEVICE_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "ctrl/ctrl2080/ctrl2080thermal.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuiftherm.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_THERM_DEVICE \
    (&(Therm.devices.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define THERM_DEVICE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(THERM_DEVICE, (_objIdx)))

/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE THERM_DEVICE, THERM_DEVICE_BASE;

// THERM_DEVICE interfaces

/*!
 * @brief  Obtain temperature of the appropriate THERM_DEVICE given the provider index.
 *
 * @param[in]  pDev             THERM_DEVICE object pointer
 * @param[in]  thermDevProvIdx  Provider index to query for temperature
 * @param[out] pLwTemp          Current temperature
 *
 * @return FLCN_OK
 *      Temperature successfully obtained.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      If interface isn't supported.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define ThermDeviceTempValueGet(fname) FLCN_STATUS (fname)(THERM_DEVICE *pDev, LwU8 thermDevProvIdx, LwTemp *pLwTemp)

/*!
 * @brief Extends BOARDOBJ providing attributes common to all THERM_DEVICES.
 */
struct THERM_DEVICE
{
    /*!
     * Super class. This should always be the first member!
     */
    BOARDOBJ    super;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS thermDevicesLoad(void)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDevicesLoad");

mockable ThermDeviceTempValueGet (thermDeviceTempValueGet)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet");

BoardObjGrpIfaceModel10CmdHandler   (thermDeviceBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermDeviceBoardObjGrpIfaceModel10Set");

BoardObjGrpIfaceModel10ObjSet       (thermDeviceGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermDeviceGrpIfaceModel10ObjSetImpl_SUPER");

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetEntry   (thermThermDeviceIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermThermDeviceIfaceModel10SetEntry");

/* ------------------------ Include Derived Types -------------------------- */
#include "therm/thrmdevice_gpu.h"
#include "therm/thrmdevice_gpu_gpc_tsosc.h"
#include "therm/thrmdevice_gpu_sci.h"
#include "therm/thrmdevice_gpu_gpc_combined.h"
#include "therm/thrmdevice_gddr6_x_combined.h"
#include "therm/thrmdevice_i2c.h"
#include "therm/thrmdevice_hbm2_site.h"
#include "therm/thrmdevice_hbm2_combined.h"

#endif // G_THRMDEVICE_H
#endif // THRMDEVICE_H
