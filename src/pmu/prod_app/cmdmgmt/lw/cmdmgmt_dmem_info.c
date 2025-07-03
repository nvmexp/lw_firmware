/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  cmdmgmt_dmem_info.c
 * @brief Implements an RPC to retrieve information regarding the PMU's run-time
 *        DMEM usage
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwos_ovl_priv.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "g_pmurpc.h"

#ifdef UPROC_RISCV
#include "config/g_sections_riscv.h"
#endif // UPROC_RISCV

/* ------------------------ External Definitions ---------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Retrieves DMEM overlay info.
 */
FLCN_STATUS
pmuRpcCmdmgmtDmemInfoGet(RM_PMU_RPC_STRUCT_CMDMGMT_DMEM_INFO_GET *pParams)
{
    LwU32   ovl;
    LwU32   cnt;

    pParams->ovlCount = OVL_COUNT_DMEM();

    // Expose info for those entries that we have space for in the API.
    cnt = LW_MIN(pParams->ovlCount, LW85B6_CTRL_PMU_DMEM_INFO_OVL_COUNT_MAX); 

    for (ovl = 0; ovl < cnt; ovl++)
    {
#ifdef UPROC_RISCV
        pParams->ovlAddrStart[ovl]   = SectionDescStartVirtual[ovl];
        pParams->ovlSizeMax[ovl]     = SectionDescMaxSize[ovl];
        pParams->ovlSizeLwrrent[ovl] = SectionDescHeapSize[ovl];
#else // UPROC_RISCV
        pParams->ovlAddrStart[ovl]   = _dmem_ovl_start_address[ovl];
        pParams->ovlSizeMax[ovl]     = _dmem_ovl_size_max[ovl];
        pParams->ovlSizeLwrrent[ovl] = _dmem_ovl_size_lwrrent[ovl];
#endif // UPROC_RISCV
    }

    return FLCN_OK;
}
