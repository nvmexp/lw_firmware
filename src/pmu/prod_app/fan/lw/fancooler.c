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
 * @file  fancooler.c
 * @brief FAN Fan Cooler Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to fan coolers in the Fan Cooler Table
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "fan/objfan.h"
#include "fan/fancooler.h"
#include "fan/fancooler_pmumon.h"
#include "dbgprintf.h"
#include "main.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * FAN_POLICY handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
fanCoolersBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        FAN_COOLER,                                 // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        fanFanCoolerIfaceModel10SetEntry,                 // _entryFunc
        all.fan.fanCoolerGrpSet,                    // _ssElement
        OVL_INDEX_DMEM(therm),                      // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
fanCoolerGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    return boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
}

/*!
 * Load all FAN_COOLERS.
 */
FLCN_STATUS
fanCoolersLoad(void)
{
    FAN_COOLER   *pCooler;
    FLCN_STATUS   status = FLCN_OK;
    LwBoardObjIdx i;

    BOARDOBJGRP_ITERATOR_BEGIN(FAN_COOLER, pCooler, i, NULL)
    {
        status = fanCoolerLoad(pCooler);
        if (status != FLCN_OK)
        {
            goto fanCoolersLoad_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_PMUMON))
    {
        status = fanCoolerPmumonLoad();
    }

fanCoolersLoad_exit:
    return status;
}

/*!
 * Queries all FAN_COOLERS.
 */
FLCN_STATUS
fanCoolersBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32, // _grpType
        FAN_COOLER,                             // _class
        pBuffer,                                // _pBuffer
        NULL,                                   // _hdrFunc
        fanFanCoolerIfaceModel10GetStatus,                  // _entryFunc
        all.fan.fanCoolerGrpGetStatus);         // _ssElement
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
fanFanCoolerIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM);
            return fanCoolerActivePwmIfaceModel10GetStatus(pModel10, pBuf);

        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR);
            return fanCoolerActivePwmTachCorrIfaceModel10GetStatus(pModel10, pBuf);
    }
    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}


/* ------------------------- Private Functions ------------------------------ */

/*!
 * @copydoc FanCoolerLoad
 */
FLCN_STATUS
fanCoolerLoad
(
    FAN_COOLER *pCooler
)
{
    switch (BOARDOBJ_GET_TYPE(pCooler))
    {
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM);
            return fanCoolerActivePwmLoad(pCooler);

        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR);
            return fanCoolerActivePwmTachCorrLoad(pCooler);
    }

    return FLCN_OK;
}

/*!
 * @copydoc FanCoolerRpmGet
 */
FLCN_STATUS
fanCoolerRpmGet
(
    FAN_COOLER *pCooler,
    LwS32      *pRpm
)
{
    switch (BOARDOBJ_GET_TYPE(pCooler))
    {
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM);
            return fanCoolerActivePwmRpmGet(pCooler, pRpm);

        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR);
            return fanCoolerActivePwmTachCorrRpmGet(pCooler, pRpm);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc FanCoolerRpmSet
 */
FLCN_STATUS
fanCoolerRpmSet
(
    FAN_COOLER *pCooler,
    LwS32       rpm
)
{
    switch (BOARDOBJ_GET_TYPE(pCooler))
    {
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR);
            return fanCoolerActivePwmTachCorrRpmSet(pCooler, rpm);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc FanCoolerPwmGet
 */
FLCN_STATUS
fanCoolerPwmGet
(
    FAN_COOLER     *pCooler,
    LwBool          bActual,
    LwSFXP16_16    *pPwm
)
{
    switch (BOARDOBJ_GET_TYPE(pCooler))
    {
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM);
            return fanCoolerActivePwmPwmGet(pCooler, bActual, pPwm);
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR);
            return fanCoolerActivePwmTachCorrPwmGet(pCooler, bActual, pPwm);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc FanCoolerPwmSet
 */
FLCN_STATUS
fanCoolerPwmSet
(
    FAN_COOLER *pCooler,
    LwBool      bBound,
    LwSFXP16_16 pwm
)
{
    switch (BOARDOBJ_GET_TYPE(pCooler))
    {
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM);
            return fanCoolerActivePwmPwmSet(pCooler, bBound, pwm);

        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR);
            return fanCoolerActivePwmTachCorrPwmSet(pCooler, bBound, pwm);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
fanFanCoolerIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM);
            return fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM(pModel10, ppBoardObj,
                sizeof(FAN_COOLER_ACTIVE_PWM), pBuf);

        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_THERM_FAN_COOLER_ACTIVE_PWM_TACH_CORR);
            return fanCoolerGrpIfaceModel10ObjSetImpl_ACTIVE_PWM_TACH_CORR(pModel10, ppBoardObj,
                sizeof(FAN_COOLER_ACTIVE_PWM_TACH_CORR), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
