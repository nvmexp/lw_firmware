/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmdevice_gpu_gpc_tsosc.h
 * @brief @copydoc thrmdevice_gpu_gpc_tsosc.c
 */

#ifndef THERMDEVICE_GPU_GPC_TSOSC_H
#define THERMDEVICE_GPU_GPC_TSOSC_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_GPU_GPC_TSOSC THERM_DEVICE_GPU_GPC_TSOSC;

/*!
 * Extends THERM_DEVICE providing attributes specific to GPU_GPC_TSOSC device.
 */
struct THERM_DEVICE_GPU_GPC_TSOSC
{
    /*!
     * THERM_DEVICE super class. This should always be the first member!
     */
    THERM_DEVICE    super;

    /*!
     * Index that selects the corresponding GPC for this GPU_GPC_TSOSC THERM device.
     */
    LwU8    gpcTsoscIdx;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
ThermDeviceTempValueGet (thermDeviceTempValueGet_GPU_GPC_TSOSC)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet_GPU_GPC_TSOSC");

BoardObjGrpIfaceModel10ObjSet       (thermDeviceGrpIfaceModel10ObjSetImpl_GPU_GPC_TSOSC)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermDeviceGrpIfaceModel10ObjSetImpl_GPU_GPC_TSOSC");

/* ------------------------- Include Derived Types ------------------------- */

#endif // THERMDEVICE_GPU_GPC_TSOSC_H
