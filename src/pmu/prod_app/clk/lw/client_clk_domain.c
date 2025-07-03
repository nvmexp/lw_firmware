/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file client_clk_domain.c
 *
 * @brief Client clock domain function implementations
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "dmemovl.h"
#include "clk/client_clk_domain.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clientClkDomainIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clientClkDomainIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clientClkDomainIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clientClkDomainIfaceModel10SetEntry");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLIENT_CLK_DOMAIN handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clientClkDomainBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        CLIENT_CLK_DOMAIN,                          // _class
        pBuffer,                                    // _pBuffer
        s_clientClkDomainIfaceModel10SetHeader,           // _hdrFunc
        s_clientClkDomainIfaceModel10SetEntry,            // _entryFunc
        ga10xAndLater.clk.clientClkDomainGrpSet,    // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

    if (status != FLCN_OK) 
    {
        PMU_BREAKPOINT();
        goto clientClkDomainBoardObjGrpIfaceModel10Set_exit;
    }

clientClkDomainBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLIENT_CLK_DOMAIN super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clientClkDomainGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLIENT_CLK_DOMAIN_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLIENT_CLK_DOMAIN_BOARDOBJ_SET *)pBoardObjDesc;
    CLIENT_CLK_DOMAIN      *pClientClkDomain;
    FLCN_STATUS             status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkDomainGrpIfaceModel10ObjSet_SUPER_exit;
    }

    pClientClkDomain = (CLIENT_CLK_DOMAIN *)*ppBoardObj;

    // Set CLIENT_CLK_DOMAIN state
    pClientClkDomain->domainIdx = pSet->domainIdx;

clientClkDomainGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLIENT_CLK_DOMAIN implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clientClkDomainIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    FLCN_STATUS         status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clientClkDomainIfaceModel10SetHeader_exit;
    }

s_clientClkDomainIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * Constructs a CLIENT_CLK_DOMAIN of the corresponding type.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clientClkDomainIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_CLIENT_CLK_DOMAIN_TYPE_FIXED:
        {
            status = clientClkDomainGrpIfaceModel10ObjSet_FIXED(pModel10, ppBoardObj, 
                sizeof(CLIENT_CLK_DOMAIN_FIXED), pBuf);
            break;
        }
        case LW2080_CTRL_CLK_CLIENT_CLK_DOMAIN_TYPE_PROG:
        {
            status = clientClkDomainGrpIfaceModel10ObjSet_PROG(pModel10, ppBoardObj,
                sizeof(CLIENT_CLK_DOMAIN_PROG), pBuf);
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
        goto s_clientClkDomainIfaceModel10SetEntry_exit;
    }

s_clientClkDomainIfaceModel10SetEntry_exit:
    return status;
}
