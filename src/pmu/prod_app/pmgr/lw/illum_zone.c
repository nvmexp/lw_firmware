/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    illum_zone.c
 * @copydoc illum_zone.h
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_zone.h"
#include "pmu_objpmgr.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetEntry    (s_illumZoneIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllumConstruct", "s_illumZoneIfaceModel10SetEntry");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
illumZoneBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_ILLUM_CONSTRUCT
    };
    FLCN_STATUS     status;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
            ILLUM_ZONE,                                 // _class
            pBuffer,                                    // _pBuffer
            boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
            s_illumZoneIfaceModel10SetEntry,                  // _entryFunc
            turingAndLater.illum.illumZoneGrpSet,       // _ssElement
            OVL_INDEX_DMEM(illum),                      // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
        if (status != FLCN_OK)
        {
            // Uncomment below code once PMU_TRUE_BP() is enabled via bug 200717054.
            // PMU_TRUE_BP();
            goto illumZoneBoardObjGrpIfaceModel10Set_exit;
        }

illumZoneBoardObjGrpIfaceModel10Set_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
illumZoneGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_ILLUM_ZONE *pDescIllumZone =
        (RM_PMU_PMGR_ILLUM_ZONE *)pBoardObjDesc;
    ILLUM_ZONE     *pIllumZone;
    ILLUM_DEVICE   *pDevice;
    FLCN_STATUS     status;

    pDevice = ILLUM_DEVICE_GET(pDescIllumZone->illumDeviceIdx);
    if (pDevice == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto illumZoneGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto illumZoneGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pIllumZone = (ILLUM_ZONE *)*ppBoardObj;

    // Set member variables.
    pIllumZone->provIdx         = pDescIllumZone->provIdx;
    pIllumZone->ctrlMode        = pDescIllumZone->ctrlMode;
    pIllumZone->pIllumDevice    = pDevice;

illumZoneGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_illumZoneIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_ILLUM_ZONE_TYPE_RGB:
        case LW2080_CTRL_PMGR_ILLUM_ZONE_TYPE_RGB_FIXED:
            return illumZoneGrpIfaceModel10ObjSetImpl_RGB(pModel10, ppBoardObj,
                        sizeof(ILLUM_ZONE_RGB), pBuf);
        case LW2080_CTRL_PMGR_ILLUM_ZONE_TYPE_RGBW:
            return illumZoneGrpIfaceModel10ObjSetImpl_RGBW(pModel10, ppBoardObj,
                        sizeof(ILLUM_ZONE_RGBW), pBuf);
        case LW2080_CTRL_PMGR_ILLUM_ZONE_TYPE_SINGLE_COLOR:
            return illumZoneGrpIfaceModel10ObjSetImpl_SINGLE_COLOR(pModel10, ppBoardObj,
                        sizeof(ILLUM_ZONE_SINGLE_COLOR), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
