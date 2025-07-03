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
 * @file  thrmchannel_device.c
 * @brief THERM_CHANNEL specific to Device class.
 *
 * This module is a collection of functions managing and manipulating state
 * related to Device class in the Thermal Channel Table
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/thrmdevice.h"
#include "therm/thrmchannel.h"
#include "therm/objtherm.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
thermChannelGrpIfaceModel10ObjSetImpl_DEVICE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_CHANNEL_DEVICE_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_CHANNEL_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS             status;
    THERM_CHANNEL_DEVICE   *pChannelDevice;
    THERM_DEVICE           *pThermDev;

    // Construct and initialize parent-class object.
    status = thermChannelGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto thermChannelGrpIfaceModel10ObjSetImpl_DEVICE_exit;
    }
    pChannelDevice = (THERM_CHANNEL_DEVICE *)*ppBoardObj;

    pThermDev = THERM_DEVICE_GET(pSet->thermDevIdx);
    if (pThermDev == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto thermChannelGrpIfaceModel10ObjSetImpl_DEVICE_exit;
    }

    // Set member variables.
    pChannelDevice->pThermDev       = pThermDev;
    pChannelDevice->thermDevProvIdx = pSet->thermDevProvIdx;

thermChannelGrpIfaceModel10ObjSetImpl_DEVICE_exit:
    return status;
}

/*!
 * @copydoc ThermChannelTempValueGet()
 */
FLCN_STATUS
thermChannelTempValueGet_DEVICE
(
    THERM_CHANNEL  *pChannel,
    LwTemp         *pLwTemp
)
{
    THERM_CHANNEL_DEVICE *pChDev = (THERM_CHANNEL_DEVICE *)pChannel;
    THERM_DEVICE         *pDev   = pChDev->pThermDev;
    FLCN_STATUS           status = FLCN_OK;

    // Call THERM_DEVICE's TEMP_VALUE_GET interface if TempSim not enabled.
    if (pChannel->tempSim.bSupported && pChannel->tempSim.bEnabled)
    {
        *pLwTemp = pChannel->tempSim.targetTemp;
    }
    else
    {
        PMU_HALT_OK_OR_GOTO(status,
            thermDeviceTempValueGet(pDev, pChDev->thermDevProvIdx, pLwTemp),
            thermChannelTempValueGet_DEVICE_exit);
    }

    // Apply temperature scaling and SW offset. Rounding to F(24.8) from F(16.16).
    *pLwTemp =
        LW_TYPES_SFXP_X_Y_TO_S32_ROUNDED(24, 8, (*pLwTemp) * pChDev->super.scaling) +
        pChDev->super.offsetSw;

    // Bound the temperature.
    *pLwTemp = LW_MAX(*pLwTemp, pChDev->super.lwTempMin);
    *pLwTemp = LW_MIN(*pLwTemp, pChDev->super.lwTempMax);

thermChannelTempValueGet_DEVICE_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
