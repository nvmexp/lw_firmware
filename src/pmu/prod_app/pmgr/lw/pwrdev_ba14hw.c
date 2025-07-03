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
 * @file    pwrdev_ba14hw.c
 * @brief   Block Activity v1.4 HW sensor/controller.
 *
 * @implements PWR_DEVICE
 *
 * @note    Only supports/implements single PWR_DEVICE Provider - index 0.
 *          Provider index parameter will be ignored in all interfaces.
 *
 * @note    Is a subset of BA1XHW device, and used for energy alwmmulation alone.
 *
 * @note    For information on BA1XHW device see RM::pwrdev_ba1xhw.c
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba1xhw.h"
#include "pmgr/pwrdev_ba14hw.h"
#include "therm/objtherm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
/*!
 * Construct a BA14HW PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_BA14HW
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    PWR_DEVICE_BA14HW                               *pBa14Hw            =
        (PWR_DEVICE_BA14HW *)*ppBoardObj;
    RM_PMU_PMGR_PWR_DEVICE_DESC_BA1XHW_BOARDOBJ_SET *pBa1xHwSet         =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_BA1XHW_BOARDOBJ_SET *)pBoardObjSet;
    FLCN_STATUS                                      status;
    LwBool                                           bFirstConstruct    =
        (*ppBoardObj == NULL);

    // 
    // BA14HW is required to work in power in order
    // to accumulate energy in the DEBUG register.
    //
    if (pBa1xHwSet->bLwrrent)
    {
        PMU_HALT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA14HW_exit;
    }
    status = pwrDevIfaceModel10SetImpl_BA13HW(pModel10, ppBoardObj,
        size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA14HW_exit;
    }

    //
    // Reset the energy counter only on the very first
    // time the device get constructed.
    //
    if (bFirstConstruct)
    {
        pBa14Hw->energymJ  = 0;
    }

pwrDevGrpIfaceModel10ObjSetImpl_BA14HW_exit:
    return status;
}

/*!
 * BA14HW power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_BA14HW
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS         status;
    PWR_DEVICE_BA14HW  *pBa14Hw = (PWR_DEVICE_BA14HW *)pDev;
    LwU64               lwrrEnergymJ;
    LwU64               totalEnergymJ;

    status = pwrDevTupleGet_BA13HW(pDev, provIdx, pTuple);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto pwrDevTupleGet_BA14HW_exit;
    }
    status = thermPwrDevBaGetEnergyReading_HAL(&Therm, pDev, provIdx, &lwrrEnergymJ);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto pwrDevTupleGet_BA14HW_exit;
    }

    // Accumulate the energy at the device level
    lw64Add(&totalEnergymJ, &(pBa14Hw->energymJ), &lwrrEnergymJ);

    // Cache the energy value at device level.
    pBa14Hw->energymJ = totalEnergymJ;
    LwU64_ALIGN32_PACK(&(pTuple->energymJ), &totalEnergymJ);

pwrDevTupleGet_BA14HW_exit:
    return status;
}
