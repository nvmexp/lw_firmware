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
 * @file thrmdevice_gpu_sci.h
 * @brief @copydoc thrmdevice_gpu_sci.c
 */

#ifndef THERMDEVICE_GPU_SCI_H
#define THERMDEVICE_GPU_SCI_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"

/* ------------------------- Macros ---------------------------------------- */
#define thermDeviceGrpIfaceModel10ObjSetImpl_GPU_SCI(pModel10, ppBoardObj, size, pDesc)   \
            thermDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pDesc)

/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_DEVICE_GPU_SCI THERM_DEVICE_GPU_SCI;

/*!
 * Extends THERM_DEVICE providing attributes specific to GPU_SCI device.
 */
struct THERM_DEVICE_GPU_SCI
{
    /*!
     * THERM_DEVICE super class. This should always be the first member!
     */
    THERM_DEVICE    super;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
ThermDeviceTempValueGet (thermDeviceTempValueGet_GPU_SCI)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermDeviceTempValueGet_GPU_SCI");

/* ------------------------- Include Derived Types ------------------------- */

#endif // THERMDEVICE_GPU_SCI_H
