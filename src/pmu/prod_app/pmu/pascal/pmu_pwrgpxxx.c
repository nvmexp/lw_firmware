/*
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pwrgpxxx.c
 * @brief  PMU Power State (GCx) Hal Functions for GPXXX
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_lw_xve.h"
/* ------------------------- Application Includes --------------------------- */
#include "pmusw.h"
#include "pmu_perfmon.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "pmu_gc6.h"
#include "pmu_disp.h"
#include "pmu_objdisp.h"
#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwU16 s_pmuCallwtilPct_GPXXX(LwU32 bytes, FLCN_TIMESTAMP *pTimestamp)
    GCC_ATTRIB_SECTION("imem_libEngUtil", "pmuCallwtilPct_GPXXX");
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Generate host idle percentage on Tx direction based on
 *        LW_XVE_PCIE_UTIL_TX_BYTES register
 *
 * @return LwU16  Utilization pct with .01% accuracy.
 */
LwU16
pmuPEXCntSampleTxNoReset_GPXXX(FLCN_TIMESTAMP *pTimestamp, LwU32 *pPrevTxCount)
{
    LwU32 bytes;
    LwU32 count;

    if ((pTimestamp == NULL) ||
        (pPrevTxCount == NULL))
    {
        PMU_BREAKPOINT();
        return 0;
    }

    count = pmuPEXCntGetTx_HAL(&Pmu);
    bytes = PMU_GET_EFF_COUNTER_VALUE(count, *pPrevTxCount);

    // store current reg value
    *pPrevTxCount = count;

    return s_pmuCallwtilPct_GPXXX(bytes, pTimestamp);
}


/*!
 * @brief Generate host idle percentage on Rx direction based on
 *        LW_XVE_PCIE_UTIL_RX_BYTES registers
 *
 * @return LwU16  Utilization pct with .01% accuracy.
 */
LwU16
pmuPEXCntSampleRxNoReset_GPXXX(FLCN_TIMESTAMP *pTimestamp, LwU32 *pPrevRxCount)
{
    LwU32 count;
    LwU32 bytes;

    if ((pTimestamp == NULL) ||
        (pPrevRxCount == NULL))
    {
        PMU_BREAKPOINT();
        return 0;
    }

    count = pmuPEXCntGetRx_HAL(&Pmu);
    bytes = PMU_GET_EFF_COUNTER_VALUE(count, *pPrevRxCount);

    // store current reg value
    *pPrevRxCount = count;

    return s_pmuCallwtilPct_GPXXX(bytes, pTimestamp);
}

/*!
 * @brief If PCIE counters are enabled, sample current PCIE Rx
 *        idle counter register value.
 *
 * @return LwU16  Current PCIE Rx idle counter value
 */
LwU32
pmuPEXCntGetRx_GPXXX(void)
{
    LwU32 countCtrl;
    LwU32 regVal;
    LwU32 bytes;

    countCtrl = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL);
    if (DRF_VAL(_XVE_PCIE, _UTIL_CTRL, _ENABLE, countCtrl) != 1)
    {
        // At this point these counters should have been enabled
        PMU_HALT();
        return 0;
    }

    // Retrieve the number of bytes transferred in Rx direction
    regVal = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_RX_BYTES);
    bytes  = DRF_VAL(_XVE_PCIE, _UTIL_RX_BYTES, _COUNT, regVal);

    return bytes;
}

/*!
 * @brief If PCIE counters are enabled, sample current PCIE Tx
 *        idle counter register value.
 *
 * @return LwU16  Current PCIE Tx idle counter value
 */
LwU32
pmuPEXCntGetTx_GPXXX(void)
{
    LwU32 countCtrl;
    LwU32 regVal;
    LwU32 bytes;

    countCtrl = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL);
    if (DRF_VAL(_XVE_PCIE, _UTIL_CTRL, _ENABLE, countCtrl) != 1)
    {
        // At this point these counters should have been enabled
        PMU_HALT();
        return 0;
    }

    // Retrieve the number of bytes transferred in the Tx direction
    regVal = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_TX_BYTES);
    bytes  = DRF_VAL(_XVE_PCIE, _UTIL_TX_BYTES, _COUNT, regVal);

    return bytes;
}

/*!
 * @brief If PCIE counters are enabled, sample current PCIE kB
 *        counter register value and difference.
 */
FLCN_STATUS
pmuPexCntGetDiff_GPXXX
(
    LwBool  bRx,
    LwU32  *pDiff,
    LwU32  *pLast
)
{
    LwU32 countCtrl;
    LwU32 regVal;
    LwU32 bytes = *pLast;

    countCtrl = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL);
    if (DRF_VAL(_XVE_PCIE, _UTIL_CTRL, _ENABLE, countCtrl) != 1)
    {
        // At this point these counters should have been enabled
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    if (bRx)
    {
        regVal = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_RX_BYTES);
    }
    else
    {
        regVal = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_TX_BYTES);
    }

    // RX and TX have the same counter format.
    *pLast = DRF_VAL(_XVE_PCIE, _UTIL_RX_BYTES, _COUNT, regVal);

    if (pDiff != NULL)
    {
        // Bit-wise-and with mask to handle overflow (not a full 32 bits value).
        *pDiff = (*pLast - bytes) & DRF_MASK(LW_XVE_PCIE_UTIL_RX_BYTES_COUNT);
    }

    return FLCN_OK;
}

FLCN_STATUS
pmuPreInitGc6AllocBsodContext_GPXXX(void)
{
    FLCN_STATUS status;
    LwU32 ctxAlignedSize;
    void *pCtxBuf;

    status = FLCN_OK;
    if (!DMA_ADDR_IS_ZERO(&PmuInitArgs.gc6BsodCtx))
    {
        // Dynamically allocate an aligned DMA buffer.
        ctxAlignedSize = LW_ALIGN_UP(sizeof(RM_PMU_GC6_BSOD_CTX), DMA_MIN_READ_ALIGNMENT);
        pCtxBuf = lwosCallocResident(ctxAlignedSize + DMA_MIN_READ_ALIGNMENT);
        if (pCtxBuf == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto pmuPreInitGc6AllocBsodContext_GPXXX_exit;
        }
        PmuGc6.pBsodCtx = PMU_ALIGN_UP_PTR(pCtxBuf, DMA_MIN_READ_ALIGNMENT);

        // DMA context into DMEM.
        status = dmaRead(PmuGc6.pBsodCtx, &PmuInitArgs.gc6BsodCtx, 0, ctxAlignedSize);
        if (status != FLCN_OK)
        {
            goto pmuPreInitGc6AllocBsodContext_GPXXX_exit;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
        {
            // Validate head.headId right after dmaRead
            if ((PmuGc6.pBsodCtx)->pmsBsodCtx.head.headId >=
                dispGetNumberOfHeads_HAL(&Disp))
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto pmuPreInitGc6AllocBsodContext_GPXXX_exit;
            }

            // Validate head.orIndex right after dmaRead
            if ((PmuGc6.pBsodCtx)->pmsBsodCtx.head.orIndex >=
                dispGetNumberOfSors_HAL(&Disp))
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto pmuPreInitGc6AllocBsodContext_GPXXX_exit;
            }

            // Validate numPadlinks right after dmaRead
            if ((PmuGc6.pBsodCtx)->pmsBsodCtx.numPadlinks >
                RM_PMU_DISP_NUMBER_OF_PADLINKS_MAX)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto pmuPreInitGc6AllocBsodContext_GPXXX_exit;
            }
        }
    }

pmuPreInitGc6AllocBsodContext_GPXXX_exit:
    return status;
}

/*!
* @brief Initialization for GC6 exit for GMXXX chips.
*
* Any general initialization code not specific to particular engines should be
* done here. This function must be called prior to starting the scheduler.
*/
void
pmuInitGc6Exit_GPXXX(void)
{
    pmuInitGc6Exit_GMXXX();
    if ((PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_MODE_SET) ||
        PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_GC6_EXIT)) &&
        (PmuGc6.pBsodCtx != NULL))
    {
        Disp.pPmsBsodCtx = &PmuGc6.pBsodCtx->pmsBsodCtx;
    }
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief      Callwlate utilization % based on the bytes transferred
 * @param[in]  bytes  Bytes transferred
 *
 * @return     LwU16  Utilization pct with .01% accuracy.
 */
static LwU16
s_pmuCallwtilPct_GPXXX(LwU32 bytes, FLCN_TIMESTAMP *pTimestamp)
{
    LwU32 regVal;
    LwU32 baseBytes;
    LwU32 availableTimeus;
    LwU32 mulVal;
    LwU32 divVal;

    if (pTimestamp == NULL)
    {
        return 0;
    }

    //
    // Callwlate percentages (in units of 0.01% to minimize the effects of
    // truncation in integer math):
    //
    //   Percentage =  Bytes Transferred (KB) /
    //                      (SOL Bandwidth (KB/microsec) * Time Available * 95%)  * 10000
    //   SOL Bandwith = Link Speed * Link Lane Num * (1 byte/ 8 lanes) * encoding overhead
    //   Encoding Overhead = 8b/10b( GEN1 & GEN2) 128b/130b (GEN3)
    //   Available Time = PMU Timer Current Time - Previous Stored Time
    //   95% is the 5% derating for dllp overhead
    //

    //
    // Here 'availableTimeus' should be less than 2 sec because we are supporting max 0.5Hz.
    // Max return value from osPTimerTimeNsElapsedGet 4.29 sec.
    //
    availableTimeus = LW_UNSIGNED_ROUNDED_DIV(
                osPTimerTimeNsElapsedGet(pTimestamp), 1000);

    if (bytes == 0)
    {
        // This means PCIE bus is completely idle.
        return 0;
    }

    regVal = REG_RD32(BAR0_CFG, LW_XVE_LINK_CONTROL_STATUS);
    if (FLD_TEST_DRF(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, _2P5, regVal))
    {
        //
        // BaseBytes = availableTimeus * 2.5 (kb/microsec) *
        //              lane num * (1 byte / 8 lanes) *
        //              8/10(encode rate)
        // 95% is because the 5% derating for dllp overhead
        // Common denominator 100, for the last step.
        // Assuming 100 ms available time and 1 lane, max truncation rate is
        // 50 / (100,000 * 19 / 80) = 0.211%
        //
        mulVal = 19;
        divVal = 8000; // 20 * 4 * 100
    }
    else if (FLD_TEST_DRF(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, _5P0, regVal))
    {
        //
        // BaseBytes = availableTimeus * 5 (kb/microsec) *
        //              lane num * (1 byte / 8 lanes) *
        //              8/10(encode rate)
        // 95% is because the 5% derating for dllp overhead
        // Common denominator 100, for the last step
        // Assuming 100 ms available time and 1 lane, max truncation rate is
        // 50 / (100,000 * 19 * 4000) = 0.105%
        //
        mulVal = 19;
        divVal = 4000; // 20 * 2 * 100
    }
    else
    {
        //
        // BaseBytes = availableTimeus * 8 (kb/microsec) *
        //              lane num * (1 byte / 8 lanes) *
        //              128/130(encode rate)
        // 95% is because the 5% derating for dllp overhead
        // Common denominator 100, for the last step
        // Common denominator 32 to avoid overflow callwlating baseBytes
        // Assuming 100 ms available time and 1 lane, max truncation rate is
        // 50 / (100,000 * 76 * 100 / 8125) = 0.005%
        //
        mulVal = 76;   // 128 * 19 / 32
        divVal = 8125; // 130 * 20 * 100 / 32
    }

    //
    // Max availableTimeus is 4.29 sec, i.e. 0x418937 msec
    // and (lane num * mulVal)/divVal <= lane num
    // i.e. the max baseBytes here will be 0x4189370
    //
    baseBytes = LW_UNSIGNED_ROUNDED_DIV(
                availableTimeus *  DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _NEGOTIATED_LINK_WIDTH, regVal) * mulVal,
                divVal);
    //
    // Overriding bytes by busy percentage here.
    // Max bytes = 8 GB/s * 1 sec * lane num(16) * (1 bytes / 8 lane) = 0xF42400(KB)
    // Hence bytes * 10000 will overflow. So we are applying common denominator 100.
    //
    return (LwU16)LW_UNSIGNED_ROUNDED_DIV(bytes * 100, baseBytes);
}
