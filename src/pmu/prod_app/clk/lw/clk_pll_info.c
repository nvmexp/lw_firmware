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
 * @file   clk_pll_info.c
 * @brief  File for CLK PLL.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "clk/clk_pll_info.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkPllDevIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPLLDevConstructHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkPllDevIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkPllDevConstructEntry");
/*!
 * PLL_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkPllDevBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
        PLL_DEVICE,                                 // _class
        pBuffer,                                    // _pBuffer
        s_clkPllDevIfaceModel10SetHeader,                 // _hdrFunc
        s_clkPllDevIfaceModel10SetEntry,                  // _entryFunc
        pascalAndLater.clk.clkPllDeviceGrpSet,      // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        goto clkPllDevBoardObjGrpIfaceModel10Set_exit;
    }

clkPllDevBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPllDevGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PLL_DEVICE                         *pDev     = NULL;
    FLCN_STATUS                         status;
    RM_PMU_CLK_PLL_DEVICE_BOARDOBJ_SET *pTmpDesc =
        (RM_PMU_CLK_PLL_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPllDevGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    pDev = (PLL_DEVICE *)*ppBoardObj;

    pDev->id        = pTmpDesc->id;
    pDev->MaxRef    = pTmpDesc->MaxRef;
    pDev->MinRef    = pTmpDesc->MinRef;
    pDev->MilwCO    = pTmpDesc->MilwCO;
    pDev->MaxVCO    = pTmpDesc->MaxVCO;
    pDev->MinUpdate = pTmpDesc->MinUpdate;
    pDev->MaxUpdate = pTmpDesc->MaxUpdate;
    pDev->MinM      = pTmpDesc->MinM;
    pDev->MaxM      = pTmpDesc->MaxM;
    pDev->MinN      = pTmpDesc->MinN;
    pDev->MaxN      = pTmpDesc->MaxN;
    pDev->MinPl     = pTmpDesc->MinPl;
    pDev->MaxPl     = pTmpDesc->MaxPl;

clkPllDevGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * PLL_DEV implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkPllDevIfaceModel10SetHeader
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
        goto s_clkPllDevIfaceModel10SetHeader_exit;
    }

s_clkPllDevIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * Construct a PLL_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkPllDevIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status;

    // Construct and initialize parent-class object.
    status = clkPllDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, sizeof(PLL_DEVICE), pBuf);
    if (status != FLCN_OK)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}
