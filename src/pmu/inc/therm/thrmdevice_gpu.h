/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmdevice_gpu.h
 * @brief @copydoc thrmdevice_gpu.c
 */

#ifndef THERMDEVICE_GPU_H
#define THERMDEVICE_GPU_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @brief Wrapper interface for @ref thermDeviceGrpIfaceModel10ObjSetImpl_SUPER()
 */
#define thermDeviceGrpIfaceModel10ObjSetImpl_GPU(pModel10, ppBoardObj, size, pDesc)   \
            thermDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pDesc)

/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_GPU THERM_DEVICE_GPU;

/*!
 * @brief Extends THERM_DEVICE providing attributes specific to GPU device.
 * 
 * @extends THERM_DEVICE
 */
struct THERM_DEVICE_GPU
{
    /*!
     * @brief THERM_DEVICE super class. This should always be the first member!
     */
    THERM_DEVICE    super;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
ThermDeviceTempValueGet (thermDeviceTempValueGet_GPU)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet_GPU");

/* ------------------------ Include Derived Types -------------------------- */

#endif // THERMDEVICE_GPU_H
