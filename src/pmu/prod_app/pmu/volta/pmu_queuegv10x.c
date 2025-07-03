/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_queuegv10x.c
 * @brief   GV10X+ functions implementing PMU<->FBFLCN message exchange.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_fbfalcon_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_bar0.h"
#include "pmu_fbflcn_if.h"
#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * Mutex allowing caller task to wait for the FBFLCN response.
 */
LwrtosSemaphoreHandle FbflcnRpcMutex = NULL;

/*!
 * Keeps a unique ID of a FBFLCN RPC lwrrently in flight.
 */
static LwU8 FbflcnRpcSeqId = 0;

/*!
 * Set if a FBFLCN request is lwrrently in flight,
 */
static LwBool FbflcnBRpcInFlight = LW_FALSE;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_pmuFbflcnRequestPost_GV10X(LwU8 cmd, LwU16 data16, LwU32 data32);
static FLCN_STATUS s_pmuFbflcnResponseGet_GV10X(LwU8 cmd, LwU16 *pData16, LwU32 *pData32);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief    Execute any FBFLCN's RPC.
 *
 * @param[in]       cmd         ID of the requested FBFLCN's RPC.
 * @param[in/out]   pData16     Request/response payload (16-bit part).
 * @param[in/out]   pData32     Request/response payload (32-bit part).
 * @param[in]       timeoutns   Timeout to be used when triggering the RPC.
 *
 * @return   Status of the exelwtion.
 */
FLCN_STATUS
pmuFbflcnRpcExelwte_GV10X
(
    LwU8    cmd,
    LwU16  *pData16,
    LwU32  *pData32,
    LwU32   timeoutus
)
{
    FLCN_STATUS status;

    status = s_pmuFbflcnRequestPost_GV10X(cmd, *pData16, *pData32);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuFbflcnRpcExelwte_GV10X_exit;
    }

    // Wait for an acknowledgment from FB falcon.
    status = lwrtosSemaphoreTake(FbflcnRpcMutex, LWRTOS_TIME_US_TO_TICKS(timeoutus));
    if (status != FLCN_OK)
    {
        // We've either timed out waiting on FB falcon or corrupted semaphore.
        PMU_BREAKPOINT();
        goto pmuFbflcnRpcExelwte_GV10X_exit;
    }

    status = s_pmuFbflcnResponseGet_GV10X(cmd, pData16, pData32);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pmuFbflcnRpcExelwte_GV10X_exit;
    }

pmuFbflcnRpcExelwte_GV10X_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Queues a request to the FBFLCN.
 */
static FLCN_STATUS
s_pmuFbflcnRequestPost_GV10X
(
    LwU8    cmd,
    LwU16   data16,
    LwU32   data32
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       head;
    LwU32       tail;

    // Only one FBFLCN request can be in flight.
    if (FbflcnBRpcInFlight)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pmuFbflcnRequestPost_GV10X_exit;
    }

    //
    // As long as request are issued by a single task (PERF_DAEMON) there is no
    // need to worry about atomic access to this flag.
    //
    FbflcnBRpcInFlight = LW_TRUE;

    if (cmd >= PMU_FBFLCN_CMD_ID__COUNT)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_pmuFbflcnRequestPost_GV10X_exit;
    }

    head = FLD_SET_DRF_NUM(_PMU_FBFLCN, _HEAD, _SEQ, ++FbflcnRpcSeqId, 0);
    head = FLD_SET_DRF_NUM(_PMU_FBFLCN, _HEAD, _CMD, cmd, head);
    head = FLD_SET_DRF(_PMU_FBFLCN, _HEAD, _CYA, _NO, head);
    head = FLD_SET_DRF_NUM(_PMU_FBFLCN, _HEAD, _DATA16, data16, head);
    tail = FLD_SET_DRF_NUM(_PMU_FBFLCN, _TAIL, _DATA32, data32, 0);

    //
    // Assure that HEAD and TAIL are always different in cases where write to
    // HEAD register will trigger interrupt only if it differs from TAIL.
    // A single-bit difference is sufficient.
    //
    if (head == tail)
    {
        head = FLD_SET_DRF(_PMU_FBFLCN, _HEAD, _CYA, _YES, head);
    }

    REG_WR32(BAR0, LW_PFBFALCON_CMD_QUEUE_TAIL(FBFLCN_CMD_QUEUE_IDX_PMU), tail);
    // HEAD must be updated last since WR triggers an interrupt to the FBFLCN.
    REG_WR32(BAR0, LW_PFBFALCON_CMD_QUEUE_HEAD(FBFLCN_CMD_QUEUE_IDX_PMU), head);

s_pmuFbflcnRequestPost_GV10X_exit:
    return status;
}

/*!
 * @brief   Retrieves a response from the FBFLCN.
 */
static FLCN_STATUS
s_pmuFbflcnResponseGet_GV10X
(
    LwU8    cmd,
    LwU16  *pData16,
    LwU32  *pData32
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       head;
    LwU32       tail;

    if ((pData16 == NULL) || (pData32 == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto s_pmuFbflcnResponseGet_GV10X_exit;
    }

    // Response is not valid when request is not in flight.
    if (!FbflcnBRpcInFlight)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pmuFbflcnResponseGet_GV10X_exit;
    }

    head = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(PMU_CMD_QUEUE_IDX_FBFLCN));
    tail = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_TAIL(PMU_CMD_QUEUE_IDX_FBFLCN));

    // FBFLCN must respond to all PMU requests.
    if (FbflcnRpcSeqId != DRF_VAL(_FBFLCN_PMU, _HEAD, _SEQ, head))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pmuFbflcnResponseGet_GV10X_exit;
    }

    if (cmd != DRF_VAL(_FBFLCN_PMU, _HEAD, _CMD, head))
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto s_pmuFbflcnResponseGet_GV10X_exit;
    }

    *pData16 = DRF_VAL(_FBFLCN_PMU, _HEAD, _DATA16, head);
    *pData32 = DRF_VAL(_FBFLCN_PMU, _TAIL, _DATA32, tail);

    FbflcnBRpcInFlight = LW_FALSE;

s_pmuFbflcnResponseGet_GV10X_exit:
    return status;
}
