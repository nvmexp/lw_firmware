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
 * @file  thrmdevice.c
 * @brief Thermal Device Model
 *
 * This module is a collection of functions managing and manipulating state
 * related to Thermal Device in the Thermal Device Table
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
 * @brief THERM_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler()
 */
FLCN_STATUS
thermDeviceBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        THERM_DEVICE,                               // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        thermThermDeviceIfaceModel10SetEntry,             // _entryFunc
        all.therm.thermDeviceGrpSet,                // _ssElement
        OVL_INDEX_DMEM(therm),                      // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * @brief Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
thermDeviceGrpIfaceModel10ObjSetImpl_SUPER
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
 * @copydoc ThermDeviceTempValueGet()
 */
FLCN_STATUS
thermDeviceTempValueGet_IMPL
(
    THERM_DEVICE  *pDev,
    LwU8           thermDevProvIdx,
    LwTemp        *pLwTemp
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pDev))
    {
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU))
            {
                status = thermDeviceTempValueGet_GPU(pDev, thermDevProvIdx,
                            pLwTemp);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU_GPC_TSOSC))
            {
                status = thermDeviceTempValueGet_GPU_GPC_TSOSC(pDev,
                            thermDevProvIdx, pLwTemp);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_SCI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU_SCI))
            {
                status = thermDeviceTempValueGet_GPU_SCI(pDev, thermDevProvIdx,
                            pLwTemp);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_COMBINED:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU_GPC_COMBINED))
            {
                status = thermDeviceTempValueGet_GPU_GPC_COMBINED(pDev,
                            thermDevProvIdx, pLwTemp);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GDDR6_X_COMBINED:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GDDR6_X_COMBINED))
            {
                status = thermDeviceTempValueGet_GDDR6_X_COMBINED(pDev,
                            thermDevProvIdx, pLwTemp);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_I2C_TMP411))
            {
                status = thermDeviceTempValueGet_I2C_TMP411(pDev,
                            thermDevProvIdx, pLwTemp);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_HBM2_SITE))
            {
                status = thermDeviceTempValueGet_HBM2_SITE(pDev,
                            thermDevProvIdx, pLwTemp);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_COMBINED:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_HBM2_COMBINED))
            {
                status = thermDeviceTempValueGet_HBM2_COMBINED(pDev,
                            thermDevProvIdx, pLwTemp);
            }
            break;
        }

        default:
        {
            // To pacify coverity.
            break;
        }
    }

    // All THERM_DEVICE implementations must override this interface.
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
thermThermDeviceIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_GPU(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_GPU), pBuf);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU_GPC_TSOSC))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_GPU_GPC_TSOSC(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_GPU_GPC_TSOSC), pBuf);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_SCI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU_SCI))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_GPU_SCI(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_GPU_SCI), pBuf);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_COMBINED:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GPU_GPC_COMBINED))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_GPU_GPC_COMBINED(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_GPU_GPC_COMBINED), pBuf);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GDDR6_X_COMBINED:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GDDR6_X_COMBINED))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_GDDR6_X_COMBINED(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_GDDR6_X_COMBINED), pBuf);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649:
        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_I2C_TMP411))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_I2C_TMP411(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_I2C_TMP411), pBuf);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_HBM2_SITE))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_SITE(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_HBM2_SITE), pBuf);
            }
            break;
        }

        case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_COMBINED:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_HBM2_COMBINED))
            {
                status = thermDeviceGrpIfaceModel10ObjSetImpl_HBM2_COMBINED(pModel10, ppBoardObj,
                            sizeof(THERM_DEVICE_HBM2_COMBINED), pBuf);
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
 * @brief  Loads all THERM_DEVICE SW state.
 *
 * @return FLCN_OK
 *         All THERM_DEVICE SW state loaded successfully.
 */
FLCN_STATUS
thermDevicesLoad(void)
{
    FLCN_STATUS     status = FLCN_OK;
    THERM_DEVICE   *pDev;
    LwBoardObjIdx   i;

    // If there are no valid THERM_DEVICEs, do nothing.
    if (boardObjGrpMaskIsZero(&(Therm.devices.objMask)))
    {
        goto thermDevicesLoad_exit;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(THERM_DEVICE, pDev, i, NULL)
    {
        switch (BOARDOBJ_GET_TYPE(pDev))
        {
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_GDDR6_X_COMBINED)
            case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GDDR6_X_COMBINED:
            {
                status = thermDeviceLoad_GDDR6_X_COMBINED(pDev);
                break;
            }
#endif
            default:
            {
                // To pacify coverity.
                break;
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

thermDevicesLoad_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
