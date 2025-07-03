/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_point.c
 *
 * @brief Module managing all state related to the CLK_VF_POINT structure.  This
 * structure defines a point on the VF lwrve of a PRIMARY CLK_DOMAIN.  This
 * finite point is created from the PRIMARY CLK_DOMAIN's CLK_PROG entries and the
 * voltage and/or frequency points they specify.  These points are evaluated and
 * cached once per the VFE polling loop such that they can be referenced later
 * via lookups for various necessary operations.
 *
 * The CLK_VF_POINT class is a virtual interface.  It does not include any
 * specifcation for how to compute voltage or frequency as a function of the
 * other.  It is up to the implementing classes to apply those semantics.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief CLK_VF_POINT handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Increment the VF Points cache counter due to change in CLK_VF_POINTs params
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    {
        clkVfPointsVfPointsCacheCounterIncrement();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
    {
        status = clkVfPointBoardObjGrpIfaceModel10Set_30(pBuffer);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
    {
        status = clkVfPointBoardObjGrpIfaceModel10Set_35(pBuffer);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
    {
        status = clkVfPointBoardObjGrpIfaceModel10Set_40(pBuffer);
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBoardObjGrpIfaceModel10Set_exit;
    }

clkVfPointBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @brief CLK_VF_POINT handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
    {
        status = clkVfPointBoardObjGrpIfaceModel10GetStatus_30(pBuffer);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
    {
        status = clkVfPointBoardObjGrpIfaceModel10GetStatus_35(pBuffer);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
    {
        status = clkVfPointBoardObjGrpIfaceModel10GetStatus_40(pBuffer);
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBoardObjGrpIfaceModel10GetStatus_exit;
    }

clkVfPointBoardObjGrpIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @brief CLK_VF_POINT super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfPointGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_VF_POINT *pVfPoint;
    FLCN_STATUS status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointGrpIfaceModel10ObjSet_SUPER_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint, *ppBoardObj,
                                  CLK, CLK_VF_POINT, BASE, &status,
                                  clkVfPointGrpIfaceModel10ObjSet_SUPER_exit);

    (void)pVfPoint;
    (void)pSet;

clkVfPointGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific status functions.
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
            {
                status = clkVfPointIfaceModel10GetStatus_30(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointIfaceModel10GetStatus_35_FREQ(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointIfaceModel10GetStatus_35_VOLT_PRI(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC) &&
                PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointIfaceModel10GetStatus_35_VOLT_SEC(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointIfaceModel10GetStatus_40_FREQ(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointIfaceModel10GetStatus_40_VOLT_PRI(pModel10, pPayload);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC) &&
                PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointIfaceModel10GetStatus_40_VOLT_SEC(pModel10, pPayload);
            }
            break;
        }
        default:
        {
            // Not Supported
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
 * @copydoc ClkVfPointVoltageuVGet
 */
FLCN_STATUS
clkVfPointVoltageuVGet
(
    CLK_VF_POINT  *pVfPoint,
    LwU8           voltageType,
    LwU32         *pVoltageuV
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    //
    // Call into type-specific implementations to see if other voltage types are
    // supported.
    //
    switch (BOARDOBJ_GET_TYPE(pVfPoint))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
            {
                status = clkVfPointVoltageuVGet_30_FREQ(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
            {
                status = clkVfPointVoltageuVGet_30_VOLT(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointVoltageuVGet_35_FREQ(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointVoltageuVGet_35_VOLT(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC) &&
                PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status = clkVfPointVoltageuVGet_35_VOLT(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointVoltageuVGet_40_FREQ(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointVoltageuVGet_40_VOLT(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC) &&
                PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointVoltageuVGet_40_VOLT(pVfPoint, voltageType, pVoltageuV);
            }
            break;
        }
        default:
        {
            // Not Supported
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
 * @copydoc ClkVfPointClientVfTupleGet
 */
FLCN_STATUS
clkVfPointClientVfTupleGet
(
    CLK_VF_POINT                                  *pVfPoint,
    LwBool                                         bOffset,
    LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TUPLE     *pClientVfPointTuple
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointClientVfTupleGet_40(pVfPoint, bOffset, pClientVfPointTuple);
            }
            break;
        }
        default:
        {
            //Not supported
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
 * @copydoc ClkVfPointClientFreqDeltaSet
 */
FLCN_STATUS
clkVfPointClientFreqDeltaSet
(
    CLK_VF_POINT                  *pVfPoint,
    LW2080_CTRL_CLK_FREQ_DELTA    *pFreqDelta
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointClientFreqDeltaSet_40(pVfPoint, pFreqDelta);
            }
            break;
        }
        default:
        {
            // Not Supported
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
 * @copydoc ClkVfPointClientVoltDeltaSet
 */
FLCN_STATUS
clkVfPointClientVoltDeltaSet
(
    CLK_VF_POINT    *pVfPoint,
    LwS32            voltDelta
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointClientVoltDeltaSet_40_FREQ(pVfPoint, voltDelta);
            }
            break;
        }
        default:
        {
            // Not Supported
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
 * @copydoc ClkVfPointClientCpmMaxOffsetOverrideSet
 */
FLCN_STATUS
clkVfPointClientCpmMaxOffsetOverrideSet
(
    CLK_VF_POINT    *pVfPoint,
    LwU16            cpmMaxFreqOffsetOverrideMHz
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status = clkVfPointClientCpmMaxOffsetOverrideSet_40_VOLT(pVfPoint, cpmMaxFreqOffsetOverrideMHz);
            }
            break;
        }
        default:
        {
            // Not Supported
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
 * @brief CLK_VF_POINT implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
clkVfPointIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    /* RM_PMU_CLK_CLK_VF_POINT_BOARDOBJGRP_SET_HEADER *pSet = */
    /*     (RM_PMU_CLK_CLK_VF_POINT_BOARDOBJGRP_SET_HEADER *)pHdrDesc; */
    /* CLK_VF_POINTS *pVfPoints = (CLK_VF_POINTS *)pBObjGrp; */
    FLCN_STATUS  status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointIfaceModel10SetHeader_exit;
    }

    // Copy global CLK_VF_POINTS state.

clkVfPointIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @brief Constructs a CLK_VF_POINT of the corresponding type.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
clkVfPointIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_30_FREQ(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_30_FREQ), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_30_VOLT(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_30_VOLT), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_35_FREQ(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_35_FREQ), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_35_VOLT(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_35_VOLT_PRI), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC) &&
                PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_35_VOLT_SEC), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_40_FREQ(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_40_FREQ), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_40_VOLT(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_40_VOLT_PRI), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC) &&
                PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40))
            {
                status =  clkVfPointGrpIfaceModel10ObjSet_40_VOLT_SEC(pModel10, ppBoardObj,
                    sizeof(CLK_VF_POINT_40_VOLT_SEC), pBuf);
            }
            break;
        }
        default:
        {
            // Not Supported
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
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10         *pModel10,
    RM_PMU_BOARDOBJGRP  *pBuf,
    BOARDOBJGRPMASK     *pMask
)
{
    RM_PMU_CLK_CLK_VF_POINT_BOARDOBJGRP_GET_STATUS_HEADER *pHdr;
    CLK_VF_POINTS                                         *pVfPoints;

    // Must NOT cast @ref pBoardObjGrp as there are multiple board object groups.
    pVfPoints = &Clk.vfPoints;
    pHdr      = (RM_PMU_CLK_CLK_VF_POINT_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;

    // Get the CLK_FREQ_CONTROLLER query parameters.
    pHdr->vfPointsCacheCounter = pVfPoints->vfPointsCacheCounter;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
