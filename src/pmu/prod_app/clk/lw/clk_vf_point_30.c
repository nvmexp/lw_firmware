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
 * @file clk_vf_point_30.c
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

/* ------------------------ Global Variables ------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different PSTATE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *ClkVfPoint30VirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, BASE)    ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 30)      ] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 30_FREQ) ] = CLK_VF_POINT_30_FREQ_VTABLE(),
    [LW2080_CTRL_BOARDOBJ_TYPE(CLK, CLK_VF_POINT, 30_VOLT) ] = CLK_VF_POINT_30_VOLT_VTABLE(),
};

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_POINT_30 handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10Set_30
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E512,            // _grpType
        CLK_VF_POINT_PRI,                           // _class
        pBuffer,                                    // _pBuffer
        clkVfPointIfaceModel10SetHeader,                  // _hdrFunc
        clkVfPointIfaceModel10SetEntry,                   // _entryFunc
        pascalAndLater.clk.clkVfPointGrpSet.pri,    // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        ClkVfPoint30VirtualTables);                 // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointBoardObjGrpIfaceModel10Set_30_exit;
    }

clkVfPointBoardObjGrpIfaceModel10Set_30_exit:
    return status;
}

/*!
 * CLK_VF_POINT_30 handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkVfPointBoardObjGrpIfaceModel10GetStatus_30
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
        goto clkVfPointBoardObjGrpIfaceModel10GetStatus_30_exit;
    }

clkVfPointBoardObjGrpIfaceModel10GetStatus_30_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clkVfPointIfaceModel10GetStatus_30
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLK_VF_POINT_30_BOARDOBJ_GET_STATUS *pVfPoint30Status =
        (RM_PMU_CLK_CLK_VF_POINT_30_BOARDOBJ_GET_STATUS *)pBuf;
    CLK_VF_POINT_30    *pVfPoint30;
    FLCN_STATUS         status      = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint30, pBoardObj, CLK, CLK_VF_POINT, 30,
                                  &status, clkVfPointIfaceModel10GetStatus_30_exit);

    status = clkVfPointIfaceModel10GetStatus_SUPER(pModel10, pBuf);
    if (status != FLCN_OK)
    {
        goto clkVfPointIfaceModel10GetStatus_30_exit;
    }

    pVfPoint30Status->pair = pVfPoint30->pair;

clkVfPointIfaceModel10GetStatus_30_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint30Cache
 */
FLCN_STATUS
clkVfPoint30Cache
(
    CLK_VF_POINT_30            *pVfPoint30,
    CLK_DOMAIN_30_PRIMARY       *pDomain30Primary,
    CLK_PROG_30_PRIMARY         *pProg30Primary,
    LwU8                        voltRailIdx,
    LW2080_CTRL_CLK_VF_PAIR    *pVfPairLast,
    LW2080_CTRL_CLK_VF_PAIR    *pBaseVFPairLast
)
{
    FLCN_STATUS status = FLCN_ERROR;

    // Initialize the current base VF pair to (0,0)
    LW2080_CTRL_CLK_VF_PAIR baseVFPairLwrr = LW2080_CTRL_CLK_VF_PAIR_INIT();

    switch (BOARDOBJ_GET_TYPE(pVfPoint30))
    {
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_FREQ:
        {
            status = clkVfPoint30Cache_FREQ(pVfPoint30, pDomain30Primary,
                        pProg30Primary, voltRailIdx, pVfPairLast,
                        pBaseVFPairLast, &baseVFPairLwrr);
            break;
        }
        case LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_30_VOLT:
        {
            status = clkVfPoint30Cache_VOLT(pVfPoint30, pDomain30Primary,
                        pProg30Primary, voltRailIdx, pVfPairLast,
                        pBaseVFPairLast, &baseVFPairLwrr);
            break;
        }
    }

    // Catch any errors in caching above.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPoint30Cache_exit;
    }

    //
    // If enabled, enforce monotonicity of VF lwrve to ensure that this VF point
    // is >= the last VF point per the provided LW2080_CTRL_CLK_VF_PAIR pointer.
    //
    if (clkDomainsVfMonotonicityEnforced())
    {
        LW2080_CTRL_CLK_VF_PAIR_LAST_MAX(clkVfPoint30PairGet(pVfPoint30),
            pVfPairLast);
    }

    LW2080_CTRL_CLK_VF_PAIR_COPY(clkVfPoint30PairGet(pVfPoint30),
            pVfPairLast);
    LW2080_CTRL_CLK_VF_PAIR_COPY((&baseVFPairLwrr),
            pBaseVFPairLast);

clkVfPoint30Cache_exit:
    return status;
}

/*!
 * @copydoc ClkVfPoint30Smoothing
 */
FLCN_STATUS
clkVfPoint30Smoothing
(
    CLK_VF_POINT_30            *pVfPoint30,
    CLK_DOMAIN_30_PRIMARY       *pDomain30Primary,
    CLK_PROG_30_PRIMARY         *pProg30Primary,
    LwU16                      *pFreqMHzExpected
)
{
    LwU32       voltageuV   =
        LW2080_CTRL_CLK_VF_PAIR_VOLTAGE_UV_GET(clkVfPoint30PairGet(pVfPoint30));
    LwU16       freqMHz     =
        LW2080_CTRL_CLK_VF_PAIR_FREQ_MHZ_GET(clkVfPoint30PairGet(pVfPoint30));
    FLCN_STATUS status      = FLCN_OK;

    // If VF lwrve smoothing is not required, bail early
    if (!((clkProg3XPrimaryIsVFSmoothingRequired(&pProg30Primary->primary, voltageuV)) &&
          (LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL == (pProg30Primary)->super.super.source)))
    {
        goto clkVfPointSmoothing_30_exit;
    }

    if (*pFreqMHzExpected > freqMHz)
    {
        // set the current VF point frequency to expected frequency
        freqMHz = *pFreqMHzExpected;

        //
        // Quantize up the frequency to ensure that quantized frequency
        // is within the allowed bound
        //
        status = clkProg3XFreqMHzQuantize(
                    &pProg30Primary->super.super,            // pProg3X
                    ((CLK_DOMAIN_3X_PROG *)pDomain30Primary), // pDomain3XProg
                    &freqMHz,                               // pFreqMHz
                    LW_FALSE);                              // bFloor
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkVfPointSmoothing_30_exit;
        }

        // Update the VF pair
        clkVfPoint30FreqMHzSet(pVfPoint30, freqMHz);
    }

    // Update expected frequency MHz for next VF point.
    if (freqMHz >= clkProg3XPrimaryMaxFreqStepSizeMHzGet(&pProg30Primary->primary))
    {
        *pFreqMHzExpected =
            freqMHz - clkProg3XPrimaryMaxFreqStepSizeMHzGet(&pProg30Primary->primary);
    }
    else
    {
        *pFreqMHzExpected = 0;
    }


clkVfPointSmoothing_30_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointAccessor
 */
FLCN_STATUS
clkVfPointAccessor_30
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
    CLK_VF_POINT_30    *pVfPoint30;
    CLK_DOMAIN_3X_PROG *pDomain3XProg = (CLK_DOMAIN_3X_PROG *)
        INTERFACE_TO_BOARDOBJ_CAST(pDomain3XPrimary);
    LW2080_CTRL_CLK_VF_PAIR vfPair;
    FLCN_STATUS             status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint30, &pVfPoint->super, CLK, CLK_VF_POINT, 30,
                                  &status, clkVfPointAccessor_30_exit);

    // v30 only store offseted VF lwrve.
    PMU_HALT_COND(bOffseted);

    // Set the pDomain3XSecondary pointer if index is specified.
    if (BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain3XProg->super.super.super.super) != clkDomIdx)
    {
        pDomain3XProg  = (CLK_DOMAIN_3X_PROG *)
            BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, clkDomIdx);
        if (pDomain3XProg == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto clkVfPointAccessor_30_exit;
        }
    }

    //
    // Retrieve voltage for this CLK_VF_POINT.
    //
    status = clkVfPointVoltageuVGet(pVfPoint, voltageType, &vfPair.voltageuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointAccessor_30_exit;
    }

    // Offset the voltage by the various CLK_DOMAIN_3X_PROG deltas.
    status = clkDomain3XProgVoltAdjustDeltauV(pDomain3XProg,
                pProg3XPrimary, voltRailIdx, &vfPair.voltageuV);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointAccessor_30_exit;
    }

    // Retrieve the frequency for this CLK_VF_POINT.
    vfPair.freqMHz = clkVfPoint30FreqMHzGet(pVfPoint30);

    // Offset the frequency by the various CLK_DOMAIN_3X_PRIMARY deltas.
    status = clkDomain3XPrimaryFreqAdjustDeltaMHz(pDomain3XPrimary, pProg3XPrimary,
                LW_TRUE, LW_FALSE, &vfPair.freqMHz);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointAccessor_30_exit;
    }

    //
    // Colwert the frequency for this CLK_VF_POINT primary -> secondary. Various
    // classes will apply the PRIMARY CLK_DOMAIN's delta as necessary.
    //
    status = clkProg3XPrimaryFreqTranslatePrimaryToSecondary(pProg3XPrimary,
                    pDomain3XPrimary,    // pDomain3XPrimary
                    clkDomIdx,          // clkDomIdx
                    &vfPair.freqMHz,    // pFreqMHz
                    LW_TRUE,            // bReqFreqDeltaAdj
                    LW_TRUE);           // bQuantize
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkVfPointAccessor_30_exit;
    }

    //
    // Copy out the adjusted VF_PAIR to the caller.
    //
    // A follow-up CL will replace this with the @ref
    // LW2080_CTRL_CLK_VF_PAIR_LAST_MAX.
    //
    *pVfPair = vfPair;

clkVfPointAccessor_30_exit:
    return status;
}

/*!
 * @copydoc ClkVfPointVoltageuVGet
 */
FLCN_STATUS
clkVfPointVoltageuVGet_30
(
    CLK_VF_POINT  *pVfPoint,
    LwU8           voltageType,
    LwU32         *pVoltageuV
)
{
    CLK_VF_POINT_30 *pVfPoint30;
    FLCN_STATUS      status;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pVfPoint30, &pVfPoint->super, CLK, CLK_VF_POINT, 30,
                                  &status, clkVfPointVoltageuVGet_30_exit);

    // If POR value requested, return that value from the SUPER structure.
    if (LW2080_CTRL_CLK_VOLTAGE_TYPE_POR == voltageType)
    {
        *pVoltageuV = pVfPoint30->pair.voltageuV;
        status = FLCN_OK;
    }
    else
    {
        status = FLCN_ERR_NOT_SUPPORTED;
    }

clkVfPointVoltageuVGet_30_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
