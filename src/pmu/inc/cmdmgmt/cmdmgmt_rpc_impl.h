/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_RPC_IMPL_H
#define CMDMGMT_RPC_IMPL_H

#include "g_cmdmgmt_rpc_impl.h"

#ifndef G_CMDMGMT_RPC_IMPL_H
#define G_CMDMGMT_RPC_IMPL_H

/*!
 * @file    cmdmgmt_rpc_impl.h
 * @brief   Declarations for implementing RM-to-PMU and PMU-to-RM RPC
 *          functionality
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "unit_api.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Macro to simplify calling PMU to RM RPC-s.
 *
 * @copydoc pmuRmRpcExelwte
 *
 * @note Because this macro doesn't dereference the arguments to _pRpc, parts of
 * the struct may look unused to compilers/Coverity. To avoid that causing
 * errors, note that we dereference the entirety of _pRpc and cast it to void.
 *
 * @param[in]   _stat       LValue to hold status of the RPC's exelwtion.
 * @param[in]   _unit       Name (string) of unit implementing RPC function.
 * @param[in]   _func       Name (string) of RPC's function.
 * @param[in]   _pRpc       RPC structure pointer.
 */
#define PMU_RM_RPC_EXELWTE(_stat, _unit, _func, _pRpc, bBlocking)              \
    do {                                                                       \
        (void)(*(_pRpc));                                                      \
        (_pRpc)->hdr.flags = DRF_DEF(_RM_PMU, _RPC_FLAGS, _PMU_RESPONSE, _NO); \
        (_pRpc)->hdr.unitId = RM_PMU_UNIT_##_unit;                             \
        (_pRpc)->hdr.function = PMU_RM_RPC_ID_##_unit##_##_func;               \
        (_stat) = pmuRmRpcExelwte(&(_pRpc)->hdr,                               \
            sizeof(PMU_RM_RPC_STRUCT_##_unit##_##_func), bBlocking);           \
        lwosVarNOP(_stat);                                                     \
    } while (LW_FALSE)

/*!
 * @copydoc PMU_RM_RPC_EXELWTE()
 *
 * @note    This interface will block until the RPC can be initiated.
 */
#define PMU_RM_RPC_EXELWTE_BLOCKING(_stat, _unit, _func, _pRpc)               \
    PMU_RM_RPC_EXELWTE(_stat, _unit, _func, _pRpc, LW_TRUE)

/*!
 * @copydoc PMU_RM_RPC_EXELWTE()
 *
 * @note    This interface will return an error if the RPC cannot be initiated
 *          immediately.
 */
#define PMU_RM_RPC_EXELWTE_NON_BLOCKING(_stat, _unit, _func, _pRpc)           \
    PMU_RM_RPC_EXELWTE(_stat, _unit, _func, _pRpc, LW_FALSE)

/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Wrapper for the conditional IMEM overlay attachment macro.
 *
 * Introduced to reduce code size and assigned into resident code since its
 * referenced by every task's unit dispatcher utilizing RPC infrastructure.
 */
LwU8 pmuRpcAttachAndLoadImemOverlayCond(LwU8 ovlIndex)
    GCC_ATTRIB_SECTION("imem_resident", "pmuRpcAttachAndLoadImemOverlayCond");

/*!
 * @breif   "Completes" processing of an RM RPC by responding appopriately
 *
 * @param[in]   pRequest    Dispatch structure used for the original request
 */
void cmdmgmtRmRpcRespond(DISP2UNIT_RM_RPC *pRequest)
    GCC_ATTRIB_SECTION("imem_resident", "cmdmgmtRmRpcRespond");

/*!
 * @breif   "Completes" processing of an RM command by responding appopriately
 *
 * @param[in]   pRequest    Dispatch structure used for the original request
 * @param[in]   pMsgHdr     Header to use for message response
 * @param[in]   pMsgBody    Body to use for message response
 */
void cmdmgmtRmCmdRespond(DISP2UNIT_RM_RPC *pRequest, RM_FLCN_QUEUE_HDR *pMsgHdr, const void *pMsgBody)
    GCC_ATTRIB_SECTION("imem_resident", "cmdmgmtRmCmdRespond");

mockable FLCN_STATUS pmuRmRpcExelwte(RM_PMU_RPC_HEADER *pRpc, LwU32 rpcSize, LwBool bBlock)
    GCC_ATTRIB_SECTION("imem_resident", "pmuRmRpcExelwte");

/* ------------------------- Global Variables ------------------------------ */

#endif // G_CMDMGMT_RPC_IMPL_H
#endif // CMDMGMT_RPC_IMPL_H
