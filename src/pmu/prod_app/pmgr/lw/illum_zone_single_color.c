/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    illum_zone_single_color.c
 * @copydoc illum_zone_single_color.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_zone_single_color.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_ILLUM_ZONE_SINGLE_COLOR *pDescIllumZoneSingleColor =
        (RM_PMU_PMGR_ILLUM_ZONE_SINGLE_COLOR *)pBoardObjDesc;
    LwBool              bFirstConstruct           =
        (*ppBoardObj == NULL);
    ILLUM_ZONE_SINGLE_COLOR     *pIllumZoneSingleColor;
    FLCN_STATUS         status;

    status = illumZoneGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR_exit;
    }
    pIllumZoneSingleColor = (ILLUM_ZONE_SINGLE_COLOR *)*ppBoardObj;

    // Set member variables.
    pIllumZoneSingleColor->ctrlModeParams = pDescIllumZoneSingleColor->ctrlModeParams;

    if (!bFirstConstruct)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
             OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibIllum)
             OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libi2c)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = illumDeviceDataSet(pIllumZoneSingleColor->super.pIllumDevice, &pIllumZoneSingleColor->super);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR_exit;
        }
    }

illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
