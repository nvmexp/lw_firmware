/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

/* ------------------------- Application Includes --------------------------- */
#include "task_i2c.h"
#include "pmu_obji2c.h"
#include "flcnifi2c.h"
#include "pmu_objpmgr.h"
#include "pmu_selwrity.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

// Wrapper for the generated I2C RPC source file.
#include "rpcgen/g_pmurpc_i2c.c"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Function Prototypes --------------------- */
static FLCN_STATUS  s_i2cProcess(RM_PMU_I2C_CMD_GENERIC *pGenCmd, LwBool bRead,
                                 void *pDmemBuf, LwU8 segment);
static LwBool       s_i2cAccessIsAllowed(I2C_PMU_TRANSACTION *pTa);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Handles RM_PMU_RPC_ID_I2C_TX_QUEUE
 *
 * RPC to perform a I2C write command.
 *
 * @param[in]   pParams Pointer to RM_PMU_RPC_STRUCT_I2C_TX_QUEUE
 * @param[in]   pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcI2cTxQueue
(
    RM_PMU_RPC_STRUCT_I2C_TX_QUEUE *pParams,
    PMU_DMEM_BUFFER                *pBuffer
)
{
    FLCN_STATUS status;

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if (pBuffer->size < LW_ALIGN_UP(pParams->txCmd.bufSize, sizeof(LwU32)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto pmuRpcI2cTxQueue_exit;
    }
#endif

    status = s_i2cProcess(&(pParams->txCmd), LW_FALSE, pBuffer->pBuf, 0);
    goto pmuRpcI2cTxQueue_exit;

pmuRpcI2cTxQueue_exit:
    return status;
}

/*!
 * @brief   Handles RM_PMU_RPC_ID_I2C_RX_QUEUE
 *
 * RPC to perform a I2C read command.
 *
 * @param[in]   pParams Pointer to RM_PMU_RPC_STRUCT_I2C_RX_QUEUE
 * @param[in]   pBuffer Scratch buffer address and size structure
 */
FLCN_STATUS
pmuRpcI2cRxQueue
(
    RM_PMU_RPC_STRUCT_I2C_RX_QUEUE *pParams,
    PMU_DMEM_BUFFER                *pBuffer
)
{
    FLCN_STATUS status;

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if (pBuffer->size < LW_ALIGN_UP(pParams->rxCmd.bufSize, sizeof(LwU32)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto pmuRpcI2cRxQueue_exit;
    }
#endif

    status =
        s_i2cProcess(&(pParams->rxCmd), LW_TRUE, pBuffer->pBuf, pParams->segment);
    goto pmuRpcI2cRxQueue_exit;

pmuRpcI2cRxQueue_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Process an I2C read/write transaction request
 *
 * @param[in]   pGenCmd   pointer to RM_PMU_I2C_CMD_GENERIC
 * @param[in]   bRead     Boolean to indicate a read or write transaction
 * @param[in]   pDmemBuf  pointer to the buffer to read from/ write to in DMEM.
 * @param[in]   segment   segment value for a read transaction
 */
static FLCN_STATUS
s_i2cProcess
(
    RM_PMU_I2C_CMD_GENERIC *pGenCmd,
    LwBool                  bRead,
    void                   *pDmemBuf,
    LwU8                    segment
)
{
    I2C_PMU_TRANSACTION ta = {0};
    FLCN_STATUS         status;
    LwU32               alignedReqSize;

    // If a request for block protocol, ensure that size can be supported.
    if (FLD_TEST_DRF(_RM_FLCN, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED,
                     pGenCmd->flags) &&
        ((pGenCmd->bufSize < RM_PMU_I2C_BLOCK_PROTOCOL_MIN) ||
         (pGenCmd->bufSize > RM_PMU_I2C_BLOCK_PROTOCOL_MAX)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_i2cProcess_exit;
    }

    ta.pGenCmd          = pGenCmd;
    ta.bRead            = bRead;
    alignedReqSize      = LW_ALIGN_UP((pGenCmd->bufSize), (bRead ?
                                DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_WRITE) :
                                DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_READ)));
    ta.pMessage         = pDmemBuf;
    ta.messageLength    = (LwU16)pGenCmd->bufSize;
    ta.segment          = segment;

    if ((PMUCFG_FEATURE_ENABLED(PMU_PRIV_SEC)) &&
        (!s_i2cAccessIsAllowed(&ta)))
    {
        status = FLCN_ERR_ILLEGAL_OPERATION;
        goto s_i2cProcess_exit;
    }

    if (bRead)
    {
        status = i2cPerformTransaction(&ta);
        if (status != FLCN_OK)
        {
            goto s_i2cProcess_exit;
        }
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    {
        // DMA requires aligned size parameter.
        status = bRead ?
            ssurfaceWr(pDmemBuf, RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.pmgr.i2cPayload), alignedReqSize) :
            ssurfaceRd(pDmemBuf, RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.pmgr.i2cPayload), alignedReqSize);
    }
    lwosDmaLockRelease();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_i2cProcess_exit;
    }

    if (!bRead)
    {
        status = i2cPerformTransaction(&ta);
    }

s_i2cProcess_exit:
    return status;
}

/*!
 * @brief  Check for privilege port's write access.
 *
 * @param[in]  pTa  The transaction information. Field 'bRead' is used to
 *                  determine the read or write transaction.
 *
 * @return LW_TRUE  If access is allowed.
 *         LW_FALSE If access is not allowed.
 *
 * @note Caller must ensure that pTa->pMessage is filled/initialized.
 */
static LwBool
s_i2cAccessIsAllowed
(
    I2C_PMU_TRANSACTION *pTa
)
{
    LwBool bAllowed = LW_TRUE;

    /*!
     * Check for:
     * 1) Write access.
     * 2) Priv Sec is enabled for this port.
     * 3) And VBIOS scratch register disallows access in PMU
     */
    if ((!pTa->bRead) &&
        (((BIT(DRF_VAL(_RM_FLCN, _I2C_FLAGS, _PORT, pTa->pGenCmd->flags))) &
         (Pmgr.i2cPortsPrivMask)) != 0) &&
        (FLD_TEST_DRF(_VSB, _REG0, _I2C_WRITES, _DISALLOWED, PmuVSBCache)))
    {
        bAllowed = LW_FALSE;
    }

    return bAllowed;
}
