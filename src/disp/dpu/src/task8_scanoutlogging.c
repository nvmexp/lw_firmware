/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task8_scanoutlogging.c
 * @brief  Task that is used to log data for scanout analysis
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "lwoslayer.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu/dpuifscanoutlogging.h"
#include "dpu_scanoutlogging.h"
#include "dpu_objdpu.h"
#include "lwos_dma.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwrtosQueueHandle       ScanoutLoggingQueue;
SCANOUTLOGGING_CONTEXT  scanoutLoggingContext;

/* ------------------------ Static Variables ------------------------------- */
static void _scanoutLoggingCmdDispatch(RM_DPU_COMMAND *pCmd, LwU8 cmdQueueId);
static void _flushScanoutData(SCANOUTLOGGING *pDmemBufAddr, LwU32 dmemRecordCount);

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task8_scanoutLogging, pvParameters)
{
    DISPATCH_SCANOUTLOGGING disp2ScanoutLogging;
    LwU32 dmemRecordCount = 0;
    for (;;)
    {
        if (OS_QUEUE_WAIT_FOREVER(ScanoutLoggingQueue, &disp2ScanoutLogging))
        {
            switch (disp2ScanoutLogging.eventType)
            {
                //command sent from RM to enable logging
                case DISP2UNIT_EVT_COMMAND:
                {
                    _scanoutLoggingCmdDispatch(disp2ScanoutLogging.command.pCmd,
                                       disp2ScanoutLogging.command.cmdQueueId);
                    break;
                }
                //command sent from DPU for DMA
                case DISP2UNIT_EVT_SIGNAL:
                {
                    dmemRecordCount = SCANOUT_LOGGING_MAX_DMA_RECORD_COUNT;
                    _flushScanoutData(disp2ScanoutLogging.scanoutEvt.pDmemBufAddr,
                                       dmemRecordCount);
                    break;
                }
            }
        }
    }
}

/* ------------------------- Static Functions ------------------------------ */
/*!
 * @brief Process a command.
 *
 * @param[in]  pCmd        Command from the RM.
.* @param[in]  cmdQueueId  Queue Id to acknowledge command is processed
 */
static void
_scanoutLoggingCmdDispatch(RM_DPU_COMMAND *pCmd, LwU8 cmdQueueId)
{
    SCANOUTLOGGING                   *pDmemBufAddr;
    FLCN_TIMESTAMP                    dpuTime;
    RM_FLCN_QUEUE_HDR                 hdr;
    RM_DPU_SCANOUTLOGGING_CMD_ENABLE *pScanoutLogCmd = &pCmd->cmd.scanoutLogging.cmdEnable;
    LwU8                              dmaIdx;
    switch (pCmd->cmd.scanoutLogging.cmdType)
    {
        case RM_DPU_CMD_TYPE(SCANOUTLOGGING, ENABLE):
            dmaIdx = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                            pScanoutLogCmd->dmaDesc.params);

            // Sanity check dmaDesc for dmaIdx
            if ((dmaIdx != RM_DPU_DMAIDX_PHYS_SYS_COH) &&
                (dmaIdx != RM_DPU_DMAIDX_PHYS_SYS_NCOH))
            {
                DPU_HALT();
            }

            scanoutLoggingContext.bDmaMemoryFlag = LW_TRUE;
            scanoutLoggingContext.scanoutFlag = pScanoutLogCmd->scanoutFlag;
            scanoutLoggingContext.dmaDesc = pScanoutLogCmd->dmaDesc;
            // Total size divided by structure size should be passed
            scanoutLoggingContext.rmBufTotalRecordCnt = pScanoutLogCmd->rmBufTotalRecordCnt;
            scanoutLoggingContext.head = pScanoutLogCmd->head;
            scanoutLoggingContext.rmBufRecordIdx = 0;
            scanoutLoggingContext.intrCount = 0;
            scanoutLoggingContext.timerOffsetLo = pScanoutLogCmd->timerOffsetLo;
            scanoutLoggingContext.timerOffsetHi = pScanoutLogCmd->timerOffsetHi;

            // time stamp based on DPU ptimer
            osPTimerTimeNsLwrrentGet(&dpuTime);
            if (scanoutLoggingContext.timerOffsetLo >= 0)
            {
                dpuTime.parts.lo += scanoutLoggingContext.timerOffsetLo;
            }
            else
            {
                dpuTime.parts.lo -= scanoutLoggingContext.timerOffsetLo;
            }

            if (scanoutLoggingContext.timerOffsetHi >= 0)
            {
                dpuTime.parts.hi += scanoutLoggingContext.timerOffsetHi;
            }
            else
            {
                dpuTime.parts.hi -= scanoutLoggingContext.timerOffsetHi;
            }
            // Logged timestamp when Enable command is sent to DPU
            scanoutLoggingContext.dmemBufAddrFront[0].timeStamp = dpuTime.data;
            scanoutLoggingContext.dmemBufAddrFront[0].ctxDmaAddr0 = 0;
            scanoutLoggingContext.dmemBufAddrFront[0].scanoutData = 0;
            // Logged first entry so increasing dmemBufRecordIdx
            scanoutLoggingContext.dmemBufRecordIdx = 1;
            scanoutLoggingContext.bScanoutLoginEnable = LW_TRUE;
            break;

        case RM_DPU_CMD_TYPE(SCANOUTLOGGING, DISABLE):
            scanoutLoggingContext.bScanoutLoginEnable = LW_FALSE;
            // after disable copy logged data upto dmemBufRecordIdx
            if(scanoutLoggingContext.bDmaMemoryFlag)
            {
                // front buffer
                pDmemBufAddr = scanoutLoggingContext.dmemBufAddrFront;
            }
            else
            {
                // back buffer
                pDmemBufAddr = scanoutLoggingContext.dmemBufAddrBack;
            }
            _flushScanoutData(pDmemBufAddr, scanoutLoggingContext.dmemBufRecordIdx);
            break;

        default:
            break;
    }
    // As no msg is sent from DPU to RM, size should only size of HDR
    hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;
    hdr.unitId    = RM_DPU_UNIT_SCANOUTLOGGING;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = pCmd->hdr.seqNumId;
    osCmdmgmtRmQueuePostBlocking(&hdr, NULL);
    osCmdmgmtCmdQSweep(&pCmd->hdr, cmdQueueId);
}

/*!
 * @brief Process a signal.
 *
 * @param[in]  pDmemBufAddr         source address for DMA.
 * @param[in]  dmemRecordCount      number of entries logged in source address.
 */
static void
_flushScanoutData(SCANOUTLOGGING *pDmemBufAddr, LwU32 dmemRecordCount)
{
    LwU32      recordCount = 0;
    void      *pSrc = (void *)pDmemBufAddr;
    LwU32      numBytes;
    LwU32      offset = scanoutLoggingContext.rmBufRecordIdx * sizeof(SCANOUTLOGGING);

    if (scanoutLoggingContext.rmBufRecordIdx < scanoutLoggingContext.rmBufTotalRecordCnt)
    {
        if ((scanoutLoggingContext.rmBufRecordIdx + dmemRecordCount) < scanoutLoggingContext.rmBufTotalRecordCnt)
        {
            recordCount = dmemRecordCount;
        }
        if ((scanoutLoggingContext.rmBufRecordIdx + dmemRecordCount) >= scanoutLoggingContext.rmBufTotalRecordCnt)
        {
            recordCount  = scanoutLoggingContext.rmBufTotalRecordCnt - scanoutLoggingContext.rmBufRecordIdx;
            scanoutLoggingContext.bScanoutLoginEnable = LW_FALSE;
        }
        numBytes = recordCount * sizeof(SCANOUTLOGGING);

        if (FLCN_OK != dmaWrite(pSrc, &scanoutLoggingContext.dmaDesc, offset, numBytes))
        {
            DBG_PRINTF(("_flushScanoutData: dma failed, bytesWritten:%d\n",
                        bytesCompleted, 0, 0, 0));
        }
        scanoutLoggingContext.rmBufRecordIdx +=  recordCount;
    }
}

