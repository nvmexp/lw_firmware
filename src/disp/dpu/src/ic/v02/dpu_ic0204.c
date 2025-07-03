/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_ic0204.c
 * @brief  DPU 02.04 Hal Functions
 *
 * DPU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#if (DPU_PROFILE_v0201)
/*!
 * This file is also compiled into v0201 ucode so that we don't need to create
 * v0204 ucode just for a few small functions. But the register accessed in this
 * file only exists after v0202, thus we need to statically include the v0202
 * header. In cases other than v0201, the included header will be dynamically
 * decided at compile time.
 */
#include "dpu/v02_02/dev_disp_falcon.h"
#define DEV_DISP_FALCON_INCLUDED
#endif
#include "dev_disp.h"
#include "dispflcn_regmacros.h"

/* ------------------------- Application Includes --------------------------- */
#include "class/cl947d.h"
#include "class/cl927c.h"
#include "dpu_objdpu.h"
#include "dpu_objic.h"
#include "dpu_gpuarch.h"
#include "lwosdebug.h"
#include "dpu_scanoutlogging.h"

#include "config/g_ic_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Writes ICD command and IBRKPT registers when interrupt handler gets called.
 *
 * This routine writes the ICD command and IBRKPT registers when the interrupt handler gets
 * called in response to the SW interrrupt generated during breakpoint hit.
 */
void
icServiceSwIntr_v02_04(void)
{
    LwU32 data = 0;
    LwU32 ptimer0Start = 0;

    //
    // This function is also compiled into v0201 ucode so that we don't need to
    // create v0204 ucode just for a few small functions. But we actually only
    // want to run this function in chips greater than v0204 (i.e. GM10X+), so
    // we need this IP_VER check to make sure it's not run in chips under v0204.
    //
#if (DPU_PROFILE_v0201)
    if (!DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_04))
    {
        return;
    }
#endif

    // Program EMASK of ICD
    data = DFLCN_FLD_SET_DRF(ICD_CMD, _OPC, _EMASK, data);
    data = DFLCN_FLD_SET_DRF(ICD_CMD, _EMASK_EXC_IBREAK, _TRUE, data);
    DFLCN_REG_WR32(ICD_CMD, data);

    // Wait for ICD done 1000 Ns
    ptimer0Start = DFLCN_REG_RD32(PTIMER0);
    while ((DFLCN_REG_RD32(PTIMER0) - ptimer0Start) <
            FLCN_DEBUG_DEFAULT_TIMEOUT)
    {
        // NOP
    }

    // Read back ICD and check for CMD ERROR
    data = DFLCN_REG_RD32(ICD_CMD);

    // regVal & 0x4000 != 0
    if (DFLCN_FLD_TEST_DRF(ICD_CMD, _ERROR, _TRUE, data))
    {
        lwuc_halt();
    }

    // The first breakpoint register
    data = DFLCN_REG_RD32(IBRKPT1);
    data = DFLCN_FLD_SET_DRF(IBRKPT1, _SUPPRESS, _ENABLE, data);
    DFLCN_REG_WR32(IBRKPT1, data);

    // The second breakpoint register
    data = DFLCN_REG_RD32(IBRKPT2);
    data = DFLCN_FLD_SET_DRF(IBRKPT2, _SUPPRESS, _ENABLE, data);
    DFLCN_REG_WR32(IBRKPT2, data);

    // Write SW GEN0 interrupt
    DFLCN_REG_WR32(IRQSSET,
            DFLCN_DRF_SHIFTMASK(IRQSSET_SWGEN0));

    // Write debug info register
    DFLCN_REG_WR32(DEBUGINFO, FLCN_DEBUG_DEFAULT_VALUE);
}

/*!
 * Service the HEAD related intrs
 */
void
icServiceHeadIntr_v02_04(LwU8 headNum)
{
    LwU32 headIntr = REG_RD32(CSB, LW_PDISP_DSI_DPU_INTR_HEAD(headNum));

    // In scanout logging, we only care the intrs in following cases
    if (DPUCFG_FEATURE_ENABLED(DPUTASK_SCANOUTLOGGING) &&
#if (DPU_PROFILE_v0201)
        // scanout logging is only needed after GM10X (i.e. v0204)
        DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_04) &&
#endif
        (headNum == scanoutLoggingContext.head) &&
        ((FLD_TEST_DRF(_RM_DPU_SCANOUTLOGGING, _FLAGS, _INTR, _RGLINE, scanoutLoggingContext.scanoutFlag) &&
          FLD_TEST_DRF(_PDISP, _DSI_DPU_INTR_HEAD, _RM_RG_LINE, _PENDING, headIntr)) ||
         (!FLD_TEST_DRF(_RM_DPU_SCANOUTLOGGING, _FLAGS, _INTR, _RGLINE, scanoutLoggingContext.scanoutFlag) &&
          FLD_TEST_DRF(_PDISP, _DSI_DPU_INTR_HEAD, _RM_DMI_LINE, _PENDING, headIntr))))
    {
        icServiceScanoutLineIntr_HAL(&IcHal);
    }

#if (DPU_PROFILE_v0207)
    if (DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_08) &&
        FLD_TEST_DRF(_PDISP, _DSI_DPU_INTR_HEAD, _PMU_RG_LINE, _PENDING, headIntr))
    {
        icServicePmuRgLineIntr_HAL(&IcHal, headNum);
    }
#endif

    // clear intr
    REG_WR32(CSB, LW_PDISP_DSI_DPU_INTR_HEAD(headNum), headIntr);
}

/* ------------------------ Static Functions ------------------------------- */

/*!
 * Service the DMI/RG line intrs specific to head passed from RM
 *
 * Once a DMI/RG line interrupt is received by DPU, it will notify this as a event
 * to scanoutlogging task to log timestamp and base ctxdma address and strip0 CRC
 * of frame based on CRC flag passed from RM
 */
void
icServiceScanoutLineIntr_v02_04(void)
{
    LwU32                    intrHead;
    SCANOUTLOGGING          *pScanoutActiveBuff;
    DISPATCH_SCANOUTLOGGING  disp2ScanoutLogging;
    FLCN_TIMESTAMP           time;
    LwU32                    lwDpsData       = 0;
    LwU32                    stallLock;
    LwBool                   bOSMActive      = LW_FALSE;
    LwU32                    channel         = LW_PDISP_907C_CHN_BASE(scanoutLoggingContext.head);
    LwU32                    pendingIntr     = 0;
    LwUPtr                   dsiArmBaseAddr  = LW_UDISP_DSI_CHN_ARMED_BASEADR(channel);

    intrHead = REG_RD32(CSB, LW_PDISP_DSI_DPU_INTR_HEAD(scanoutLoggingContext.head));
    if (FLD_TEST_DRF(_RM_DPU_SCANOUTLOGGING, _FLAGS, _INTR, _RGLINE, scanoutLoggingContext.scanoutFlag))
    {
        // Check for RG_LINE interrupt
        pendingIntr = (intrHead & DRF_SHIFTMASK(LW_PDISP_DSI_DPU_INTR_HEAD_RM_RG_LINE));
    }
    else
    {
        // Check for DMI_LINE interrupt
        pendingIntr = (intrHead & DRF_SHIFTMASK(LW_PDISP_DSI_DPU_INTR_HEAD_RM_DMI_LINE));
    }

    if (pendingIntr != 0)
    {
        scanoutLoggingContext.intrCount++;
        if (scanoutLoggingContext.intrCount > scanoutLoggingContext.rmBufTotalRecordCnt)
        {
            // disable logging when sysmem buffer limit is reached
            scanoutLoggingContext.bScanoutLoginEnable = LW_FALSE;
        }

        // check for logging flag is enabled or not
        if (scanoutLoggingContext.bScanoutLoginEnable)
        {
            if(scanoutLoggingContext.bDmaMemoryFlag)
            {
                // front buffer
                pScanoutActiveBuff = scanoutLoggingContext.dmemBufAddrFront;
            }
            else
            {
                // back buffer
                pScanoutActiveBuff = scanoutLoggingContext.dmemBufAddrBack;
            }
            // time stamp based on DPU ptimer
            osPTimerTimeNsLwrrentGet(&time);
            if (scanoutLoggingContext.timerOffsetLo >= 0)
            {
                time.parts.lo += scanoutLoggingContext.timerOffsetLo;
            }
            else
            {
                time.parts.lo -= scanoutLoggingContext.timerOffsetLo;
            }

            if (scanoutLoggingContext.timerOffsetHi >= 0)
            {
                time.parts.hi += scanoutLoggingContext.timerOffsetHi;
            }
            else
            {
                time.parts.hi -= scanoutLoggingContext.timerOffsetHi;
            }
            pScanoutActiveBuff[scanoutLoggingContext.dmemBufRecordIdx].timeStamp = time.data;
            // base ctxdma flip pointer
            pScanoutActiveBuff[scanoutLoggingContext.dmemBufRecordIdx].ctxDmaAddr0 =
                REG_RD32(CSB, dsiArmBaseAddr + LW927C_SET_CONTEXT_DMAS_ISO(0));

            lwDpsData = REG_RD32(CSB, LW_PDISP_COMP_LWDPS_CONTROL(scanoutLoggingContext.head));

            if (FLD_TEST_DRF(_RM_DPU_SCANOUTLOGGING, _FLAGS, _LOG, _CRC,
                                scanoutLoggingContext.scanoutFlag))
            {
                // If lwdps is enable then capture CRC of frame
                pScanoutActiveBuff[scanoutLoggingContext.dmemBufRecordIdx++].scanoutData =
                        REG_RD32(CSB, LW_PDISP_COMP_DEBUG_LWDPS_STRIP_CRC_DATA(scanoutLoggingContext.head));
            }
            else if (FLD_TEST_DRF(_RM_DPU_SCANOUTLOGGING, _FLAGS, _LOG, _CTXDMA1,
                                    scanoutLoggingContext.scanoutFlag))
            {
                // base ctxdma stereo flip pointer
                pScanoutActiveBuff[scanoutLoggingContext.dmemBufRecordIdx++].scanoutData =
                    REG_RD32(CSB, dsiArmBaseAddr + LW927C_SET_CONTEXT_DMAS_ISO(1));
            }
            else if (FLD_TEST_DRF(_RM_DPU_SCANOUTLOGGING, _FLAGS, _LOG, _VRR_STATE,
                                        scanoutLoggingContext.scanoutFlag))
            {
                // Get STALL_LOCK to check OSM is active or suspended
                stallLock = REG_RD32(CSB, LW_UDISP_DSI_CHN_ARMED_BASEADR(LW_PDISP_907D_CHN_CORE) +
                                           LW947D_HEAD_SET_STALL_LOCK(scanoutLoggingContext.head));

                if (FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _ENABLE, _TRUE,     stallLock) &&
                    FLD_TEST_DRF(947D, _HEAD_SET_STALL_LOCK, _MODE,   _ONE_SHOT, stallLock))
                {
                    bOSMActive = LW_TRUE;
                }
                pScanoutActiveBuff[scanoutLoggingContext.dmemBufRecordIdx++].scanoutData =
                                                                                bOSMActive;
            }

            if (scanoutLoggingContext.dmemBufRecordIdx == SCANOUT_LOGGING_MAX_DMA_RECORD_COUNT)
            {
                disp2ScanoutLogging.scanoutEvt.eventType = DISP2UNIT_EVT_SIGNAL;
                // active dmem buffer
                disp2ScanoutLogging.scanoutEvt.pDmemBufAddr = pScanoutActiveBuff;
                // task is queued
                lwrtosQueueSendFromISR(ScanoutLoggingQueue, &disp2ScanoutLogging,
                                       sizeof(disp2ScanoutLogging));
                // After task queuing idx should be reset
                scanoutLoggingContext.dmemBufRecordIdx = 0;
                // toggle dmaflag
                scanoutLoggingContext.bDmaMemoryFlag = (scanoutLoggingContext.bDmaMemoryFlag ? LW_FALSE : LW_TRUE);
            }
            else if(scanoutLoggingContext.dmemBufRecordIdx > SCANOUT_LOGGING_MAX_DMA_RECORD_COUNT)
            {
                // To avoid invalid write
                DPU_BREAKPOINT();
            }
        }

        // reset the handled interrupt
        REG_WR32(CSB, LW_PDISP_DSI_DPU_INTR_HEAD(scanoutLoggingContext.head),
                     pendingIntr);
    }
}

