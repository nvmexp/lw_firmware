/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_point_35.c
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
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different PSTATE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *ClkVfPoint35VirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, BASE)        ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35)          ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35_FREQ)     ] = CLK_VF_POINT_35_FREQ_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35_VOLT_PRI) ] = CLK_VF_POINT_35_VOLT_PRI_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 35_VOLT_SEC) ] = CLK_VF_POINT_35_VOLT_SEC_VTABLE(),
};

/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkVfPointSecIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkVfPointSecIfaceModel10SetHeader");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_POINT_35 handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10Set_35
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    LwBool      bFirstTime = (Clk.vfPoints.super.super.objSlots == 0U);

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E512,            // _grpType
        CLK_VF_POINT_PRI,                           // _class
        pBuffer,                                    // _pBuffer
        clkVfPointIfaceModel10SetHeader,                  // _hdrFunc
        clkVfPointIfaceModel10SetEntry,                   // _entryFunc
        pascalAndLater.clk.clkVfPointGrpSet.pri,    // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        ClkVfPoint35VirtualTables);                 // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBoardObjGrpIfaceModel10Set_35_exit;
    }

    // If secondary lwrves are supported, contruct board objects group.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVfPointSec)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = BOARDOBJGRP_IFACE_MODEL_10_SET(E512,            // _grpType
                CLK_VF_POINT_SEC,                           // _class
                pBuffer,                                    // _pBuffer
                s_clkVfPointSecIfaceModel10SetHeader,             // _hdrFunc
                clkVfPointIfaceModel10SetEntry,                   // _entryFunc
                pascalAndLater.clk.clkVfPointGrpSet.sec,    // _ssElement
                OVL_INDEX_DMEM(clkVfPointSec),              // _ovlIdxDmem
                ClkVfPoint35VirtualTables);                 // _ppObjectVtables
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointBoardObjGrpIfaceModel10Set_35_exit;
        }
    }

    //
    // On version 35, Ilwalidate the CLK VF POINTs on SET CONTROL.
    // @note SET_CONTROL are not supported on AUTO as we do not
    // support OVOC on AUTO.
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
        (!bFirstTime))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
        };

        OSTASK_OVL_DESC ovlDescListLoad[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, vfLoad)
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVfPointSec)
#endif
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListLoad);
        {
            status = clkDomainsLoad();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListLoad);

        // VFE evaluation is not required.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkIlwalidate(LW_FALSE);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointBoardObjGrpIfaceModel10Set_35_exit;
        }
    }

clkVfPointBoardObjGrpIfaceModel10Set_35_exit:
    return status;
}


/*!
 * CLK_VF_POINT_35 handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10GetStatus_35
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E512,               // _grpType
        CLK_VF_POINT_PRI,                               // _class
        pBuffer,                                        // _pBuffer
        clkVfPointIfaceModel10GetStatusHeader,                      // _hdrFunc
        clkVfPointIfaceModel10GetStatus,                            // _entryFunc
        pascalAndLater.clk.clkVfPointGrpGetStatus.pri); // _ssElement
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBoardObjGrpIfaceModel10GetStatus_35_exit;
    }

    // If secondary lwrves are supported, contruct board objects group.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, clkVfPointSec)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E512,               // _grpType
                CLK_VF_POINT_SEC,                               // _class
                pBuffer,                                        // _pBuffer
                clkVfPointIfaceModel10GetStatusHeader,                      // _hdrFunc
                clkVfPointIfaceModel10GetStatus,                            // _entryFunc
                pascalAndLater.clk.clkVfPointGrpGetStatus.sec); // _ssElement
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointBoardObjGrpIfaceModel10GetStatus_35_exit;
        }
    }

clkVfPointBoardObjGrpIfaceModel10GetStatus_35_exit:
    return status;
}

/*!
 * CLK_VF_POINT_35 super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfPointGrpIfaceModel10ObjSet_35_IMPL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS      status;

    // Call into super-constructor
    status = clkVfPointGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointGrpIfaceModel10ObjSet_35_exit;
    }

clkVfPointGrpIfaceModel10ObjSet_35_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus_35_IMPL
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    return clkVfPointIfaceModel10GetStatus_SUPER(pModel10, pPayload);
}

/*!
 * @copydoc ClkVfPoint35BaseVFTupleGet
 */
LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *
clkVfPoint35BaseVFTupleGet
(
    CLK_VF_POINT_35 *pVfPoint35
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pTuple = NULL;

    switch (BOARDOBJ_GET_TYPE(pVfPoint35))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            pTuple = clkVfPoint35BaseVFTupleGet_FREQ(pVfPoint35);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            pTuple = clkVfPoint35BaseVFTupleGet_VOLT_PRI(pVfPoint35);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            {
                pTuple = clkVfPoint35BaseVFTupleGet_VOLT_SEC(pVfPoint35);
            }
            break;
        }
        default:
        {
            // Not Supported
            break;
        }
    }
    if (pTuple == NULL)
    {
        PMU_BREAKPOINT();
    }

    return pTuple;
}

/*!
 * @copydoc ClkVfPoint35OffsetedVFTupleGet
 */
LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *
clkVfPoint35OffsetedVFTupleGet
(
    CLK_VF_POINT_35 *pVfPoint35
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pTuple = NULL;

    switch (BOARDOBJ_GET_TYPE(pVfPoint35))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            pTuple = clkVfPoint35OffsetedVFTupleGet_FREQ(pVfPoint35);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            pTuple = clkVfPoint35OffsetedVFTupleGet_VOLT_PRI(pVfPoint35);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            {
                pTuple = clkVfPoint35OffsetedVFTupleGet_VOLT_SEC(pVfPoint35);
            }
            break;
        }
        default:
        {
            // Not Supported
            break;
        }
    }
    if (pTuple == NULL)
    {
        PMU_BREAKPOINT();
    }

    return pTuple;
}

/*!
 * @copydoc ClkVfPoint35Cache
 */
FLCN_STATUS
clkVfPoint35Cache_IMPL
(
    CLK_VF_POINT_35            *pVfPoint35,
    CLK_VF_POINT_35            *pVfPoint35Last,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    LwU8                        voltRailIdx,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx clkIdx;
    LwU8          clkPosPrimary;
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTuple  =
        clkVfPoint35OffsetedVFTupleGet(pVfPoint35);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple =
        clkVfPoint35BaseVFTupleGet(pVfPoint35);

    // Sanity check.
    if ((pBaseVFTuple == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPoint35Cache_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pVfPoint35))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ:
        {
            status = clkVfPoint35Cache_FREQ(pVfPoint35,
                                            pVfPoint35Last,
                                            pDomain35Primary,
                                            pProg35Primary,
                                            voltRailIdx,
                                            lwrveIdx,
                                            bVFEEvalRequired);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            status = clkVfPoint35Cache_VOLT_PRI(pVfPoint35,
                                            pVfPoint35Last,
                                            pDomain35Primary,
                                            pProg35Primary,
                                            voltRailIdx,
                                            lwrveIdx,
                                            bVFEEvalRequired);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_VF_POINT_SEC);
        {
            status = clkVfPoint35Cache_VOLT_SEC(pVfPoint35,
                                            pVfPoint35Last,
                                            pDomain35Primary,
                                            pProg35Primary,
                                            voltRailIdx,
                                            lwrveIdx,
                                            bVFEEvalRequired);
            break;
        }
        default:
        {
            // Not Supported
            break;
        }
    }
   // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint35Cache_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED))
    {
        CLK_DOMAIN_35_PROG *pDomain35Prog;

        pDomain35Prog = &pDomain35Primary->super;
        if (pDomain35Prog == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfPoint35Cache_exit;
        }

        clkPosPrimary =
            clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

        // Validate the position.
        if (clkPosPrimary == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto clkVfPoint35Cache_exit;
        }

        // Adjust with clock domain's and clock programming's voltage delta
        status = clkDomain3XProgVoltAdjustDeltauV(
                    (CLK_DOMAIN_3X_PROG *)pDomain35Primary,
                    &pProg35Primary->primary,
                    voltRailIdx,
                    &pOffsetedVFTuple[clkPosPrimary].voltageuV);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPoint35Cache_exit;
        }

        // Apply clock domain and clock programming offsets to primary clock domain
        status = clkDomain3XPrimaryFreqAdjustDeltaMHz(
                    &pDomain35Primary->primary,                   // pDomain3XPrimary
                    &pProg35Primary->primary,                     // pProg3XPrimary
                    LW_FALSE,                                   // bQuantize
                    LW_FALSE,                                   // bVFOCAdjReq
                    &pOffsetedVFTuple[clkPosPrimary].freqMHz);   // pFreqMHz
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPoint35Cache_exit;
        }

        // Quantize the offseted frequency
        status = clkProg3XFreqMHzQuantize(
                    &pProg35Primary->super.super,                // pProg3X
                    (CLK_DOMAIN_3X_PROG *)pDomain35Primary,       // pDomain3XProg
                    &pOffsetedVFTuple[clkPosPrimary].freqMHz,    // pFreqMHz
                    LW_TRUE);                                   // bFloor
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPoint35Cache_exit;
        }

        // Update the offseted frequency value of secondary clock domains
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
            &pDomain35Primary->primary.secondaryIdxsMask.super)
        {
            LwU8 clkPos;

            pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkIdx);
            if (pDomain35Prog == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkVfPoint35Cache_exit;
            }

            clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

            // Validate the position.
            if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
            {
                if (lwrveIdx < LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto clkVfPoint35Cache_exit;
                }
                continue;
            }

            // Copy-in the offseted primary clock freq to secondary.
            pOffsetedVFTuple[clkPos].freqMHz =
                pOffsetedVFTuple[clkPosPrimary].freqMHz;

            status = clkProg3XPrimaryFreqTranslatePrimaryToSecondary(
                        &pProg35Primary->primary,                     // pProg3XPrimary
                        &pDomain35Primary->primary,                   // pDomain3XPrimary
                        BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),           // clkDomIdx
                        &pOffsetedVFTuple[clkPos].freqMHz,          // pFreqMHz
                        LW_FALSE,                                   // bOCAjdAndQuantize
                        LW_TRUE);                                   // bQuantize
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint35Cache_exit;
            }

            // Adjust with clock domain's and clock programming's voltage delta
            status = clkDomain3XProgVoltAdjustDeltauV(
                        (CLK_DOMAIN_3X_PROG *)pDomain,
                        &pProg35Primary->primary,
                        voltRailIdx,
                        &pOffsetedVFTuple[clkPos].voltageuV);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint35Cache_exit;
            }

            CLK_DOMAIN_35_SECONDARY *pDomain35secondary = (CLK_DOMAIN_35_SECONDARY *)pDomain;

            // Adjust with clock domain's and clock programming's frequency delta
            status = clkDomain3XSecondaryFreqAdjustDeltaMHz(
                        &pDomain35secondary->secondary,         // pDomain3XProg
                        &pProg35Primary->primary,               // pProg3XPrimary
                        LW_FALSE,                               // bIncludePrimary
                        LW_FALSE,                               // bQuantize
                        LW_FALSE,                               // bVFOCAdjReq
                        &pOffsetedVFTuple[clkPos].freqMHz);     // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint35Cache_exit;
            }

            // Quantize the offseted frequency
            status = clkProg3XFreqMHzQuantize(
                        &pProg35Primary->super.super,            // pProg3X
                        (CLK_DOMAIN_3X_PROG *)pDomain,           // pDomain3XProg
                        &pOffsetedVFTuple[clkPos].freqMHz,      // pFreqMHz
                        LW_TRUE);                               // bFloor
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfPoint35Cache_exit;
            }

        }
        BOARDOBJGRP_ITERATOR_END;
    }

    //
    // If enabled, enforce monotonicity of VF lwrve to ensure that this VF point
    // is >= the last VF point per the provided pVfPoint35Last pointer.
    //
    if ((clkDomainsVfMonotonicityEnforced()) &&
        (pVfPoint35Last != NULL))
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast =
            clkVfPoint35BaseVFTupleGet(pVfPoint35Last);

        // Sanity check.
        if (pBaseVFTupleLast == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto clkVfPoint35Cache_exit;
        }

        pBaseVFTuple->voltageuV =
            LW_MAX(pBaseVFTuple->voltageuV,
                pBaseVFTupleLast->voltageuV);

        // Adjust the offseted VF lwrve point based on POR requirements.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
            &pDomain35Primary->primarySecondaryDomainsGrpMask.super)
        {
            CLK_DOMAIN_35_PROG *pDomain35Prog;
            LwU8 clkPos;

            pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkIdx);
            if (pDomain35Prog == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkVfPoint35Cache_exit;
            }

            clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

            // Validate the position.
            if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
            {
                if (lwrveIdx < LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto clkVfPoint35Cache_exit;
                }
                continue;
            }

            pBaseVFTuple->freqTuple[clkPos].freqMHz =
                LW_MAX(pBaseVFTuple->freqTuple[clkPos].freqMHz,
                    pBaseVFTupleLast->freqTuple[clkPos].freqMHz);

            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED))
            {
                LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTupleLast  =
                    clkVfPoint35OffsetedVFTupleGet(pVfPoint35Last);

                // Sanity check.
                if (pOffsetedVFTupleLast == NULL)
                {
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto clkVfPoint35Cache_exit;
                }

                pOffsetedVFTuple[clkPos].voltageuV =
                    LW_MAX(pOffsetedVFTuple[clkPos].voltageuV,
                        pOffsetedVFTupleLast[clkPos].voltageuV);

                pOffsetedVFTuple[clkPos].freqMHz =
                    LW_MAX(pOffsetedVFTuple[clkPos].freqMHz,
                        pOffsetedVFTupleLast[clkPos].freqMHz);
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

clkVfPoint35Cache_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint35Smoothing
 */
FLCN_STATUS
clkVfPoint35Smoothing
(
    CLK_VF_POINT_35            *pVfPoint35,
    CLK_VF_POINT_35            *pVfPoint35Last,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    LwU8                        lwrveIdx
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint35))
    {
        // Only supported on NAFLL (Voltage) based VF points.
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            status = clkVfPoint35Smoothing_VOLT_PRI(pVfPoint35,
                                                pVfPoint35Last,
                                                pDomain35Primary,
                                                pProg35Primary,
                                                lwrveIdx);
            break;
        }
        // Only supported on NAFLL (Voltage) based VF points.
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            {
                status = clkVfPoint35Smoothing_VOLT_SEC(pVfPoint35,
                                                    pVfPoint35Last,
                                                    pDomain35Primary,
                                                    pProg35Primary,
                                                    lwrveIdx);
            }
            break;
        }
        default:
        {
            // VF Smoothing is not required on _FREQ class.
            status = FLCN_OK;
            break;
        }
    }

    // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkVfPoint35Trim
 */
FLCN_STATUS
clkVfPoint35Trim
(
    CLK_VF_POINT_35            *pVfPoint35,
    CLK_DOMAIN_35_PRIMARY       *pDomain35Primary,
    CLK_PROG_35_PRIMARY         *pProg35Primary,
    LwU8                        lwrveIdx
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx clkIdx;
    FLCN_STATUS   status = FLCN_OK;
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTuple  =
        clkVfPoint35OffsetedVFTupleGet(pVfPoint35);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple =
        clkVfPoint35BaseVFTupleGet(pVfPoint35);

    // Sanity check.
    if ((pOffsetedVFTuple == NULL) ||
        (pBaseVFTuple == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPoint35Trim_exit;
    }

    // Trim the VF point of secondaries to enumeration max.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &pDomain35Primary->primary.secondaryIdxsMask.super)
    {
        CLK_DOMAIN_35_PROG *pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkIdx);
        CLK_PROG_35        *pProg35;
        LwU8                clkPos;

        if (pDomain35Prog == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfPoint35Trim_exit;
        }

        pProg35 = (CLK_PROG_35 *)BOARDOBJGRP_OBJ_GET(CLK_PROG, 
            pDomain35Prog->super.clkProgIdxLast);
        if (pProg35 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfPoint35Trim_exit;
        }

        clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog, lwrveIdx);

        // Validate the position.
        if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            // Not all clock domains support secondary VF lwrves.
            if (lwrveIdx == LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto clkVfPoint35Trim_exit;
            }
            continue;
        }

        // Trim the VF lwrve to enumeration max.
        pOffsetedVFTuple[clkPos].freqMHz =
            LW_MIN(pOffsetedVFTuple[clkPos].freqMHz,
                clkProg35OffsettedFreqMaxMHzGet(pProg35));

        pBaseVFTuple->freqTuple[clkPos].freqMHz =
            LW_MIN(pBaseVFTuple->freqTuple[clkPos].freqMHz,
                clkProg3XFreqMaxMHzGet(&pProg35->super));
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPoint35Trim_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointAccessor
 */
FLCN_STATUS
clkVfPointAccessor_35
(
    CLK_VF_POINT            *pVfPoint,
    CLK_PROG_3X_PRIMARY      *pProg3XPrimary,
    CLK_DOMAIN_3X_PRIMARY    *pDomain3XPrimary,
    LwU8                     clkDomIdx,
    LwU8                     voltRailIdx,
    LwU8                     voltageType,
    LwBool                   bOffseted,
    LW2080_CTRL_CLK_VF_PAIR *pVfPair
)
{
    CLK_VF_POINT_35 *pVfPoint35;
    CLK_DOMAIN_35_PROG *pDomain35Prog = CLK_DOMAIN_35_PROG_GET(clkDomIdx);
    LwU8             clkPos;
    FLCN_STATUS      status     = FLCN_OK;
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTuple;
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint35, &pVfPoint->super,
                                  CLK, CLK_VF_POINT, 35, &status,
                                  clkVfPointAccessor_35_exit);
    pOffsetedVFTuple  = clkVfPoint35OffsetedVFTupleGet(pVfPoint35);
    pBaseVFTuple      = clkVfPoint35BaseVFTupleGet(pVfPoint35);

    if (pDomain35Prog == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto clkVfPointAccessor_35_exit;
    }

    clkPos = clkDomain35ProgGetClkDomainPosByIdx(pDomain35Prog,
        LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI);

    // Sanity check.
    if ((pBaseVFTuple == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPointAccessor_35_exit;
    }

    // Validate the position.
    if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto clkVfPointAccessor_35_exit;
    }

    if (bOffseted)
    {
        pVfPair->voltageuV = pOffsetedVFTuple[clkPos].voltageuV;
        pVfPair->freqMHz   = pOffsetedVFTuple[clkPos].freqMHz;
    }
    else
    {
        pVfPair->voltageuV = pBaseVFTuple->voltageuV;
        pVfPair->freqMHz   = pBaseVFTuple->freqTuple[clkPos].freqMHz;
    }

    // Get the voltage value based on requested @ref voltageType
    if (LW2080_CTRL_CLK_VOLTAGE_TYPE_POR != voltageType)
    {
        status = FLCN_ERR_NOT_SUPPORTED;

        //
        // Call into type-specific implementations to see if other voltage types are
        // supported.
        //
        switch (BOARDOBJ_GET_TYPE(pVfPoint))
        {
            case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
                {
                    status = clkVfPointVoltageuVGet_35_VOLT(pVfPoint,
                                                            voltageType,
                                                            &pVfPair->voltageuV);
                }
                break;
            }
            case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35))
                {
                    status = clkVfPointVoltageuVGet_35_VOLT(pVfPoint,
                                                            voltageType,
                                                            &pVfPair->voltageuV);
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
            goto clkVfPointAccessor_35_exit;
        }
    }

clkVfPointAccessor_35_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint35FreqTupleAccessor
 */
FLCN_STATUS
clkVfPoint35FreqTupleAccessor
(
    CLK_VF_POINT_35                                *pVfPoint35,
    CLK_DOMAIN_3X_PRIMARY                           *pDomain3XPrimary,
    LwU8                                            clkDomIdx,
    LwU8                                            lwrveIdx,
    LwBool                                          bOffseted,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint35))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI:
        {
            status = clkVfPoint35FreqTupleAccessor_VOLT_PRI(pVfPoint35,
                                                            pDomain3XPrimary,
                                                            clkDomIdx,
                                                            lwrveIdx,
                                                            bOffseted,
                                                            pOutput);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_CLK_CLK_VF_POINT_SEC);
        {
            status = clkVfPoint35FreqTupleAccessor_VOLT_SEC(pVfPoint35,
                                                            pDomain3XPrimary,
                                                            clkDomIdx,
                                                            lwrveIdx,
                                                            bOffseted,
                                                            pOutput);
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
        goto clkVfPoint35FreqTupleAccessor_exit;
    }

clkVfPoint35FreqTupleAccessor_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointVoltageuVGet
 */
FLCN_STATUS
clkVfPointVoltageuVGet_35_IMPL
(
    CLK_VF_POINT  *pVfPoint,
    LwU8           voltageType,
    LwU32         *pVoltageuV
)
{
    CLK_VF_POINT_35 *pVfPoint35;
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple;
    FLCN_STATUS status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint35, &pVfPoint->super,
                                  CLK, CLK_VF_POINT, 35, &status,
                                  clkVfPointVoltageuVGet_35_exit);
    pBaseVFTuple = clkVfPoint35BaseVFTupleGet(pVfPoint35);

    // Sanity check.
    if (pBaseVFTuple == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // If POR value requested, return that value from the SUPER structure.
    if (LW2080_CTRL_CLK_VOLTAGE_TYPE_POR == voltageType)
    {
        *pVoltageuV = pBaseVFTuple->voltageuV;
        status = FLCN_OK;
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
    }

clkVfPointVoltageuVGet_35_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * CLK_VF_POINT implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
s_clkVfPointSecIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    FLCN_STATUS status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkVfPointSecIfaceModel10SetHeader_exit;
    }

    // Copy global CLK_VF_POINTS state.

s_clkVfPointSecIfaceModel10SetHeader_exit:
    return status;
}
