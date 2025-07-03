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
 * @file    illum_device.c
 * @copydoc illum_device.h
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_device.h"
#include "pmu_objpmgr.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetEntry    (s_illumDeviceIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibIllumConstruct", "s_illumDeviceIfaceModel10SetEntry");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
illumDeviceBoardObjGrpIfaceModel10Set
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
        status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
            ILLUM_DEVICE,                               // _class
            pBuffer,                                    // _pBuffer
            boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
            s_illumDeviceIfaceModel10SetEntry,                // _entryFunc
            turingAndLater.illum.illumDeviceGrpSet,     // _ssElement
            OVL_INDEX_DMEM(illum),                      // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
        if (status != FLCN_OK)
        {
            // Uncomment below code once PMU_TRUE_BP() is enabled via bug 200717054.
            // PMU_TRUE_BP();
            goto illumDeviceBoardObjGrpIfaceModel10Set_exit;
        }

illumDeviceBoardObjGrpIfaceModel10Set_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
illumDeviceGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_ILLUM_DEVICE *pDescIllumDevice =
        (RM_PMU_PMGR_ILLUM_DEVICE *)pBoardObjDesc;
    LwBool                    bFirstConstruct  =
        (*ppBoardObj == NULL);
    ILLUM_DEVICE   *pIllumDevice;
    FLCN_STATUS     status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pIllumDevice = (ILLUM_DEVICE *)*ppBoardObj;

    // Set member variables.
    pIllumDevice->ctrlModeMask   = pDescIllumDevice->ctrlModeMask;
    pIllumDevice->syncData.bSync = pDescIllumDevice->syncData.bSync;
    pIllumDevice->syncData.timeStampms = pDescIllumDevice->syncData.timeStampms;

    if (!bFirstConstruct && pIllumDevice->syncData.bSync)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
             OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibIllum)
             OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libi2c)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = illumDeviceSync(pIllumDevice, &pIllumDevice->syncData);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            // Uncomment below code once PMU_TRUE_BP() is enabled via bug 200717054.
            // PMU_TRUE_BP();
            goto illumZoneGrpIfaceModel10ObjSetImpl_RGB_exit;
        }
    }

illumZoneGrpIfaceModel10ObjSetImpl_RGB_exit:
    return status;
}

/*!
 * @copydoc IllumDeviceDataSet
 */
FLCN_STATUS
illumDeviceDataSet
(
    ILLUM_DEVICE   *pIllumDevice,
    ILLUM_ZONE     *pIllumZone
)
{
    switch (BOARDOBJ_GET_TYPE(pIllumDevice))
    {
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_MLWV10:
            return illumDeviceDataSet_MLWV10(pIllumDevice, pIllumZone);
#if PMUCFG_FEATURE_ENABLED(PMU_ILLUM_DEVICE_GPIO)
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_GPIO_PWM_SINGLE_COLOR_V10:
            return illumDeviceDataSet_GPIO_PWM_SINGLE_COLOR_V10(pIllumDevice, pIllumZone);
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_GPIO_PWM_RGBW_V10:
            return illumDeviceDataSet_GPIO_PWM_RGBW_V10(pIllumDevice, pIllumZone);
#endif
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc IllumDeviceSync
 */
FLCN_STATUS
illumDeviceSync
(
    ILLUM_DEVICE             *pIllumDevice,
    RM_PMU_ILLUM_DEVICE_SYNC *pSyncData
)
{
    switch (BOARDOBJ_GET_TYPE(pIllumDevice))
    {
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_MLWV10:
            return illumDeviceSync_MLWV10(pIllumDevice, pSyncData);
#if PMUCFG_FEATURE_ENABLED(PMU_ILLUM_DEVICE_GPIO)
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_GPIO_PWM_SINGLE_COLOR_V10:
            return illumDeviceSync_GPIO_PWM_SINGLE_COLOR_V10(pIllumDevice, pSyncData);
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_GPIO_PWM_RGBW_V10:
            return illumDeviceSync_GPIO_PWM_RGBW_V10(pIllumDevice, pSyncData);
#endif
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_illumDeviceIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_MLWV10:
            return illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10(pModel10, ppBoardObj,
                        sizeof(ILLUM_DEVICE_MLWV10), pBuf);
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_GPIO_PWM_SINGLE_COLOR_V10:
            return illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_SINGLE_COLOR_V10(pModel10, ppBoardObj,
                        sizeof(ILLUM_DEVICE_GPIO_PWM_SINGLE_COLOR_V10), pBuf);
        case LW2080_CTRL_PMGR_ILLUM_DEVICE_TYPE_GPIO_PWM_RGBW_V10:
            return illumDeviceGrpIfaceModel10ObjSetImpl_GPIO_PWM_RGBW_V10(pModel10, ppBoardObj,
                        sizeof(ILLUM_DEVICE_GPIO_PWM_RGBW_V10), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
