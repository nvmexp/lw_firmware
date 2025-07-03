/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_vf_point_40.c
 *
 * @brief Module managing all state related to the CLK_VF_POINT structure. This
 * structure defines a point on the VF lwrve. This finite point is created from
 * the PRIMARY CLK_DOMAIN's VF REL entries using the voltage and/or frequency
 * points they specify. These points are evaluated and cached once per the VFE
 * polling loop such that they can be referenced later via lookups for various
 * necessary operations.
 *
 * The CLK_VF_POINT_40 class is a virtual interface. It does not include any
 * specification for how to compute voltage or frequency as a function of the
 * other. It is up to the implementing classes to apply those semantics.
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
BOARDOBJ_VIRTUAL_TABLE *ClkVfPoint40VirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, BASE)        ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40)          ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_FREQ)     ] = CLK_VF_POINT_40_FREQ_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT_PRI) ] = CLK_VF_POINT_40_VOLT_PRI_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 40_VOLT_SEC) ] = CLK_VF_POINT_40_VOLT_SEC_VTABLE(),
};

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_BASE_VF_CACHE))
static CLK_VF_POINT_PRI_VF_CACHE  VfCachePri
    GCC_ATTRIB_SECTION("dmem_perf", "VfCachePri") = {0};
static CLK_VF_POINT_SEC_VF_CACHE  VfCacheSec
    GCC_ATTRIB_SECTION("dmem_perf", "VfCacheSec") = {0};
#endif

/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkVfPointSecIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkVfPointSecIfaceModel10SetHeader");
static FLCN_STATUS s_clkVfPoint40OCOVAdjust(CLK_VF_POINT_40 *pVfPoint40, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *pOffsetedVFTuple)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40OCOVAdjust");
static FLCN_STATUS s_clkVfPoint40OffsetVFOCOVAdjust(CLK_VF_POINT_40 *pVfPoint40, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL *pVfRel, LW2080_CTRL_CLK_OFFSET_VF_TUPLE *pOffsetedVFTuple)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40OffsetVFOCOVAdjust");
static FLCN_STATUS
s_clkVfPoint40OffsetedVFCache_HELPER
(
    CLK_VF_POINT_40                             *pVfPoint40,
    CLK_VF_POINT_40                             *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                          *pDomain40Prog,
    CLK_VF_REL                                  *pVfRel,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE       *pOffsetedVFTuple
) GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40OffsetedVFCache_HELPER");
static FLCN_STATUS
s_clkVfPoint40SecondaryOffsetedVFCache_HELPER
(
    CLK_VF_POINT_40                             *pVfPoint40,
    CLK_VF_POINT_40                             *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                          *pDomain40Prog,
    CLK_VF_REL                                  *pVfRel,
    LwU8                                         lwrveIdx,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE       *pOffsetedVFTuple
) GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40SecondaryOffsetedVFCache_HELPER");
static FLCN_STATUS
s_clkVfPoint40SecondaryOffsetVFCache_HELPER
(
    CLK_VF_POINT_40                     *pVfPoint40,
    CLK_VF_POINT_40                     *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                  *pDomain40Prog,
    CLK_VF_REL                          *pVfRel,
    LwU8                                 lwrveIdx,
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE     *pOffsetVFTuple
) GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40SecondaryOffsetVFCache_HELPER");
static FLCN_STATUS
s_clkVfPoint40OffsetVFCache_HELPER
(
    CLK_VF_POINT_40                     *pVfPoint40,
    CLK_VF_POINT_40                     *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                  *pDomain40Prog,
    CLK_VF_REL                          *pVfRel,
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE     *pOffsetVFTuple
) GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "s_clkVfPoint40OffsetVFCache_HELPER");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_POINT_40 handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10Set_40
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    LwBool      bFirstTime = (Clk.vfPoints.super.super.objSlots == 0U);

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_BASE_VF_CACHE))
    if (bFirstTime)
    {
        Clk.vfPoints.pVfCache       = &VfCachePri;
        Clk.vfPoints.sec.pVfCache   = &VfCacheSec;
    }
#endif

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E512,            // _grpType
        CLK_VF_POINT_PRI,                           // _class
        pBuffer,                                    // _pBuffer
        clkVfPointIfaceModel10SetHeader,                  // _hdrFunc
        clkVfPointIfaceModel10SetEntry,                   // _entryFunc
        pascalAndLater.clk.clkVfPointGrpSet.pri,    // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        ClkVfPoint40VirtualTables);                 // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBoardObjGrpIfaceModel10Set_40_exit;
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
                ClkVfPoint40VirtualTables);                 // _ppObjectVtables
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointBoardObjGrpIfaceModel10Set_40_exit;
        }
    }

    // On version 40, Ilwalidate the CLK VF POINTs on SET CONTROL.
    if (!bFirstTime)
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
            goto clkVfPointBoardObjGrpIfaceModel10Set_40_exit;
        }
    }

clkVfPointBoardObjGrpIfaceModel10Set_40_exit:
    return status;
}

/*!
 * CLK_VF_POINT_40 handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10GetStatus_40
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
        goto clkVfPointBoardObjGrpIfaceModel10GetStatus_40_exit;
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
            goto clkVfPointBoardObjGrpIfaceModel10GetStatus_40_exit;
        }
    }

clkVfPointBoardObjGrpIfaceModel10GetStatus_40_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Load
 */
FLCN_STATUS
clkVfPoint40Load
(
    CLK_VF_POINT_40        *pVfPoint40,
    CLK_DOMAIN_40_PROG     *pDomain40Prog,
    CLK_VF_REL             *pVfRel,
    LwU8                    lwrveIdx
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific status functions.
    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        // Cache the quantized base voltage values.
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            status = clkVfPoint40Load_VOLT_PRI(pVfPoint40,
                        pDomain40Prog, pVfRel, lwrveIdx);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            status = clkVfPoint40Load_VOLT_SEC(pVfPoint40,
                        pDomain40Prog, pVfRel, lwrveIdx);
        }
        break;
        // Callwlate the secondary clock frequency and cache it.
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            status = clkVfPoint40Load_FREQ(pVfPoint40,
                        pDomain40Prog, pVfRel, lwrveIdx);
            break;
        }
        default:
        {
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40Load_exit;
    }

clkVfPoint40Load_exit:
    return status;
}

/*!
 * CLK_VF_POINT_40 super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfPointGrpIfaceModel10ObjSet_40
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    // Call into super-constructor
    return clkVfPointGrpIfaceModel10ObjSet_SUPER(pModel10,
                ppBoardObj, size, pBoardObjDesc);
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus_40
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    return clkVfPointIfaceModel10GetStatus_SUPER(pModel10, pPayload);
}

/*!
 * @copydoc ClkVfPoint40BaseVFTupleGet
 */
LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *
clkVfPoint40BaseVFTupleGet
(
    CLK_VF_POINT_40 *pVfPoint40
)
{
    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            return clkVfPoint40BaseVFTupleGet_FREQ(pVfPoint40);
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            return clkVfPoint40BaseVFTupleGet_VOLT_PRI(pVfPoint40);
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            return clkVfPoint40BaseVFTupleGet_VOLT_SEC(pVfPoint40);
        }
        break;
    }

    PMU_BREAKPOINT();
    return NULL;
}

/*!
 * @copydoc ClkVfPoint40OffsetedVFTupleGet
 */
LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE *
clkVfPoint40OffsetedVFTupleGet
(
    CLK_VF_POINT_40 *pVfPoint40
)
{
    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            return clkVfPoint40OffsetedVFTupleGet_FREQ(pVfPoint40);
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            return clkVfPoint40OffsetedVFTupleGet_VOLT_PRI(pVfPoint40);
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            return clkVfPoint40OffsetedVFTupleGet_VOLT_SEC(pVfPoint40);
        }
        break;
    }

    PMU_BREAKPOINT();
    return NULL;
}

/*!
 * @copydoc ClkVfPoint40OffsetVFTupleGet
 */
LW2080_CTRL_CLK_OFFSET_VF_TUPLE *
clkVfPoint40OffsetVFTupleGet
(
    CLK_VF_POINT_40 *pVfPoint40
)
{
    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            return clkVfPoint40OffsetVFTupleGet_FREQ(pVfPoint40);
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            return clkVfPoint40OffsetVFTupleGet_VOLT_PRI(pVfPoint40);
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            return clkVfPoint40OffsetVFTupleGet_VOLT_SEC(pVfPoint40);
        }
        break;
    }

    PMU_BREAKPOINT();
    return NULL;
}

/*!
 * @copydoc ClkVfPoint40Cache
 */
FLCN_STATUS
clkVfPoint40Cache
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple =
        clkVfPoint40OffsetedVFTupleGet(pVfPoint40);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
        clkVfPoint40BaseVFTupleGet(pVfPoint40);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check.
    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40Cache_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            status = clkVfPoint40Cache_FREQ(pVfPoint40,
                                            pVfPoint40Last,
                                            pDomain40Prog,
                                            pVfRel,
                                            lwrveIdx,
                                            bVFEEvalRequired);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            status = clkVfPoint40Cache_VOLT_PRI(pVfPoint40,
                                            pVfPoint40Last,
                                            pDomain40Prog,
                                            pVfRel,
                                            lwrveIdx,
                                            bVFEEvalRequired);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            status = clkVfPoint40Cache_VOLT_SEC(pVfPoint40,
                                            pVfPoint40Last,
                                            pDomain40Prog,
                                            pVfRel,
                                            lwrveIdx,
                                            bVFEEvalRequired);
        }
        break;
    }
    // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40Cache_exit;
    }

    // Adjust for OVOC if supported and enabled.
    status = s_clkVfPoint40OffsetedVFCache_HELPER(pVfPoint40,
                                                  pVfPoint40Last,
                                                  pDomain40Prog,
                                                  pVfRel,
                                                  pBaseVFTuple,
                                                  pOffsetedVFTuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40Cache_exit;
    }

    // Enforce VF Monotonicity.
    if ((clkDomainsVfMonotonicityEnforced()) &&
        (pVfPoint40Last != NULL))
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTupleLast =
            clkVfPoint40OffsetedVFTupleGet(pVfPoint40Last);
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast     =
            clkVfPoint40BaseVFTupleGet(pVfPoint40Last);

        // Sanity check.
        if ((pOffsetedVFTupleLast == NULL) ||
            (pBaseVFTupleLast     == NULL))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkVfPoint40Cache_exit;
        }

        pBaseVFTuple->voltageuV =
            LW_MAX(pBaseVFTuple->voltageuV, pBaseVFTupleLast->voltageuV);
        pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz =
            LW_MAX(pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz,
                pBaseVFTupleLast->freqTuple[CLK_VF_PRIMARY_POS].freqMHz);

        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz =
            LW_MAX(pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz,
                pOffsetedVFTupleLast[CLK_VF_PRIMARY_POS].freqMHz);
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].voltageuV =
            LW_MAX(pOffsetedVFTuple[CLK_VF_PRIMARY_POS].voltageuV,
                pOffsetedVFTupleLast[CLK_VF_PRIMARY_POS].voltageuV);
    }

clkVfPoint40Cache_exit:
    return status;
}


/*!
 * @copydoc ClkVfPoint40Cache
 */
static FLCN_STATUS
s_clkVfPoint40OffsetedVFCache_HELPER
(
    CLK_VF_POINT_40                             *pVfPoint40,
    CLK_VF_POINT_40                             *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                          *pDomain40Prog,
    CLK_VF_REL                                  *pVfRel,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE       *pOffsetedVFTuple
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkVfPoint40OffsetedVFCache_HELPER_exit;
    }

    // Adjust for OVOC if supported and enabled.
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
        (clkVfRelOVOCEnabled(pVfRel)))
    {
        // Adjust client frequency offset.
        if (clkDomain40ProgIsOCEnabled(pDomain40Prog))
        {
            LW2080_CTRL_CLK_VF_INPUT    inputFreq;
            LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

            // Init
            LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);
            inputFreq.value = pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz;

            // Adjust client clock vf point offset.
            pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz =
                 clkFreqDeltaAdjust(pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz,
                                    &pVfPoint40->clientFreqDelta);

            if (inputFreq.value != pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz)
            {
                inputFreq.value = pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz;
                status = clkDomainProgFreqQuantize_40(
                            CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                            &inputFreq,                                         // pInputFreq
                            &outputFreq,                                        // pOutputFreq
                            LW_TRUE,                                            // bFloor
                            LW_FALSE);                                          // bBound
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_clkVfPoint40OffsetedVFCache_HELPER_exit;
                }
                pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz = (LwU16) outputFreq.value;
            }
        }

        // Adjustment for primary.
        status = s_clkVfPoint40OCOVAdjust(pVfPoint40,
                                          pDomain40Prog,
                                          pVfRel,
                                          pOffsetedVFTuple);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OffsetedVFCache_HELPER_exit;
        }
    }
    else
    {
        // Copy-in the base value to offseted tuple.
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz   =
            pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz;
        pOffsetedVFTuple[CLK_VF_PRIMARY_POS].voltageuV = pBaseVFTuple->voltageuV;
    }

s_clkVfPoint40OffsetedVFCache_HELPER_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40SecondaryCache
 */
FLCN_STATUS
clkVfPoint40SecondaryCache
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwBool                      bVFEEvalRequired
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple =
        clkVfPoint40OffsetedVFTupleGet(pVfPoint40);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
        clkVfPoint40BaseVFTupleGet(pVfPoint40);
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx clkIdx;
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check.
    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40SecondaryCache_exit;
    }

    // Generate Secondary's base lwrve from primary.
    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            // Nothing to do as base voltage is common across all.
            status = FLCN_OK;
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            status = clkVfPoint40SecondaryCache_VOLT(pVfPoint40,
                                                 pVfPoint40Last,
                                                 pDomain40Prog,
                                                 pVfRel,
                                                 lwrveIdx,
                                                 bVFEEvalRequired);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            status = clkVfPoint40SecondaryCache_VOLT(pVfPoint40,
                                                 pVfPoint40Last,
                                                 pDomain40Prog,
                                                 pVfRel,
                                                 lwrveIdx,
                                                 bVFEEvalRequired);
        }
        break;
    }
    // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40SecondaryCache_exit;
    }

    // Adjust for OVOC if supported and enabled.
    status = s_clkVfPoint40SecondaryOffsetedVFCache_HELPER(pVfPoint40,
                                                   pVfPoint40Last,
                                                   pDomain40Prog,
                                                   pVfRel,
                                                   lwrveIdx,
                                                   pBaseVFTuple,
                                                   pOffsetedVFTuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40SecondaryCache_exit;
    }

    // Adjustment for secondaries.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx)->super))
    {
        CLK_DOMAIN_40_PROG *pDomain40ProgSecondary = (CLK_DOMAIN_40_PROG *)pDomain;

        LwU8 clkPos =
            clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgSecondary, pVfRel->railIdx);
        if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            // Primary lwrve must be supported on all secondary clk domains.
            if (lwrveIdx == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto clkVfPoint40SecondaryCache_exit;
            }
            continue;
        }

        // Obtain the rail index and other index for comparison
        LwU8            railIdx         = clkVfRelVoltRailIdxGet(pVfRel);
        LwBoardObjIdx   vfPointLwrrIdx  = BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super);
        LwBoardObjIdx   freqEnumTrimIdx =
                clkDomain40ProgBaseFreqEnumMaxTrimVfPointIdxGet(pDomain40ProgSecondary,
                                                                railIdx,
                                                                lwrveIdx);

        //
        // If base lwrve is not generated, bVFEEvalRequired is false,
        // then the index at which trim starts for this secondary domain
        // should be valid. If invalid, something has gone horribly
        // wrong, and we should halt/break.
        // It is useful to remember that trimIdx is set when the base
        // lwrve is generated, and is used when the base lwrve is
        // not.
        //
        if ((!bVFEEvalRequired)                                             &&
            (freqEnumTrimIdx == LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID))
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto clkVfPoint40SecondaryCache_exit;
        }

        //
        // Trim VF lwrve to VF max and frequency enumeration max.
        //
        // If the base lwrve is not regenerated (bVFEEvalRequired is false),
        // then we also look at the index at which the base lwrve was
        // trimmed due to the enumeration max.
        //
        if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX)) &&
            ((clkDomain40ProgEnumFreqMaxMHzGet(pDomain40ProgSecondary) <
                pBaseVFTuple->freqTuple[clkPos].freqMHz)                             ||
            ((!bVFEEvalRequired)                        &&
             (vfPointLwrrIdx > freqEnumTrimIdx))))
        {
            LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTupleLast;
            LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast;

            // If Clock Enum is less than first VF point, it is not acceptable.
            if (pVfPoint40Last == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto clkVfPoint40SecondaryCache_exit;
            }

            //
            // If this code path is taken, and if the index at which the
            // base lwrve was trimmed is invalid, it means it has
            // to be set now
            //
            if (freqEnumTrimIdx == LW2080_CTRL_CLK_CLK_VF_POINT_IDX_ILWALID)
            {
                clkDomain40ProgBaseFreqEnumMaxTrimVfPointIdxSet(
                    pDomain40ProgSecondary,
                    railIdx,
                    lwrveIdx,
                    BOARDOBJ_GET_GRP_IDX(&pVfPoint40Last->super.super));
            }

            pOffsetedVFTupleLast = clkVfPoint40OffsetedVFTupleGet(pVfPoint40Last);
            pBaseVFTupleLast     = clkVfPoint40BaseVFTupleGet(pVfPoint40Last);

            pBaseVFTuple->freqTuple[clkPos].freqMHz =
                pBaseVFTupleLast->freqTuple[clkPos].freqMHz;
            pOffsetedVFTuple[clkPos].freqMHz        =
                pOffsetedVFTupleLast[clkPos].freqMHz;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkVfPoint40SecondaryCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40SecondaryCache
 */
static FLCN_STATUS
s_clkVfPoint40SecondaryOffsetedVFCache_HELPER
(
    CLK_VF_POINT_40                             *pVfPoint40,
    CLK_VF_POINT_40                             *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                          *pDomain40Prog,
    CLK_VF_REL                                  *pVfRel,
    LwU8                                         lwrveIdx,
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  *pBaseVFTuple,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE       *pOffsetedVFTuple
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx clkIdx;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity check.
    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkVfPoint40SecondaryOffsetedVFCache_HELPER;
    }

    // Adjustment for secondaries.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx)->super))
    {
        CLK_DOMAIN_40_PROG *pDomain40ProgSecondary = (CLK_DOMAIN_40_PROG *)pDomain;

        LwU8 clkPos =
            clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgSecondary, pVfRel->railIdx);
        if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            // Primary lwrve must be supported on all secondary clk domains.
            if (lwrveIdx == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto s_clkVfPoint40SecondaryOffsetedVFCache_HELPER;
            }
            continue;
        }

        // Adjust for OVOC if supported and enabled.
        if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
            (clkVfRelOVOCEnabled(pVfRel)))
        {
            // Copy-in the adjusted primary freq/volt to secondary.
            pOffsetedVFTuple[clkPos].freqMHz   = pOffsetedVFTuple[CLK_VF_PRIMARY_POS].freqMHz;
            pOffsetedVFTuple[clkPos].voltageuV = pOffsetedVFTuple[CLK_VF_PRIMARY_POS].voltageuV;

            // Perform primary -> secondary translation.
            status = clkVfRelFreqTranslatePrimaryToSecondary(pVfRel, // pVfRel
                        BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),       // secondaryIdx
                        &pOffsetedVFTuple[clkPos].freqMHz,      // pFreqMHz
                        LW_TRUE);                               // bQuantize
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_clkVfPoint40SecondaryOffsetedVFCache_HELPER;
            }

            status = s_clkVfPoint40OCOVAdjust(pVfPoint40,
                                              pDomain40ProgSecondary,
                                              pVfRel,
                                              pOffsetedVFTuple);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_clkVfPoint40SecondaryOffsetedVFCache_HELPER;
            }
        }
        else
        {
            // Copy-in the adjusted primary freq/volt to secondary.
            pOffsetedVFTuple[clkPos].freqMHz   = pBaseVFTuple->freqTuple[clkPos].freqMHz;
            pOffsetedVFTuple[clkPos].voltageuV = pBaseVFTuple->voltageuV;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_clkVfPoint40SecondaryOffsetedVFCache_HELPER:
    return status;
}

/*!
 * @copydoc ClkVfPoint40OffsetVFCache
 */
FLCN_STATUS
clkVfPoint40OffsetVFCache
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel
)
{
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE      *pOffsetVFTuple =
        clkVfPoint40OffsetVFTupleGet(pVfPoint40);

    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check.
    if (pOffsetVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40OffsetVFCache_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            status = clkVfPoint40OffsetVFCache_FREQ(pVfPoint40,
                                                    pVfPoint40Last,
                                                    pDomain40Prog,
                                                    pVfRel);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            status = clkVfPoint40OffsetVFCache_VOLT_PRI(pVfPoint40,
                                                        pVfPoint40Last,
                                                        pDomain40Prog,
                                                        pVfRel);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            status = clkVfPoint40OffsetVFCache_VOLT_SEC(pVfPoint40,
                                                        pVfPoint40Last,
                                                        pDomain40Prog,
                                                        pVfRel);
        }
        break;
    }
    // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40OffsetVFCache_exit;
    }

    // Adjust for OVOC if supported and enabled.
    status = s_clkVfPoint40OffsetVFCache_HELPER(pVfPoint40,
                                                pVfPoint40Last,
                                                pDomain40Prog,
                                                pVfRel,
                                                pOffsetVFTuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40OffsetVFCache_exit;
    }

clkVfPoint40OffsetVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40OffsetVFCache
 */
static FLCN_STATUS
s_clkVfPoint40OffsetVFCache_HELPER
(
    CLK_VF_POINT_40                     *pVfPoint40,
    CLK_VF_POINT_40                     *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                  *pDomain40Prog,
    CLK_VF_REL                          *pVfRel,
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE     *pOffsetVFTuple
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    if (pOffsetVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkVfPoint40OffsetVFCache_HELPER_exit;
    }

    // Adjust for OVOC if supported and enabled.
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
        (clkVfRelOVOCEnabled(pVfRel)))
    {
        // Adjust client frequency offset.
        if (clkDomain40ProgIsOCEnabled(pDomain40Prog))
        {
            // Adjust client clock vf point offset.
            pOffsetVFTuple[CLK_VF_PRIMARY_POS].freqMHz =
                 clkOffsetVFFreqDeltaAdjust(pOffsetVFTuple[CLK_VF_PRIMARY_POS].freqMHz,
                                    &pVfPoint40->clientFreqDelta);
        }

        // Adjustment for primary.
        status = s_clkVfPoint40OffsetVFOCOVAdjust(pVfPoint40,
                                                  pDomain40Prog,
                                                  pVfRel,
                                                  pOffsetVFTuple);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OffsetVFCache_HELPER_exit;
        }
    }
    else
    {
        // Copy-in the 0 to offseted tuple.
        pOffsetVFTuple[CLK_VF_PRIMARY_POS].freqMHz   = 0U;
        pOffsetVFTuple[CLK_VF_PRIMARY_POS].voltageuV = 0U;
    }

s_clkVfPoint40OffsetVFCache_HELPER_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40SecondaryOffsetVFCache
 */
FLCN_STATUS
clkVfPoint40SecondaryOffsetVFCache
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx
)
{
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE      *pOffsetVFTuple =
        clkVfPoint40OffsetVFTupleGet(pVfPoint40);

    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check.
    if (pOffsetVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40SecondaryOffsetVFCache_exit;
    }

    // Adjust for OVOC if supported and enabled.
    status = s_clkVfPoint40SecondaryOffsetVFCache_HELPER(pVfPoint40,
                                                     pVfPoint40Last,
                                                     pDomain40Prog,
                                                     pVfRel,
                                                     lwrveIdx,
                                                     pOffsetVFTuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40SecondaryOffsetVFCache_exit;
    }

clkVfPoint40SecondaryOffsetVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40SecondaryCache
 */
static FLCN_STATUS
s_clkVfPoint40SecondaryOffsetVFCache_HELPER
(
    CLK_VF_POINT_40                     *pVfPoint40,
    CLK_VF_POINT_40                     *pVfPoint40Last,
    CLK_DOMAIN_40_PROG                  *pDomain40Prog,
    CLK_VF_REL                          *pVfRel,
    LwU8                                 lwrveIdx,
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE     *pOffsetVFTuple
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx clkIdx;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity check.
    if (pOffsetVFTuple == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_clkVfPoint40SecondaryOffsetVFCache_HELPER;
    }

    // Adjustment for secondaries.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx)->super))
    {
        CLK_DOMAIN_40_PROG *pDomain40ProgSecondary = (CLK_DOMAIN_40_PROG *)pDomain;

        LwU8 clkPos =
            clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgSecondary, pVfRel->railIdx);
        if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
        {
            // Primary lwrve must be supported on all secondary clk domains.
            if (lwrveIdx == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
            {
                status = FLCN_ERR_ILWALID_INDEX;
                PMU_BREAKPOINT();
                goto s_clkVfPoint40SecondaryOffsetVFCache_HELPER;
            }
            continue;
        }

        // Adjust for OVOC if supported and enabled.
        if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
            (clkVfRelOVOCEnabled(pVfRel)))
        {
            // Copy-in the adjusted primary freq/volt to secondary.
            pOffsetVFTuple[clkPos].freqMHz   = pOffsetVFTuple[CLK_VF_PRIMARY_POS].freqMHz;
            pOffsetVFTuple[clkPos].voltageuV = pOffsetVFTuple[CLK_VF_PRIMARY_POS].voltageuV;

            // Perform primary -> secondary translation.
            status = clkVfRelOffsetVFFreqTranslatePrimaryToSecondary(pVfRel, // pVfRel
                        BOARDOBJ_GRP_IDX_TO_8BIT(clkIdx),       // secondaryIdx
                        &pOffsetVFTuple[clkPos].freqMHz,        // pFreqMHz
                        LW_TRUE);                               // bQuantize
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_clkVfPoint40SecondaryOffsetVFCache_HELPER;
            }

            status = s_clkVfPoint40OffsetVFOCOVAdjust(pVfPoint40,
                                                      pDomain40ProgSecondary,
                                                      pVfRel,
                                                      pOffsetVFTuple);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_clkVfPoint40SecondaryOffsetVFCache_HELPER;
            }
        }
        else
        {
            // Copy-in the adjusted primary freq/volt to secondary.
            pOffsetVFTuple[clkPos].freqMHz   = 0;
            pOffsetVFTuple[clkPos].voltageuV = 0;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_clkVfPoint40SecondaryOffsetVFCache_HELPER:
    return status;
}

/*!
 * @copydoc ClkVfPoint40BaseVFCache
 */
FLCN_STATUS
clkVfPoint40BaseVFCache
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwU8                        cacheIdx
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            status = clkVfPoint40BaseVFCache_FREQ(pVfPoint40,
                                            pVfPoint40Last,
                                            pDomain40Prog,
                                            pVfRel,
                                            lwrveIdx,
                                            cacheIdx);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            status = clkVfPoint40BaseVFCache_VOLT_PRI(pVfPoint40,
                                            pVfPoint40Last,
                                            pDomain40Prog,
                                            pVfRel,
                                            lwrveIdx,
                                            cacheIdx);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            status = clkVfPoint40BaseVFCache_VOLT_SEC(pVfPoint40,
                                            pVfPoint40Last,
                                            pDomain40Prog,
                                            pVfRel,
                                            lwrveIdx,
                                            cacheIdx);
        }
        break;
    }
    // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40BaseVFCache_exit;
    }

    // Enforce VF Monotonicity.
    if ((clkDomainsVfMonotonicityEnforced()) &&
        (pVfPoint40Last != NULL))
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
            clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
                BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super));
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast     =
            clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
                BOARDOBJ_GET_GRP_IDX(&pVfPoint40Last->super.super));

        // Sanity check.
        if ((pBaseVFTuple     == NULL) ||
            (pBaseVFTupleLast == NULL))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkVfPoint40BaseVFCache_exit;
        }

        pBaseVFTuple->voltageuV =
            LW_MAX(pBaseVFTuple->voltageuV, pBaseVFTupleLast->voltageuV);
        pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz =
            LW_MAX(pBaseVFTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz,
                pBaseVFTupleLast->freqTuple[CLK_VF_PRIMARY_POS].freqMHz);
    }

clkVfPoint40BaseVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40SecondaryBaseVFCache
 */
FLCN_STATUS
clkVfPoint40SecondaryBaseVFCache
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_VF_POINT_40            *pVfPoint40Last,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwU8                        cacheIdx
)
{
    FLCN_STATUS   status = FLCN_ERR_NOT_SUPPORTED;

    // Generate Secondary's base lwrve from primary.
    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_FREQ:
        {
            // Nothing to do as base voltage is common across all.
            status = FLCN_OK;
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            status = clkVfPoint40SecondaryBaseVFCache_VOLT(pVfPoint40,
                                                 pVfPoint40Last,
                                                 pDomain40Prog,
                                                 pVfRel,
                                                 lwrveIdx,
                                                 cacheIdx);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
        {
            status = clkVfPoint40SecondaryBaseVFCache_VOLT(pVfPoint40,
                                                 pVfPoint40Last,
                                                 pDomain40Prog,
                                                 pVfRel,
                                                 lwrveIdx,
                                                 cacheIdx);
        }
        break;
    }
    // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint40SecondaryBaseVFCache_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_TRIM_VF_LWRVE_BY_ENUM_MAX))
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
            clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
                BOARDOBJ_GET_GRP_IDX(&pVfPoint40->super.super));
        CLK_DOMAIN      *pDomain;
        LwBoardObjIdx    clkIdx;

        // Sanity check.
        if (pBaseVFTuple == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkVfPoint40SecondaryBaseVFCache_exit;
        }

        // Adjustment for secondaries.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
            &(clkDomain40ProgGetSecondaryDomainsMask(pDomain40Prog, pVfRel->railIdx)->super))
        {
            CLK_DOMAIN_40_PROG *pDomain40ProgSecondary = (CLK_DOMAIN_40_PROG *)pDomain;

            LwU8 clkPos =
                clkDomain40ProgGetClkDomainPosByIdx(pDomain40ProgSecondary, pVfRel->railIdx);
            if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
            {
                // Primary lwrve must be supported on all secondary clk domains.
                if (lwrveIdx == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI)
                {
                    status = FLCN_ERR_ILWALID_INDEX;
                    PMU_BREAKPOINT();
                    goto clkVfPoint40SecondaryBaseVFCache_exit;
                }
                continue;
            }

            // Trim VF lwrve to VF max and frequency enumeration max.
            if (clkDomain40ProgEnumFreqMaxMHzGet(pDomain40ProgSecondary) <
                    pBaseVFTuple->freqTuple[clkPos].freqMHz)
            {
                LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTupleLast;

                // If Clock Enum is less than first VF point, it is not acceptable.
                if (pVfPoint40Last == NULL)
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto clkVfPoint40SecondaryBaseVFCache_exit;
                }
                pBaseVFTupleLast = clkVfPointsBaseVfCacheBaseVFTupleGet(lwrveIdx, cacheIdx,
                                        BOARDOBJ_GET_GRP_IDX(&pVfPoint40Last->super.super));

                pBaseVFTuple->freqTuple[clkPos].freqMHz =
                    pBaseVFTupleLast->freqTuple[clkPos].freqMHz;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

clkVfPoint40SecondaryBaseVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint40Accessor
 */
FLCN_STATUS
clkVfPoint40Accessor
(
    CLK_VF_POINT_40            *pVfPoint40,
    CLK_DOMAIN_40_PROG         *pDomain40Prog,
    CLK_VF_REL                 *pVfRel,
    LwU8                        lwrveIdx,
    LwU8                        voltageType,
    LwBool                      bOffseted,
    LW2080_CTRL_CLK_VF_PAIR    *pVfPair
)
{
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE      *pOffsetedVFTuple =
        clkVfPoint40OffsetedVFTupleGet(pVfPoint40);
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple     =
        clkVfPoint40BaseVFTupleGet(pVfPoint40);
    LwU8        clkPos =
        clkDomain40ProgGetClkDomainPosByIdx(pDomain40Prog, pVfRel->railIdx);
    FLCN_STATUS status = FLCN_OK;

    // Validate clock position in VF tuple.
    if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto clkVfPoint40Accessor_exit;
    }

    // Sanity check.
    if ((pBaseVFTuple     == NULL) ||
        (pOffsetedVFTuple == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkVfPoint40Accessor_exit;
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
        //
        // Call into type-specific implementations to see if other voltage types are
        // supported.
        //
        switch (BOARDOBJ_GET_TYPE(pVfPoint40))
        {
            case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
                {
                    status = clkVfPointVoltageuVGet_40_VOLT(&pVfPoint40->super,
                                                            voltageType,
                                                            &pVfPair->voltageuV);
                }
                break;
            }
            case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
            {
                status = clkVfPointVoltageuVGet_40_VOLT(&pVfPoint40->super,
                                                        voltageType,
                                                        &pVfPair->voltageuV);
                break;
            }
            default:
            {
                // Not Supported
                status = FLCN_ERR_NOT_SUPPORTED;
                break;
            }
        }
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPoint40Accessor_exit;
        }
    }

clkVfPoint40Accessor_exit:
    return status;
}

/*!
 * @Copydoc ClkVfPoint40FreqTupleAccessor
 */
FLCN_STATUS
clkVfPoint40FreqTupleAccessor
(
    CLK_VF_POINT_40                                *pVfPoint40,
    CLK_DOMAIN_40_PROG                             *pDomain40Prog,
    CLK_VF_REL                                     *pVfRel,
    LwU8                                            lwrveIdx,
    LwBool                                          bOffseted,
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE    *pOutput
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pVfPoint40))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI:
        {
            status = clkVfPoint40FreqTupleAccessor_VOLT_PRI(pVfPoint40,
                                                            pDomain40Prog,
                                                            pVfRel,
                                                            lwrveIdx,
                                                            bOffseted,
                                                            pOutput);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_SEC:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC))
            {
                status = clkVfPoint40FreqTupleAccessor_VOLT_SEC(pVfPoint40,
                                                                pDomain40Prog,
                                                                pVfRel,
                                                                lwrveIdx,
                                                                bOffseted,
                                                                pOutput);
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
        goto clkVfPoint40FreqTupleAccessor_exit;
    }

clkVfPoint40FreqTupleAccessor_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointVoltageuVGet
 */
FLCN_STATUS
clkVfPointVoltageuVGet_40
(
    CLK_VF_POINT  *pVfPoint,
    LwU8           voltageType,
    LwU32         *pVoltageuV
)
{
    CLK_VF_POINT_40 *pVfPoint40;
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple;
    FLCN_STATUS status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint40, &pVfPoint->super,
                                  CLK, CLK_VF_POINT, 40, &status,
                                  clkVfPointVoltageuVGet_40_exit);
    pBaseVFTuple = clkVfPoint40BaseVFTupleGet(pVfPoint40);

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

clkVfPointVoltageuVGet_40_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointClientFreqDeltaSet
 */
FLCN_STATUS
clkVfPointClientFreqDeltaSet_40
(
    CLK_VF_POINT                 *pVfPoint,
    LW2080_CTRL_CLK_FREQ_DELTA   *pFreqDelta
)
{
    CLK_VF_POINT_40 *pVfPoint40 = (CLK_VF_POINT_40 *)pVfPoint;

    pVfPoint40->clientFreqDelta = *pFreqDelta;

    return FLCN_OK;
}

/*!
 * @copydoc ClkVfPointClientVfTupleGet
 */
FLCN_STATUS
clkVfPointClientVfTupleGet_40
(
    CLK_VF_POINT                                  *pVfPoint,
    LwBool                                         bOffset,
    LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TUPLE     *pClientVfPointTuple
)
{
    CLK_VF_POINT_40                               *pVfPoint40 = (CLK_VF_POINT_40 *)pVfPoint;
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE         *pVfPointTuple;
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE    *pVfPointBaseTuple;
    FLCN_STATUS                                    status = FLCN_OK;

    if (bOffset)
    {
        pVfPointTuple = clkVfPoint40OffsetedVFTupleGet(pVfPoint40);
        pClientVfPointTuple->freqkHz   = pVfPointTuple->freqMHz * 1000;
        pClientVfPointTuple->voltageuV = pVfPointTuple->voltageuV;
    }
    else
    {
        LwU16               freqMHz;
        CLK_DOMAIN_40_PROG *pDomain40Prog = clkDomain40ProgGetByVfPointIdx(
                                                BOARDOBJ_GET_GRP_IDX(&pVfPoint->super),
                                                LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI);
        if (pDomain40Prog == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto clkVfPointClientVfTupleGet_40_exit;
        }

        pVfPointBaseTuple = clkVfPoint40BaseVFTupleGet(pVfPoint40);
        freqMHz           = pVfPointBaseTuple->freqTuple[CLK_VF_PRIMARY_POS].freqMHz;

        // Adjust base VF frequency with POR frequency offsets (GRD / AIC)
        status = clkDomainProgFactoryDeltaAdj(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog), &freqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointClientVfTupleGet_40_exit;
        }

        pClientVfPointTuple->freqkHz   = freqMHz * 1000;
        pClientVfPointTuple->voltageuV = pVfPointBaseTuple->voltageuV;
    }

clkVfPointClientVfTupleGet_40_exit:
    return status;
}

/* ------------------------- Private Functions ----------------------------- */
/*!
 * Helper interface to adjust the input VF point with Clock Domains and
 * Clock VF Relationship's OCOV tweaks.
 */
static FLCN_STATUS
s_clkVfPoint40OCOVAdjust
(
    CLK_VF_POINT_40                        *pVfPoint40,
    CLK_DOMAIN_40_PROG                     *pDomain40Prog,
    CLK_VF_REL                             *pVfRel,
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE  *pOffsetedVFTuple
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU16       origFreqMHz;
    LwU8        clkPos =
        clkDomain40ProgGetClkDomainPosByIdx(pDomain40Prog, pVfRel->railIdx);

    // Validate the position.
    if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto s_clkVfPoint40OCOVAdjust_exit;
    }
    origFreqMHz = pOffsetedVFTuple[clkPos].freqMHz;

    //
    // Do not perform voltage offset adjustment if clock domain
    // does not support it.
    //
    if (clkDomain40ProgIsOVEnabled(pDomain40Prog))
    {
        // Adjust with clock domain's and clock vf rel's voltage delta
        status = clkDomain40ProgVoltAdjustDeltauV(
                    pDomain40Prog,                          // pDomain40Prog
                    pVfRel,                                 // pVfRel
                    &pOffsetedVFTuple[clkPos].voltageuV);   // pVoltuV
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OCOVAdjust_exit;
        }
    }

    //
    // Do not perform frequency offset adjustment if clock domain
    // does not support it.
    //
    if (clkDomain40ProgIsOCEnabled(pDomain40Prog))
    {
        // Adjust with clock domain's and clock vf rel's freq delta
        status = clkDomain40ProgFreqAdjustDeltaMHz(
                    pDomain40Prog,                          // pDomain40Prog
                    pVfRel,                                 // pVfRel
                    &pOffsetedVFTuple[clkPos].freqMHz);     // pFreqMHz
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OCOVAdjust_exit;
        }
    }

    //
    // Trim the frequency delta to max allowed POR bounds unless
    // client explicitly override it through internal OVOC flag.
    // Always trim the frequency delta to remove any propagation
    // of frequency delta from primary -> secondary in cases where
    // primary supports frequency offset but secondaries does not.
    //
    if (!clkDomainsOverrideOVOCEnabled())
    {
        LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE *pBaseVFTuple =
            clkVfPoint40BaseVFTupleGet(pVfPoint40);

        if (pOffsetedVFTuple[clkPos].freqMHz > pBaseVFTuple->freqTuple[clkPos].freqMHz)
        {
            pOffsetedVFTuple[clkPos].freqMHz =
                LW_MIN(pOffsetedVFTuple[clkPos].freqMHz,
                       (pBaseVFTuple->freqTuple[clkPos].freqMHz +
                        clkDomain40ProgFreqDeltaMaxMHzGet(pDomain40Prog)));
        }
        else if (pOffsetedVFTuple[clkPos].freqMHz < pBaseVFTuple->freqTuple[clkPos].freqMHz)
        {
            pOffsetedVFTuple[clkPos].freqMHz =
                LW_MAX(pOffsetedVFTuple[clkPos].freqMHz,
                       (pBaseVFTuple->freqTuple[clkPos].freqMHz +
                        clkDomain40ProgFreqDeltaMinMHzGet(pDomain40Prog)));
        }
        else
        {
            // Nothing to do.
        }
    }

    // Quantize the offseted frequency
    if (origFreqMHz != pOffsetedVFTuple[clkPos].freqMHz)
    {
        LW2080_CTRL_CLK_VF_INPUT    inputFreq;
        LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

        // Init
        LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
        LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

        inputFreq.value = pOffsetedVFTuple[clkPos].freqMHz;
        status = clkDomainProgFreqQuantize_40(
                    CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                    &inputFreq,                                         // pInputFreq
                    &outputFreq,                                        // pOutputFreq
                    LW_TRUE,                                            // bFloor
                    LW_FALSE);                                          // bBound
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OCOVAdjust_exit;
        }
        pOffsetedVFTuple[clkPos].freqMHz = (LwU16) outputFreq.value;
    }

s_clkVfPoint40OCOVAdjust_exit:
    return status;
}

/*!
 * Helper interface to adjust the input VF point with Clock Domains and
 * Clock VF Relationship's OCOV tweaks.
 */
static FLCN_STATUS
s_clkVfPoint40OffsetVFOCOVAdjust
(
    CLK_VF_POINT_40                     *pVfPoint40,
    CLK_DOMAIN_40_PROG                  *pDomain40Prog,
    CLK_VF_REL                          *pVfRel,
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE     *pOffsetVFTuple
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        clkPos =
        clkDomain40ProgGetClkDomainPosByIdx(pDomain40Prog, pVfRel->railIdx);

    // Validate the position.
    if (clkPos == LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto s_clkVfPoint40OffsetVFOCOVAdjust_exit;
    }

    //
    // Do not perform voltage offset adjustment if clock domain
    // does not support it.
    //
    if (clkDomain40ProgIsOVEnabled(pDomain40Prog))
    {
        // Adjust with clock domain's and clock vf rel's voltage delta
        status = clkDomain40ProgOffsetVFVoltAdjustDeltauV(
                    pDomain40Prog,                          // pDomain40Prog
                    pVfRel,                                 // pVfRel
                    &pOffsetVFTuple[clkPos].voltageuV);     // pVoltuV
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OffsetVFOCOVAdjust_exit;
        }
    }

    //
    // Do not perform frequency offset adjustment if clock domain
    // does not support it.
    //
    if (clkDomain40ProgIsOCEnabled(pDomain40Prog))
    {
        // Adjust with clock domain's and clock vf rel's freq delta
        status = clkDomain40ProgOffsetVFFreqAdjustDeltaMHz(
                    pDomain40Prog,                        // pDomain40Prog
                    pVfRel,                               // pVfRel
                    &pOffsetVFTuple[clkPos].freqMHz);     // pFreqMHz
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_clkVfPoint40OffsetVFOCOVAdjust_exit;
        }
    }
    // TODO:
    // Quantize non zero value, but lwrrently quantization code might quantize to 0 if negative

s_clkVfPoint40OffsetVFOCOVAdjust_exit:
    return status;
}

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
