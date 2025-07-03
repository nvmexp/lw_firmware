/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_pr.c
 * @brief  Task to handle PlayReady 
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objpr.h"
#include "sec2_chnmgmt.h"
#include "pr/pr_mthds.h"
#include "pr/sec2_pr.h"
#include "drmteestub.h"
#include "oemcommonlw.h"
#include "oemteecryptointernaltypes.h"

#ifndef PR_V0404
#include "playready/3.0/lw_playready.h"
#else
#include "playready/4.4/lw_playready.h"
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be PLAYREADY commands.
 */
LwrtosQueueHandle PrQueue;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Processes the methods received via the channel.
 *
 * Iterates over all the methods received from the channel. For each method,
 * the function reads in the parameter for the method, calls the actual handler
 * to process the method, the writes the parameter back to the channel.
 *
 * @return
 *      FLCN_OK if the method is processed succesfully; otherwise, a detailed
 *      error code is returned.
 */
FLCN_STATUS
_prProcessMethod(void)
{
    FLCN_STATUS       status       = FLCN_ERROR;
    LwU32             mthd;
    LwU32             i;
    LwU32             funcID       = 0;
    LwU64             reqBufBase   = 0;
    LwU64             resBufBase   = 0;
    LwU32             reqBufSize   = 0;
    LwU32             alignedReqSz = 0;
    LwU32             alignedResSz = 0;
    LwU32             resBufSize   = 0;
    LwU8             *pReqMsg      = NULL;
    LwU8             *pResMsg      = NULL;
    LwU8             *pReqMsgOrig  = NULL;
    LwU8             *pResMsgOrig  = NULL;
    RM_FLCN_MEM_DESC  memDesc;

    //
    // LWE (nkuo) - Init the shared struct which stores the FB offsets of all
    // global variables that will be shared with LWDEC
    //
    if (!prIsSharedStructInitialized_HAL(&PrHal))
    {
        if (DRM_SUCCESS != OEM_TEE_InitSharedDataStruct())
        {
            status = FLCN_ERR_PR_SHARED_STRUCT_INIT_FAILED;
            goto label_return;
        }
    }

    // Read all the relevant Playready methods
    FOR_EACH_VALID_METHOD(i, appMthdValidMask)
    {
        mthd = PR_GET_METHOD_ID(i);

        if (mthd == PR_METHOD_ID(FUNCTION_ID))
        {
            funcID = PR_GET_METHOD_PARAM_OFFSET_FROM_ID(FUNCTION_ID);
        }
        else if (mthd == PR_METHOD_ID(REQUEST_MESSAGE))
        {
            reqBufBase = (LwU64)PR_GET_METHOD_PARAM_OFFSET_FROM_ID(REQUEST_MESSAGE);
        }
        else if (mthd == PR_METHOD_ID(REQUEST_MESSAGE_SIZE))
        {
            reqBufSize = PR_GET_METHOD_PARAM_OFFSET_FROM_ID(REQUEST_MESSAGE_SIZE);
        }
        else if (mthd == PR_METHOD_ID(RESPONSE_MESSAGE))
        {
            resBufBase = (LwU64)PR_GET_METHOD_PARAM_OFFSET_FROM_ID(RESPONSE_MESSAGE);
        }
        else if (mthd == PR_METHOD_ID(RESPONSE_MESSAGE_SIZE))
        {
            resBufSize = PR_GET_METHOD_PARAM_OFFSET_FROM_ID(RESPONSE_MESSAGE_SIZE);
            //
            // HMR status will be embedded in the response buffer, so we need to
            // extra allocate more space for it
            //
            resBufSize += LW_PR_HMR_STATUS_SIZE;
        }
    }

    if ((reqBufBase == 0) || (reqBufSize == 0) || (resBufBase == 0) || (resBufSize == 0))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }
    else
    {
        // LWE (nkuo) - These two buffers will be used for DMA transfer, so the address has to be 256-byte aligned
        pReqMsg = pvPortMallocFreeable(reqBufSize + 256);
        pResMsg = pvPortMallocFreeable(resBufSize + 256);
        pReqMsgOrig = pReqMsg;
        pResMsgOrig = pResMsg;

        if ((pReqMsg == NULL) || (pResMsg == NULL))
        {
            status = FLCN_ERR_NO_FREE_MEM;
            goto label_return;
        }

        memset((void *)pReqMsg , 0, (reqBufSize + 256));
        memset((void *)pResMsg , 0, (resBufSize + 256));

        // LWE (nkuo) - Cache the original address before aligning up the address so that we know where to free the buffer
        if (!VAL_IS_ALIGNED((LwU32)pReqMsg, 256))
        {
            pReqMsg = (LwU8*)LW_ALIGN_UP((LwU32)pReqMsg, 256);
        }

        if (!VAL_IS_ALIGNED((LwU32)pResMsg, 256))
        {
            pResMsg = (LwU8*)LW_ALIGN_UP((LwU32)pResMsg, 256);
        }

        alignedReqSz = LW_ALIGN_UP(reqBufSize, DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_READ));
        //
        // Read the input request message
        // Address is by 256 byte aligned by default.
        //
        reqBufBase = reqBufBase << 8;
        RM_FLCN_U64_PACK(&(memDesc.address), &reqBufBase);
        memDesc.params = 0;
        memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_VIRT, memDesc.params);
        memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, alignedReqSz,
                                     memDesc.params);

        if ((status = dmaRead(pReqMsg, &memDesc, 0, alignedReqSz)) != FLCN_OK)
        {
            goto label_return;
        }

        // HMR status will be embedded in the first two DWORDs of the response buffer
        *((LwU32*)(pResMsg + LW_PR_HMR_STATUS_ID_OFFSET))   = LW_PR_HMR_STATUS_ID_VALUE;
        *((LwU32*)(pResMsg + LW_PR_HMR_STATUS_CODE_OFFSET)) = DRM_TEE_STUB_HandleMethodRequest(reqBufSize, pReqMsg, &resBufSize, (pResMsg + LW_PR_HMR_STATUS_SIZE));

        alignedResSz  = LW_ALIGN_UP(resBufSize, DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_WRITE));

        //
        // HMR won't know about the two DWORDs we used to pass the HMR status, thus
        // those two DOWRDs were not considered by HMR, so need to add that two DWORDs
        // to alignedResSz to ensure we DMA correct size of data back to UMD.
        //
        alignedResSz += LW_PR_HMR_STATUS_SIZE;
        //
        // Write back the output response message
        // Address is by 256 byte aligned by default.
        //
        resBufBase = resBufBase << 8;
        RM_FLCN_U64_PACK(&(memDesc.address), &resBufBase);
        memDesc.params = 0;
        memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                     RM_SEC2_DMAIDX_VIRT, memDesc.params);
        memDesc.params = FLD_SET_DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, alignedResSz,
                                     memDesc.params);

        if ((status = dmaWrite(pResMsg, &memDesc, 0, alignedResSz)) != FLCN_OK)
        {
            goto label_return;
        }
    }

label_return:
    if (pReqMsgOrig != NULL)
    {
        vPortFreeFreeable(pReqMsgOrig);
    }
    if (pResMsgOrig != NULL)
    {
        vPortFreeFreeable(pResMsgOrig);
    }

    if (status == FLCN_ERR_DMA_NACK)
    {
        NOTIFY_EXELWTE_NACK();
    }
    else
    {
        NOTIFY_EXELWTE_COMPLETE();
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task_pr, pvParameters)
{
    DISPATCH_PR dispatch;

    while (LW_TRUE)
    {
        if (OS_QUEUE_WAIT_FOREVER(PrQueue, &dispatch))
        {
            //
            // Process the method(s) sent via the host/channel. This is
            // lwrrently the only way the PLAYREADY task receives commands.
            //
            (void)_prProcessMethod();
        }
    }
}
