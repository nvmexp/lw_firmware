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
 * @file  fancooler_active.c
 * @brief FAN Fan Cooler Specific to ACTIVE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/fancooler.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a FAN_COOLER_ACTIVE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_FAN_FAN_COOLER_ACTIVE_BOARDOBJ_SET *pCoolerASet =
        (RM_PMU_FAN_FAN_COOLER_ACTIVE_BOARDOBJ_SET *)pBoardObjDesc;
    FAN_COOLER_ACTIVE                         *pCoolerA;
    FLCN_STATUS                                status;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if ((pCoolerASet->rpmMin > pCoolerASet->rpmMax) || // Sanity check acoustic minimum and maximum RPM.
            (pCoolerASet->bTachSupported && (pCoolerASet->rpmMax == 0)) || // Sanity check max RPM.
            (!pCoolerASet->bTachSupported && (pCoolerASet->rpmMax != 0)) || // Sanity check max RPM.
            (pCoolerASet->bTachSupported && (pCoolerASet->tachRate == 0))) // Sanity check tach rate.
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_exit;
        }
    }

    // Construct and initialize parent-class object.
    status = fanCoolerGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_exit;
    }
    pCoolerA = (FAN_COOLER_ACTIVE *)*ppBoardObj;

    // Set member variables.
    pCoolerA->bTachSupported = pCoolerASet->bTachSupported;
    pCoolerA->tachRate       = pCoolerASet->tachRate;
    pCoolerA->tachPin        = pCoolerASet->tachPin;
    pCoolerA->rpmMin         = pCoolerASet->rpmMin;
    pCoolerA->rpmMax         = pCoolerASet->rpmMax;
    if (pCoolerA->bTachSupported)
    {
        pCoolerA->tachSource =
            pmgrMapTachPinToTachSource_HAL(&Pmgr, pCoolerA->tachPin);
        if (pCoolerA->tachSource == PMU_PMGR_TACH_SOURCE_ILWALID)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_exit;
        }
    }

fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_exit:
    return status;
}

/*!
 * Class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanCoolerActiveIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_FAN_FAN_COOLER_ACTIVE_BOARDOBJ_GET_STATUS
                   *pCoolerStatus;
    FAN_COOLER     *pCooler;
    LwS32           rpmLwrrent;
    FLCN_STATUS     status;

    // Call super-class implementation.
    status  = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pCooler       = (FAN_COOLER *)pBoardObj;
    pCoolerStatus = (RM_PMU_FAN_FAN_COOLER_ACTIVE_BOARDOBJ_GET_STATUS *)pPayload;

    //
    // Retrieve current RPM.
    // Bypass rpmGet() in case of 2-wire fans, as RPM is not
    // supported on them.
    //
    status = fanCoolerRpmGet(pCooler, &rpmLwrrent);
    if ((status != FLCN_OK) &&
        (FLCN_ERR_NOT_SUPPORTED != status))
    {
        return status;
    }

    // Set current RPM.
    pCoolerStatus->rpmLwrr = (LwU32)rpmLwrrent;

    return FLCN_OK;
}

/*!
 * Class implementation.
 *
 * @copydoc FanCoolerLoad
 */
FLCN_STATUS
fanCoolerActiveLoad
(
    FAN_COOLER *pCooler
)
{
    FAN_COOLER_ACTIVE  *pCoolerA = (FAN_COOLER_ACTIVE *)pCooler;
    FLCN_STATUS         status   = FLCN_OK;

    // If tachometer is supported, load it.
    if (pCoolerA->bTachSupported)
    {
        PMU_HALT_OK_OR_GOTO(status,
            pmgrTachLoad_HAL(&Pmgr, pCoolerA->tachSource, pCoolerA->tachPin),
            fanCoolerActiveLoad_exit);

        PMU_HALT_OK_OR_GOTO(status,
            thermTachLoad_HAL(&Therm, pCoolerA->tachSource, pCoolerA->tachRate),
            fanCoolerActiveLoad_exit);
    }

fanCoolerActiveLoad_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Class implementation.
 *
 * @copydoc FanCoolerRpmGet
 */
FLCN_STATUS
fanCoolerActiveRpmGet
(
    FAN_COOLER *pCooler,
    LwS32      *pRpm
)
{
    FAN_COOLER_ACTIVE  *pCool = (FAN_COOLER_ACTIVE *)pCooler;
    LwU32               numerator;
    LwU32               denominator;

    *pRpm = 0;

    if (!pCool->bTachSupported)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    if (pmgrGetTachFreq_HAL(&Pmgr, pCool->tachSource,
                            &numerator, &denominator) == FLCN_OK)
    {
        // Return default tach reading if its not possible to do the math.
        if (denominator != 0)
        {
            // Perform math to colwert edge count per sec to RPM.
            *pRpm = LW_UNSIGNED_ROUNDED_DIV(60 * numerator,
                        denominator * pCool->tachRate);
        }
    }

    return FLCN_OK;
}
