/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  thrmdevice_gpu_gpc_combined.c
 * @brief THERM Thermal Device Specific to GPU_GPC_COMBINED device class.
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
 * Return the internal temperature for GPU_GPC_COMBINED device class.
 * @copydoc ThermDeviceTempValueGet()
 */
FLCN_STATUS
thermDeviceTempValueGet_GPU_GPC_COMBINED
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    return thermGetGpuGpcCombinedTemp_HAL(&Therm, thermDevProvIdx, pLwTemp);
}

/* ------------------------- Private Functions ------------------------------ */
