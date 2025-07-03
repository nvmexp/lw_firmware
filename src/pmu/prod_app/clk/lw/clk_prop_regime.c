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
 * @file clk_prop_regime.c
 *
 * @brief Module managing all state related to the CLK_PROP_REGIME structure.
 *
 * @ref - https://confluence.lwpu.com/display/RMPER/Clock+Propagation
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkPropRegimeIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkPropRegimeIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkPropRegimeIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkPropRegimeIfaceModel10SetEntry");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP_REGIME handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkPropRegimeBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    LwBool      bFirstTime = (Clk.propRegimes.super.super.objSlots == 0U);

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
        CLK_PROP_REGIME,                            // _class
        pBuffer,                                    // _pBuffer
        s_clkPropRegimeIfaceModel10SetHeader,             // _hdrFunc
        s_clkPropRegimeIfaceModel10SetEntry,              // _entryFunc
        ga10xAndLater.clk.clkPropRegimeGrpSet,      // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropRegimeBoardObjGrpIfaceModel10Set_exit;
    }

    // Ilwalidate on SET CONTROL.
    if (!bFirstTime)
    {
        // Ilwalidate PMU arbiter if supported.
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
        {
            //
            // Detach VFE IMEM and DMEM as it's not required anymore
            // and it's not possible to accommidate PERF, PERF_LIMIT,
            // and VFE DMEM/IMEM at the same time.
            //
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, perfVfe)
                OSTASK_OVL_DESC_BUILD(_DMEM, _DETACH, perfVfe)
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLw64Umul)
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLwF32)
                PERF_LIMIT_OVERLAYS_BASE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                // Skip further processing if perf_limits has not been inititalized
                if (!BOARDOBJGRP_IS_EMPTY(PERF_LIMIT))
                {
                    status = perfLimitsIlwalidate(PERF_PERF_LIMITS_GET(),
                                LW_TRUE,
                                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
                }
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkPropRegimeBoardObjGrpIfaceModel10Set_exit;
            }
        }
    }

clkPropRegimeBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLK_PROP_REGIME class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropRegimeGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_REGIME_BOARDOBJ_SET *pPropRegimeSet =
        (RM_PMU_CLK_CLK_PROP_REGIME_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP_REGIME *pPropRegime;
    FLCN_STATUS      status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropRegimeGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pPropRegime = (CLK_PROP_REGIME *)*ppBoardObj;

    // Copy the CLK_PROP_REGIME-specific data.
    pPropRegime->regimeId = pPropRegimeSet->regimeId;

    // Init and copy in the mask.
    boardObjGrpMaskInit_E32(&(pPropRegime->clkDomainMask));
    status = boardObjGrpMaskImport_E32(&(pPropRegime->clkDomainMask),
                                       &(pPropRegimeSet->clkDomainMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropRegimeGrpIfaceModel10ObjSet_SUPER_exit;
    }

clkPropRegimeGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc ClkPropRegimesGetByRegimeId
 */
CLK_PROP_REGIME *
clkPropRegimesGetByRegimeId
(
    LwU8    regimeId
)
{
    CLK_PROP_REGIME *pPropRegime = 
        CLK_PROP_REGIME_GET((CLK_CLK_PROP_REGIMES_GET())->regimeIdToIdxMap[regimeId]);

    if ((pPropRegime == NULL) ||
        (pPropRegime->regimeId != regimeId))
    {
        PMU_BREAKPOINT();
        return NULL;
    }

    return pPropRegime;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_PROP_REGIME implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkPropRegimeIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROP_REGIME_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_CLK_CLK_PROP_REGIME_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLK_PROP_REGIMES   *pPropRegimes = (CLK_PROP_REGIMES *)pBObjGrp;
    FLCN_STATUS         status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkPropRegimeIfaceModel10SetHeader_exit;
    }

    // Copy global CLK_PROP_REGIME state.
    pPropRegimes->regimeHal = pSet->regimeHal;

    (void)memcpy(pPropRegimes->regimeIdToIdxMap, pSet->regimeIdToIdxMap,
         (sizeof(LwBoardObjIdx) * LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_MAX));

s_clkPropRegimeIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_PROP_REGIME implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkPropRegimeIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBoardObjSet->type)
    {
        case LW2080_CTRL_CLK_CLK_PROP_REGIME_TYPE_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_REGIME))
            {
                status = clkPropRegimeGrpIfaceModel10ObjSet_SUPER(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_PROP_REGIME),
                                  pBoardObjSet);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
