/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DPU_SCANOUTLOGGING_H
#define DPU_SCANOUTLOGGING_H

#include "dpu/dpuifscanoutlogging.h"
#include "unit_dispatch.h"

#define SCANOUT_LOGGING_MAX_DMA_RECORD_COUNT 16

/*! 
 * @brief scanout event stucture used for DMI line interrupts.
 */
typedef struct
{
    /*!
     * Singal type to scanoutlogging task from isr
     */
    LwU8            eventType;
    /*!
     * active dmem buffer
     */
    SCANOUTLOGGING  *pDmemBufAddr;
} SCANOUT_EVT;

/*!
 * @brief Structure used to pass SCANOUTLOGGING related events
 */
typedef union
{
    LwU8          eventType;
    DISP2UNIT_CMD command;
    SCANOUT_EVT   scanoutEvt;
} DISPATCH_SCANOUTLOGGING;


/*!
 * @brief Context information for the scanoutlogging task.
 */
typedef struct
{
    /*!
     * DMA address descriptor
     */
    RM_FLCN_MEM_DESC  dmaDesc;
    /*!
     * Number of record of type SCANOUTLOGGING to be logged by DPU
     */
    LwU32           rmBufTotalRecordCnt;
    /*!
     * interrupt counter to disable logging
     */
    LwU32           intrCount;
    /*!
     * front buffer
     */
    SCANOUTLOGGING  dmemBufAddrFront[SCANOUT_LOGGING_MAX_DMA_RECORD_COUNT];
    /*!
     * back buffer
     */
    SCANOUTLOGGING  dmemBufAddrBack[SCANOUT_LOGGING_MAX_DMA_RECORD_COUNT];
    /*!
     * index to track dmem buffer entry
     */
    LwU32           dmemBufRecordIdx;
    /*!
     * index to track RM buffer entry
     */
    LwU32           rmBufRecordIdx;
    /*!
     * active head on which logging should be done
     */
    LwU32           head;
    /*!
     * offset to be adjusted from DPU clock(low)
     */
    LwS32           timerOffsetLo;
    /*!
     * offset to be adjusted from DPU clock(high)
     */
    LwS32           timerOffsetHi;
    /*!
     * flag to switch between front and back dmem buffer
     */
    LwBool          bDmaMemoryFlag;
    /*!
     * flag to disable logging when intrCount > rmBufTotalRecordCnt
     */
    LwBool          bScanoutLoginEnable;
    /*!
     * Bit 0 : 1 - log CRC
     *         0 - log ctxdma1
     * Bit 1 : 1 - process RG interrupt
     *         0 - process DMI interrupt
     */
    LwU8            scanoutFlag;
} SCANOUTLOGGING_CONTEXT;

extern SCANOUTLOGGING_CONTEXT  scanoutLoggingContext;


#endif // DPU_SCANOUTLOGGING_H
