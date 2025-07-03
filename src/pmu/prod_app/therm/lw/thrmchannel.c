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
 * @file  thrmchannel.c
 * @brief Thermal Channel Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to Thermal Channel in the Thermal Channel Table
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
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
 * @brief THERM_CHANNEL handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
thermChannelBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        THERM_CHANNEL,                              // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        thermThermChannelIfaceModel10SetEntry,            // _entryFunc
        all.therm.thermChannelGrpSet,               // _ssElement
        OVL_INDEX_DMEM(therm),                      // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * @brief Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
thermChannelGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_THERM_THERM_CHANNEL_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_CHANNEL_BOARDOBJ_SET *)pBoardObjDesc;
    THERM_CHANNEL        *pChannel = NULL;
    FLCN_STATUS           status   = FLCN_OK;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pChannel = (THERM_CHANNEL *)*ppBoardObj;

    // Set member variables.
    pChannel->scaling   = pSet->scaling;
    pChannel->offsetSw  = pSet->offsetSw;
    pChannel->lwTempMin = pSet->lwTempMin;
    pChannel->lwTempMax = pSet->lwTempMax;
    pChannel->tempSim.bSupported = pSet->tempSim.bSupported;
    pChannel->tempSim.bEnabled   = pSet->tempSim.bEnabled;
    pChannel->tempSim.targetTemp = pSet->tempSim.targetTemp;

    return status;
}

/*!
 * @brief THERM_CHANNEL handler for the RM_PMU_BOARDOBJ_CMD_GRP_GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
thermChannelBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32, // _grpType
        THERM_CHANNEL,                          // _class
        pBuffer,                                // _pBuffer
        NULL,                                   // _hdrFunc
        thermThermChannelIfaceModel10GetStatus,             // _entryFunc
        all.therm.thermChannelGrpGetStatus);    // _ssElement
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
thermThermChannelIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_DEVICE))
            {
                status = thermChannelIfaceModel10GetStatus_DEVICE(pModel10, pBuf);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ThermChannelTempValueGet()
 */
FLCN_STATUS
thermChannelTempValueGet
(
    THERM_CHANNEL  *pChannel,
    LwTemp         *pLwTemp
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;
    
    if (pChannel == NULL || pLwTemp == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto thermChannelTempValueGet_exit;
    }

    switch ((BOARDOBJ_GET_TYPE(pChannel)))
    {
        case LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_DEVICE))
            {
                status = thermChannelTempValueGet_DEVICE(pChannel, pLwTemp);
            }
            break;
        }

        // All THERM_CHANNEL implementations must override this interface.
        default:
        {
            // To pacify coverity.
            break;
        }
    }

thermChannelTempValueGet_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry()
 */
FLCN_STATUS
thermThermChannelIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_THERMAL_THERM_CHANNEL_CLASS_DEVICE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_DEVICE))
            {
                status = thermChannelGrpIfaceModel10ObjSetImpl_DEVICE(pModel10, ppBoardObj,
                            sizeof(THERM_CHANNEL_DEVICE), pBuf);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Super-class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
thermChannelIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_THERM_THERM_CHANNEL_BOARDOBJ_GET_STATUS *pGetStatus;
    THERM_CHANNEL                                  *pChannel;
    LwTemp                                          lwrrentTemp;
    FLCN_STATUS                                     status;

    // Call super-class implementation.
    PMU_HALT_OK_OR_GOTO(status,
        boardObjIfaceModel10GetStatus(pModel10, pPayload),
        thermChannelIfaceModel10GetStatus_SUPER_exit);

    pChannel   = (THERM_CHANNEL *)pBoardObj;
    pGetStatus = (RM_PMU_THERM_THERM_CHANNEL_BOARDOBJ_GET_STATUS *)pPayload;

    PMU_HALT_OK_OR_GOTO(status,
        thermChannelTempValueGet(pChannel, &lwrrentTemp),
        thermChannelIfaceModel10GetStatus_SUPER_exit);

    // Set THERM_CHANNEL query parameters.
    pGetStatus->lwrrentTemp = lwrrentTemp;

thermChannelIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
