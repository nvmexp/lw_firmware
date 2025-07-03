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
 * @file  thrmdevice_hbm2_site.c
 * @brief THERM Thermal Device Specific to HBM2_SITE device class
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/thrmdevice.h"
#include "therm/objtherm.h"
#include "pmu_objfb.h"

/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a THERM_DEVICE_HBM2_SITE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_SITE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_DEVICE_HBM2_SITE_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_DEVICE_HBM2_SITE_BOARDOBJ_SET *)pBoardObjDesc;
    THERM_DEVICE_HBM2_SITE   *pDevice;
    FLCN_STATUS               status;

    // Construct and initialize parent-class object.
    status = thermDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDevice = (THERM_DEVICE_HBM2_SITE *)*ppBoardObj;

    // Set member variables.
    pDevice->siteIdx = pSet->siteIdx;
    pDevice->fbpaIdx = fbHBMSiteNonFloorsweptFBPAIdxGet_HAL(&Therm, pDevice->siteIdx);

    return status;
}

/*!
 * @copydoc ThermDeviceTempValueGet
 */
FLCN_STATUS
thermDeviceTempValueGet_HBM2_SITE
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    THERM_DEVICE_HBM2_SITE *pDevice = (THERM_DEVICE_HBM2_SITE *)pDev;

    return fbHBMSiteTempGet_HAL(&Therm, pDevice->fbpaIdx, thermDevProvIdx, pLwTemp);
}

/* ------------------------- Private Functions ------------------------------ */
