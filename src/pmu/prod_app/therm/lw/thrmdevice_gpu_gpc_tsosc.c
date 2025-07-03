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
 * @file  thrmdevice_gpu_gpc_tsosc.c
 * @brief THERM Thermal Device Specific to GPU_GPC_TSOSC device class
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/thrmdevice.h"
#include "therm/objtherm.h"

/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a THERM_DEVICE_GPU_GPC_TSOSC object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermDeviceGrpIfaceModel10ObjSetImpl_GPU_GPC_TSOSC
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_DEVICE_GPU_GPC_TSOSC_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_DEVICE_GPU_GPC_TSOSC_BOARDOBJ_SET *)pBoardObjDesc;
    THERM_DEVICE_GPU_GPC_TSOSC   *pDevice;
    FLCN_STATUS                   status;

    // Construct and initialize parent-class object.
    status = thermDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDevice = (THERM_DEVICE_GPU_GPC_TSOSC *)*ppBoardObj;

    // Set member varaibles.
    pDevice->gpcTsoscIdx = pSet->gpcTsoscIdx;

    return status;
}

/*!
 * @copydoc ThermDeviceTempValueGet
 */
FLCN_STATUS
thermDeviceTempValueGet_GPU_GPC_TSOSC
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    THERM_DEVICE_GPU_GPC_TSOSC *pDevice = (THERM_DEVICE_GPU_GPC_TSOSC *)pDev;

    *pLwTemp = thermGetGpuGpcTsoscTemp_HAL(&Therm, thermDevProvIdx, pDevice->gpcTsoscIdx);

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
