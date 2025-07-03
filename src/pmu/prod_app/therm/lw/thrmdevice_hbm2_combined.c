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
 * @file  thrmdevice_hbm2_combined.c
 * @brief THERM Thermal Device Specific to HBM2_COMBINED device class
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/thrmdevice.h"
#include "therm/objtherm.h"
#include "pmu_objfb.h"
#include "dmemovl.h"
#include "dev_fbpa_addendum.h"

/* ------------------------- Static Function Prototypes --------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a THERM_DEVICE_HBM2_COMBINED object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_COMBINED
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    THERM_DEVICE_HBM2_COMBINED   *pDevice;
    FLCN_STATUS                   status;
    LwU8                          i;

    // Construct and initialize parent-class object.
    status = thermDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDevice = (THERM_DEVICE_HBM2_COMBINED *)*ppBoardObj;

    // Set member variables.
    pDevice->numSites = fbHBMSiteCountGet_HAL(&Fb);
    if (pDevice->numSites == 0)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }
    pDevice->pFbpaIdx = lwosCallocType(pBObjGrp->ovlIdxDmem, pDevice->numSites, LwU8);
    if (pDevice->pFbpaIdx == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }
    for (i = 0; i < pDevice->numSites; i++)
    {
        pDevice->pFbpaIdx[i] = fbHBMSiteNonFloorsweptFBPAIdxGet_HAL(&Fb, i);
    }

    return status;
}

/*!
 * @copydoc ThermDeviceTempValueGet
 */
FLCN_STATUS
thermDeviceTempValueGet_HBM2_COMBINED
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    THERM_DEVICE_HBM2_COMBINED *pDevice = (THERM_DEVICE_HBM2_COMBINED *)pDev;
    LwTemp                      resTemp = RM_PMU_LW_TEMP_0_C;
    FLCN_STATUS                 status  = FLCN_OK;

    switch (thermDevProvIdx)
    {
        case LW2080_CTRL_THERMAL_THERM_DEVICE_HBM2_COMBINED_PROV_MAX:
        {
            LwTemp lwrrSiteTemp = RM_PMU_LW_TEMP_0_C;
            LwU8   siteIdx;

            for (siteIdx = 0; siteIdx < pDevice->numSites; siteIdx++)
            {
                PMU_HALT_OK_OR_GOTO(status,
                    fbHBMSiteTempGet_HAL(&Fb, pDevice->pFbpaIdx[siteIdx], thermDevProvIdx, &lwrrSiteTemp),
                    thermDeviceTempValueGet_HBM2_COMBINED_exit);

                resTemp = LW_MAX(lwrrSiteTemp, resTemp);
            }
            break;
        }
    }

    *pLwTemp = resTemp;

thermDeviceTempValueGet_HBM2_COMBINED_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
