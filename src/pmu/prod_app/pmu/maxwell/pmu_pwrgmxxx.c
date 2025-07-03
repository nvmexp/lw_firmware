/*
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pwrgmxxx.c
 * @brief  PMU Power State (GCx) Hal Functions for GMXXX
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pwr_csb.h"
#include "dev_bus.h"
#include "dev_therm.h"
#include "dev_lw_xve.h"
#include "pmu_gc6.h"
#include "pmu_disp.h"
#include "pmu_objdisp.h"
#include "dev_flush.h"
#include "dev_pwr_pri.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objbif.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
#ifndef LW_CPWR_THERM
#define LW_CPWR_THERM 0x17FF:0x1000
// sanitize the define
ct_assert(LW_CPWR_THERM_ARP_STATUS - DRF_BASE(LW_CPWR_THERM) ==
          LW_THERM_ARP_STATUS - DRF_BASE(LW_THERM));
#endif

// poll timeout 2 msec
#define GC6M_I2CS_STATE_SECONDARY_IDLE_TIMEOUT_USEC  2000

/* ------------------------- External Definitions --------------------------- */
extern FLCN_TIMESTAMP PerfmonPreTime;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

#define DMA_MIN_READ_ALIGN_MASK   (DMA_MIN_READ_ALIGNMENT - 1)
#define PCI_DATA_BUFFER_SIZE      256
#define PCI_REGMAP_BUFFER_SIZE    128
#define PCI_REGMAP_INDEX_SIZE     32
#define PCI_MAX_REGISTER_INDEX    (copySize / PCI_REGISTER_SIZE)
#define PCI_CONFIG_SPACE_SIZE     0x1000
#define HEAP_BUFFER_SIZE_NEEDED   (PCI_DATA_BUFFER_SIZE + PCI_REGMAP_BUFFER_SIZE)
#define PCI_REGISTER_SIZE         0x4
#define PCI_REGMAP_INDEX_WIDTH    0x4
#define PCI_REGMAP_SIZE           (regmapMaxIndex * PCI_REGMAP_INDEX_WIDTH)
#define PCI_DMA_READ_ALIGNMENT    DMA_MIN_READ_ALIGNMENT

/* ------------------------- Global Variables ------------------------------- */
PMU_GC6 PmuGc6;

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_pmuRestorePciConfigRegisters_GMXXX
                  (RM_PMU_PCI_CONFIG_ADDR *pPciCfgAddr)
    GCC_ATTRIB_SECTION("imem_init", "pmuRestorePciConfigRegisters_GMXXX");
static FLCN_STATUS s_pgSyncI2CSHandoff_GMXXX(void)
    GCC_ATTRIB_SECTION("imem_init", "pgSyncI2CSHandoff_GMXXX");
static LwU16 s_pmuCallwtilPct_GMXXX(LwU32 bytes)
    GCC_ATTRIB_SECTION("imem_libEngUtil", "pmuCallwtilPct_GMXXX");
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Pre-initialize GC6 exit before main PMU init.
 * Populate GC6 context data structure.
 *
 * @return  FLCN_OK                 On success
 * @return  FLCN_ERR_NO_FREE_MEM    If malloc fails
 * @return  Others                  Error codes returned by child functions
 */
FLCN_STATUS
pmuPreInitGc6Exit_GMXXX(void)
{
    LwU32       ctxAlignedSize;
    void       *pCtxBuf;
    FLCN_STATUS status;
    LwU32       timeDevinitStart;
    LwU32       timeDevinitFinish;
    LwU32       timeGc6UDEPTimerCount;
    LwU32       timeLwrrent;
    LwU32       regVal;
    LwU8        pwmVidPin;

    // Trigger SCI to update SCI PTIMER TIME_0 and TIME_1 registers
    regVal = REG_RD32(BAR0, LW_PGC6_SCI_PTIMER_READ);
    regVal = FLD_SET_DRF(_PGC6, _SCI_PTIMER_READ, _SAMPLE, _TRIGGER, regVal);
    REG_WR32(BAR0, LW_PGC6_SCI_PTIMER_READ, regVal);

    // Wait for TIME_0 and TIME_1 registers to be updated
    do
    {
        regVal = REG_RD32(BAR0, LW_PGC6_SCI_PTIMER_READ);
    } while (!FLD_TEST_DRF(_PGC6, _SCI_PTIMER_READ, _SAMPLE, _DONE, regVal));

    // Read TIME_0(Low 32 bits)
    timeLwrrent = REG_RD32(BAR0, LW_PGC6_SCI_PTIMER_TIME_0);

    //
    // Mailbox registers are used to store SCI PTIMER Values at the
    //              start and end of devinit
    //
    timeDevinitStart = REG_RD32(BAR0, LW_PPWR_PMU_MAILBOX
                                      (RM_GC6UDE_PMU_MAILBOX_ID_LEGACY_DEVINIT_START_TIME));
    timeDevinitFinish = REG_RD32(BAR0, LW_PPWR_PMU_MAILBOX
                                       (RM_GC6UDE_PMU_MAILBOX_ID_LEGACY_DEVINIT_FINISH_TIME));

    // Mailbox 1 is used in devinit profiling after core 80
    timeGc6UDEPTimerCount = REG_RD32(BAR0, LW_PPWR_FALCON_MAILBOX1);

    ctxAlignedSize = (sizeof(RM_PMU_GC6_CTX) + DMA_MIN_READ_ALIGN_MASK) &
                                              ~DMA_MIN_READ_ALIGN_MASK;
    pCtxBuf = lwosCallocResident(ctxAlignedSize + DMA_MIN_READ_ALIGN_MASK);
    if (pCtxBuf == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        PMU_BREAKPOINT();
        goto pmuPreInitGc6Exit_GMXXX_exit;
    }
    PmuGc6.pCtx = PMU_ALIGN_UP_PTR(pCtxBuf, DMA_MIN_READ_ALIGNMENT);

    // DMA context into DMEM.
    status = dmaRead(PmuGc6.pCtx, &PmuInitArgs.gc6Ctx, 0, ctxAlignedSize);
    if (status != FLCN_OK)
    {
        goto pmuPreInitGc6Exit_GMXXX_exit;
    }

    // Part of LWLINK unclamping done in phase1 of exit (Bug2197144)
    status = pgGc6LwlinkL2Exit_HAL(&Pg, PmuGc6.pCtx->lwlinkMask);
    if (status != FLCN_OK)
    {
        goto pmuPreInitGc6Exit_GMXXX_exit;
    }

    if (DISP_IS_PRESENT())
    {
        status = pmuPreInitGc6AllocBsodContext_HAL();
        if (status != FLCN_OK)
        {
            goto pmuPreInitGc6Exit_GMXXX_exit;
        }
    }

    if (!DMA_ADDR_IS_ZERO(&PmuGc6.pCtx->pciConfigAddr.pciAddr) &&
        !DMA_ADDR_IS_ZERO(&PmuGc6.pCtx->pciConfigAddr.regmapAddr) &&
        (PMUCFG_FEATURE_ENABLED(PMU_GC6_CONFIG_SPACE_RESTORE)))
    {
        status = s_pmuRestorePciConfigRegisters_GMXXX(&PmuGc6.pCtx->pciConfigAddr);
        if (status != FLCN_OK)
        {
            goto pmuPreInitGc6Exit_GMXXX_exit;
        }
    }

    if (PmuGc6.pCtx->bGC6StatsEnabled)
    {
        PmuGc6.pStats = lwosCallocResident(sizeof(RM_PMU_GC6_EXIT_STATS));
        if (PmuGc6.pStats == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto pmuPreInitGc6Exit_GMXXX_exit;
        }

        //
        // Devinit Time =   PTIMER_END_DEVINIT - PTIMER_START_DEVINIT
        // Bootstrap Time = TIME_START_GC6_PREINIT - PTIMER_END_DEVINIT
        // Using above formula to get respective exelwtion times
        //
        PmuGc6.pStats->ifrPmuBootstrap_xtal4x = timeLwrrent - timeDevinitFinish;
        PmuGc6.pStats->ifrPmuDevinit_xtal4x   = timeDevinitFinish - timeDevinitStart;
        PmuGc6.pStats->bsiUde_ptimer          = timeGc6UDEPTimerCount;

        // DMA DMEM copy of stats to FB.
        status = dmaWrite(PmuGc6.pStats, &PmuGc6.pCtx->exitStats, 0,
                          sizeof(RM_PMU_GC6_EXIT_STATS));
        if (status != FLCN_OK)
        {
            goto pmuPreInitGc6Exit_GMXXX_exit;
        }
    }

    if (PmuGc6.pCtx->bExitBootVoltageEnabled)
    {
        // Retrieve the saved pin
        pwmVidPin = (LwU8) REG_RD32(FECS, LW_PGC6_BSI_SCRATCH(1));

        // Set SCI PWM to FULL_DRIVE
        regVal = REG_RD32(FECS, LW_PGC6_SCI_GPIO_OUTPUT_CNTL(pwmVidPin));
        regVal = FLD_SET_DRF(_PGC6, _SCI_GPIO_OUTPUT_CNTL, _DRIVE_MODE,
                             _FULL_DRIVE, regVal);
        REG_WR32(FECS, LW_PGC6_SCI_GPIO_OUTPUT_CNTL(pwmVidPin), regVal);
    }

    if (PmuGc6.pCtx->bFakeI2csEnabled)
    {
        status = s_pgSyncI2CSHandoff_GMXXX();
        if (status != FLCN_OK)
        {
            goto pmuPreInitGc6Exit_GMXXX_exit;
        }
    }

    pgGc6BsiStateRestore_HAL(&Pmu);

    // Disable wake timer. This will disable the wake timer for generating
    // another GC6 exit request to SCI, since we are already in the exit path
    regVal = REG_RD32(FECS, LW_PGC6_SCI_WAKE_EN);
    regVal = FLD_SET_DRF(_PGC6, _SCI_WAKE_EN, _WAKE_TIMER, _DISABLE, regVal);
    REG_WR32(FECS, LW_PGC6_SCI_WAKE_EN, regVal);

    // Also disable SW reporting
    regVal = REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_EN);
    regVal = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _WAKE_TIMER, _DISABLE, regVal);
    REG_WR32(FECS, LW_PGC6_SCI_SW_INTR0_EN, regVal);

pmuPreInitGc6Exit_GMXXX_exit:
    return status;
}

/*!
 * @brief Reads the BSI ram from provided offset and
 * creates a save state for privs defined in the ram.
 *
 * @param[in]  ramOffset   Offset to the save state in ram
 * @param[in]  nPrivs      Num of privs (addr-data) pairs in the state
 */
void
pgCreateBsiThermState_GMXXX
(
    LwU32 ramOffset,
    LwU32 nPrivs
)
{
    LwU32 i, val, payload;

    if (!nPrivs)
    {
        return;
    }

    // Setup port to the offset provided through command
    val = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, val);
    val = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _ENABLE, val);

    // Set the starting address to read from
    val = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, ramOffset, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, val);

    //
    // For each priv;
    // 1. Read the CSB address of the priv from the ram offset.
    //    Increments addr by 4.
    // 2. Read the value on the CSB priv address
    // 3. Write the value gathered into ram offset
    //    Increments addr by 4 again.
    //
    for (i = 0; i < nPrivs; i++)
    {
        //
        // Step 1. Read the therm address from the ram offset
        // Read the therm address value
        // addr incremented by 4
        //
        payload = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

        if (payload >= DRF_BASE(LW_THERM) && payload <= DRF_EXTENT(LW_THERM))
        {
            // colwert to CSB
            payload -= DRF_BASE(LW_THERM);
            payload += DRF_BASE(LW_CPWR_THERM);
        }

        //
        // Step 2 and 3. Read the value on the CSB priv address
        // Write the value gathered into ram offset + 4
        // addr incremented by 4 again
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, REG_RD32(CSB, payload));
        // Debug:
        // REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, 0xcafebee0 + i);
    }
}

/*!
 * @brief Initialization for GC6 exit for GMXXX chips.
 *
 * Any general initialization code not specific to particular engines should be
 * done here. This function must be called prior to starting the scheduler.
 */
void
pmuInitGc6Exit_GMXXX(void)
{
    LwU32 val;
    FLCN_TIMESTAMP timeOut;

    val = REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_STATUS_LWRR);

    //
    // Check if selfwake is enabled and also if the GC6 exit is because of a
    // selfwake and only then send PMU interrupt to KMD to process the selfwake
    // GC6 exit.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_GC6_CONFIG_SPACE_RESTORE) &&
        PmuGc6.pCtx->bSelfWakeSupported &&
        FLD_TEST_DRF(_PGC6_SCI, _SW_INTR0_STATUS_LWRR, _GC6_EXIT, _PENDING, val) &&
        FLD_TEST_DRF(_PGC6_SCI, _SW_INTR0_STATUS_LWRR, _GPU_EVENT, _NOT_PENDING, val))
    {
        // Make sure that link is up
        while (LW_TRUE)
        {
            val = REG_RD32(FECS, LW_XP_PL_LTSSM_STATE);
            if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_LINK_READY, _TRUE, val))
            {
                break;
            }
        }

        //
        // The delay below is needed after we saw link enabled true.
        //
        OS_PTIMER_SPIN_WAIT_NS(4000);

        //
        // Set the following in register LW_XVE_SW_MSG_HDR_DWx:
        // 1. FORMAT = 0x2
        // 2. TYPE = 0x0
        // 3. MSG_TYPE = 0xf
        // 4. REQ_ID_DEV_NO = <whatever is set on the system>
        // 5. REQ_ID_BUS_NO = <whatever is set on the system>
        //
        // Let all other fields be set to default value(0) in LW_XVE_SW_MSG_HDR_DWx.
        //
        // Sequence to follow to send INTA assert message from XVE:
        // 1. Set LW_XVE_SW_MSG_CTRL_REQ_ID_OVERRIDE_ENABLE
        // 2. Set the LW_XVE_SW_MSG_HDR_DWx as mentioned above.
        // 3. Set LW_XVE_SW_MSG_CTRL_TRIGGER_TRIGGER to LW_XVE_SW_MSG_CTRL_TRIGGER_PENDING.
        // Then check for LW_XVE_SW_MSG_CTRL_TRIGGER to be equal to
        // LW_XVE_SW_MSG_CTRL_TRIGGER_NOT_PENDING to confirm that the SW_MSG was sent up.
        //

        // Enable override
        val = REG_RD32(BAR0_CFG, LW_XVE_SW_MSG_CTRL);
        val = FLD_SET_DRF( _XVE, _SW_MSG_CTRL, _REQ_ID_OVERRIDE, _ENABLE, val);
        REG_WR32(BAR0_CFG, LW_XVE_SW_MSG_CTRL, val);

        // Set packet values
        val = 0;
        val = FLD_SET_DRF_NUM(_XVE, _SW_MSG_HDR_DW0, _FORMAT, 2, val);
        val = FLD_SET_DRF_NUM(_XVE, _SW_MSG_HDR_DW0, _LENGTH, 0x1, val);
        REG_WR32(BAR0_CFG, LW_XVE_SW_MSG_HDR_DW0, val);

        val = 0;
        val = FLD_SET_DRF_NUM(_XVE, _SW_MSG_HDR_DW1, _MSG_TYPE, 0xf, val);
        val = FLD_SET_DRF_NUM(_XVE, _SW_MSG_HDR_DW1, _REQ_ID_BUS_NO, 1, val);
        REG_WR32(BAR0_CFG, LW_XVE_SW_MSG_HDR_DW1, val);

        REG_WR32(BAR0_CFG, LW_XVE_SW_MSG_HDR_DW2, REG_RD32(BAR0_CFG, LW_XVE_MSI_LOW_ADDR));
        REG_WR32(BAR0_CFG, LW_XVE_SW_MSG_HDR_DW3, REG_RD32(BAR0_CFG, LW_XVE_MSI_UPPER_ADDR));
        REG_WR32(BAR0_CFG, LW_XVE_SW_MSG_DATA, REG_RD32(BAR0_CFG, LW_XVE_MSI_DATA));

        // Do the trigger
        val = REG_RD32(BAR0_CFG, LW_XVE_SW_MSG_CTRL);
        val = FLD_SET_DRF( _XVE, _SW_MSG_CTRL, _TRIGGER, _PENDING, val);
        REG_WR32(BAR0_CFG, LW_XVE_SW_MSG_CTRL, val);

        // Poll for SW_MSG send
        osPTimerTimeNsLwrrentGet(&timeOut);
        while (LW_TRUE)
        {
            //
            // Once we set the trigger in SW_MSG_CTRL register,
            // we should wait for it to go low to know that msg has been sent out
            //
            val = REG_RD32(BAR0_CFG, LW_XVE_SW_MSG_CTRL);
            if (FLD_TEST_DRF(_XVE, _SW_MSG_CTRL, _TRIGGER, _NOT_PENDING, val))
            {
                break;
            }

            //
            // TODO - SC,
            // For now to just lossen the loop. Need to come up a better way for logging
            // Ideally it's better to HAL this
            // The GC6 error code is in Turing only.
            // But we don't have BIF HAL before Turing
            // So put those as a future work on me and just break for now
            //
            if (osPTimerTimeNsElapsedGet(&timeOut) >= BIF_TLP_MSGSEND_TIMTOUT_NS)
            {
                pgGc6LogErrorStatus(GCX_GC6_SW_MSG_SEND_TIMEOUT);
                break;
            }
        }
    }

    // Restore RM Oneshot Context after exiting GC6
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_OSM) &&
        PmuGc6.pCtx->rmOneshotCtx.bValid)
    {
        DispContext.pRmOneshotCtx->bRmAllow          = PmuGc6.pCtx->rmOneshotCtx.bRmAllow;
        DispContext.pRmOneshotCtx->head.idx          = PmuGc6.pCtx->rmOneshotCtx.headIdx;
    }

    // Let RM know we are bootstrapped.
    pmuUpdateCheckPoint_HAL(&Pmu, CHECKPOINT_BOOTSTRAP_COMPLETED);

    if (PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_MODE_SET) ||
        PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_GC6_EXIT))
    {
        Disp.pPmsCtx = &PmuGc6.pCtx->pmsCtx;
    }
}

/*!
 * @brief      Callwlate utilization % based on the bytes transferred
 * @param[in]  bytes  Bytes transferred
 *
 * @return     LwU16  Utilization pct with .01% accuracy.
 */
static LwU16
s_pmuCallwtilPct_GMXXX(LwU32 bytes)
{
    LwU32 regVal;
    LwU32 baseBytes;
    LwU32 availableTimeus;
    LwU32 mulVal;
    LwU32 divVal;

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
                    osPTimerTimeNsElapsedGet(&PerfmonPreTime), 1000);

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

/*!
 * @brief Generate host idle percentage on Tx direction based on
 *        LW_XVE_PCIE_UTIL_TX_BYTES register
 *
 * @return LwU16  Utilization pct with .01% accuracy.
 */
LwU16
pmuPEXCntSampleTx_GMXXX(void)
{
    LwU32 countCtrl;
    LwU32 regVal;
    LwU32 bytes;

    countCtrl = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL);
    if (DRF_VAL(_XVE_PCIE, _UTIL_CTRL, _ENABLE, countCtrl) != 1)
    {
        // At this point these counters should have been enabled.
        PMU_HALT();
        return 0;
    }

    //
    // Retrieve the number of bytes transferred in Tx direction and
    // use it to callwlate Tx Utilization
    //

    regVal = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_TX_BYTES);
    bytes  = DRF_VAL(_XVE_PCIE, _UTIL_TX_BYTES, _COUNT, regVal);

    // Reset counters
    countCtrl = FLD_SET_DRF(_XVE_PCIE, _UTIL_CTRL, _RESET_TX_BYTES_COUNT,
                            _PENDING, countCtrl);
    REG_WR32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL, countCtrl);

    return s_pmuCallwtilPct_GMXXX(bytes);
}


/*!
 * @brief Generate host idle percentage on Rx direction based on
 *        LW_XVE_PCIE_UTIL_RX_BYTES registers
 *
 * @return LwU16  Utilization pct with .01% accuracy.
 */
LwU16
pmuPEXCntSampleRx_GMXXX(void)
{
    LwU32 countCtrl;
    LwU32 regVal;
    LwU32 bytes;

    countCtrl = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL);
    if (DRF_VAL(_XVE_PCIE, _UTIL_CTRL, _ENABLE, countCtrl) != 1)
    {
        // At this point these counters should have been enabled.
        PMU_HALT();
        return 0;
    }

    //
    // Retrieve the number of bytes transferred in Rx direction and
    // use it to callwlate Rx Utilization
    //

    regVal = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_RX_BYTES);
    bytes  = DRF_VAL(_XVE_PCIE, _UTIL_RX_BYTES, _COUNT, regVal);

    // Reset counters
    countCtrl = FLD_SET_DRF(_XVE_PCIE, _UTIL_CTRL, _RESET_RX_BYTES_COUNT,
                            _PENDING, countCtrl);
    REG_WR32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL, countCtrl);

    return s_pmuCallwtilPct_GMXXX(bytes);
}

/*!
 * @brief Enable PEX utilization couters.
 */
void
pmuPEXCntInit_GMXXX(void)
{
    LwU32 countersCtrl;
    LwU32 countersEnable;

    countersCtrl = REG_RD32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL);
    countersEnable = FLD_SET_DRF_NUM(_XVE_PCIE, _UTIL_CTRL, _ENABLE, 1, countersCtrl);
    if (countersCtrl == countersEnable)
    {
        return;
    }
    REG_WR32(BAR0_CFG, LW_XVE_PCIE_UTIL_CTRL, countersEnable);
}

/*!
 * @brief Restore PCIE Config Space Data From PMU
 *
 * @param[in]  pPciCfgAddr  Pointer with addresses of Data and Regmap Surfaces
 *
 * @return    'FLCN_OK' if successful,
 *            'FLCN_ERR_NO_FREE_MEM' if unable to allocate memory,
 *            'FLCN_ERROR' otherwise.
 *
 * Restore Steps :
 * 1. Get Memory address of free Heap memory to be used as buffer
 * 2. Get Surface addresses to get Data and Regmap
 * 3. Obtain Regmap Data from Surface in one shot (128 Byte Read)
 * 4. Obtain Config Space Data from Surface in chunks of 256 Byte each
 *      4.a Obtain Data
 *      4.b Restore Data based on the regmap
 * 5. Enable CMD MEMORY SPACE to convey RM that restore has finished
 * RETURN & EXIT
 */
static FLCN_STATUS
s_pmuRestorePciConfigRegisters_GMXXX
(
    RM_PMU_PCI_CONFIG_ADDR *pPciCfgAddr
)
{
    LwU32 regMapWriteIndex = 0;
    LwU32 offset           = 0;
    LwU32 *pRegData        = 0;
    LwU32 *pRegmap         = 0;
    LwU32 regIndex         = 0;
    LwU32 regOffset        = 0;
    LwU32 regmapIndexBit   = 0;
    LwU32 size             = PCI_CONFIG_SPACE_SIZE;
    LwU32 copySize;
    LwU32 dmaSize;
    LwU32 dmaOffset;
    LwU32 dmaOffsetOriginal;
    LwU8  unalignedBytes;
    LwU32 regmapMaxIndex;
    RM_FLCN_MEM_DESC *pSrc;
    RM_FLCN_MEM_DESC *pSrcRegmap;
    FLCN_STATUS status = FLCN_OK;

    //Step 1
    /********************************WARNING**********************************/
    /*        The Following Code is a Hack into the PMU DMEM Heap            */
    /*        This code SHOULD NOT BE COPIED OR USED ANYWHERE IN             */
    /*             THE PMU WITHOUT PERMISSION FROM PMU OWNER                 */
    /*************************************************************************/

    /**************************NEED FOR THE HACK******************************/
    /*       256 + 128 bytes of Buffer is needed to store config             */
    /*      data + regmap. A local buffer would be assigned on the           */
    /*      stack which might lead to stack overloading error. A global      */
    /*      buffer cannot be freed (no support in PMU), hence this would     */
    /*      occupy heap memory throughout the lifecycle of PMU               */
    /* So here we use a chunk of memory in heap without actually allocating  */
    /*   the memory. Hence no frreing of memory is required. This chunk      */
    /*   would later get overwritten by some other PMU function, hence       */
    /*                       no wastage of memory                            */
    /*************************************************************************/

    LwU8 *heapBuffer;
    heapBuffer = (LwU8 *)lwosOsHeapGetNextFreeByte();
    if (heapBuffer == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        PMU_BREAKPOINT();
        goto s_pmuRestorePciConfigRegisters_GMXXX_exit;
    }
    heapBuffer = PMU_ALIGN_UP_PTR(heapBuffer, DMA_MIN_READ_ALIGNMENT);

    if (((LwU32)lwosOsHeapGetFreeHeap()) < HEAP_BUFFER_SIZE_NEEDED)
    {
        status = FLCN_ERROR;
        PMU_BREAKPOINT();
        goto s_pmuRestorePciConfigRegisters_GMXXX_exit;
    }

    /*****************************END OF HACK*********************************/

    //Step 2
    pSrc            = &pPciCfgAddr->pciAddr;
    pSrcRegmap      = &pPciCfgAddr->regmapAddr;
    regmapMaxIndex  = pPciCfgAddr->regmapMaxIndex;

    //Step 3 - Extract DMA offset and clear it.
    dmaOffsetOriginal = *((LwU8 *)&pSrcRegmap->address);
    *((LwU8 *)&pSrcRegmap->address) = 0;

    unalignedBytes = dmaOffsetOriginal & (PCI_DMA_READ_ALIGNMENT - 1);
    dmaOffset      = ALIGN_DOWN(dmaOffsetOriginal, PCI_DMA_READ_ALIGNMENT);
    dmaSize        = LW_MIN(ALIGN_UP(PCI_REGMAP_SIZE + unalignedBytes,
                                     PCI_DMA_READ_ALIGNMENT),
                            PCI_REGMAP_BUFFER_SIZE);

    status = dmaRead(heapBuffer, pSrcRegmap, dmaOffset, dmaSize);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_pmuRestorePciConfigRegisters_GMXXX_exit;
    }

    pRegmap = (LwU32 *)(heapBuffer + unalignedBytes);

    // Increment heap Buffer to get a buffer for Config Space Data

    heapBuffer += ((PCI_REGMAP_BUFFER_SIZE + DMA_MIN_READ_ALIGN_MASK) &
                                                ~DMA_MIN_READ_ALIGN_MASK);

    // Step 4, program LW_XVE_PRIV_XV_0
    pgGc6ExitSeq_HAL();

    // Extract DMA offset and clear it.
    dmaOffsetOriginal = *((LwU8 *)&pSrc->address);
    *((LwU8 *)&pSrc->address) = 0;

    unalignedBytes = dmaOffsetOriginal & (PCI_DMA_READ_ALIGNMENT - 1);

    //Read Config Space Data in chunks of 256 bytes from the surface
    //No Need to read further if regmap indexes have exhausted
    while ((size > 0) && (regMapWriteIndex < regmapMaxIndex))
    {
        //Step 4.a
        dmaOffset  = ALIGN_DOWN(dmaOffsetOriginal + offset, PCI_DMA_READ_ALIGNMENT);
        dmaSize    = LW_MIN(ALIGN_UP(size + unalignedBytes,
                                     PCI_DMA_READ_ALIGNMENT),
                            PCI_DATA_BUFFER_SIZE);
        copySize   = LW_MIN(size, PCI_DATA_BUFFER_SIZE - unalignedBytes);

        status = dmaRead(heapBuffer, pSrc, dmaOffset, dmaSize);
        if ((status != FLCN_OK) &&
            (size > 0))
        {
            PMU_BREAKPOINT();
            goto s_pmuRestorePciConfigRegisters_GMXXX_exit;
        }

        pRegData = (LwU32 *)(heapBuffer + unalignedBytes);

        //Step 4.b
        regIndex = 0;
        while ((regIndex < PCI_MAX_REGISTER_INDEX) &&
               (regMapWriteIndex < regmapMaxIndex))
        {
            for (; (regIndex < PCI_MAX_REGISTER_INDEX) &&
                   (regmapIndexBit < PCI_REGMAP_INDEX_SIZE)
                 ;  regmapIndexBit++, regIndex++)
            {
                if ((BIT32(regmapIndexBit) & pRegmap[regMapWriteIndex]) == 0)
                {
                    continue;
                }

                regOffset =
                    (regMapWriteIndex * PCI_REGMAP_INDEX_SIZE + regmapIndexBit)
                    * PCI_REGISTER_SIZE;

                // restore the CFG space, PLM check inside HALs
                pgGc6CfgSpaceRestore_HAL(&Pg, regOffset, pRegData[regIndex]);

            }

            if (regmapIndexBit >= PCI_REGMAP_INDEX_SIZE)
            {
                regMapWriteIndex++;
                regmapIndexBit = 0;
            }
        }

        size -= copySize;
        offset += copySize;
        unalignedBytes = 0;
    }

    //Restore BAR1/2 Block registers
    PMU_ASSERT_OK_OR_GOTO(status,
        pmuBarBlockRegistersRestore_HAL(&Pmu, pPciCfgAddr),
        s_pmuRestorePciConfigRegisters_GMXXX_exit);

    REG_WR32(BAR0, LW_UFLUSH_FB_FLUSH,
             DRF_DEF( _UFLUSH, _FB_FLUSH, _PENDING, _BUSY));

    //Step 5
    REG_FLD_WR_DRF_DEF(BAR0_CFG, _XVE, _DEV_CTRL, _CMD_MEMORY_SPACE, _ENABLED);

    heapBuffer = (LwU8 *)lwosOsHeapGetNextFreeByte();
    if (heapBuffer == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        PMU_BREAKPOINT();
        goto s_pmuRestorePciConfigRegisters_GMXXX_exit;
    }
    memset(heapBuffer, 0x00, HEAP_BUFFER_SIZE_NEEDED);

s_pmuRestorePciConfigRegisters_GMXXX_exit:
    return status;
}

/*!
 * @brief Sync the I2CS handoff from SCI to PMGR on GC6 exit,
 *        sequence from bug 1508560
 *
 * @return  FLCN_OK             On success
 * @return  FLCN_ERR_TIMEOUT    If register polling times out
 */
static FLCN_STATUS
s_pgSyncI2CSHandoff_GMXXX()
{
    LwU32 data = 0;
    FLCN_STATUS status = FLCN_OK;

    // Step 1: Poll LW_PGC6_SCI_DEBUG_STATUS_I2CS_STATUS until it is EMPTY
    if (!PMU_REG32_POLL_US(
        USE_FECS(LW_PGC6_SCI_DEBUG_STATUS),
        DRF_SHIFTMASK(LW_PGC6_SCI_DEBUG_STATUS_I2CS_STATUS),
        DRF_DEF(_PGC6, _SCI_DEBUG_STATUS, _I2CS_STATUS, _EMPTY),
        GC6M_I2CS_STATE_SECONDARY_IDLE_TIMEOUT_USEC,
        PMU_REG_POLL_MODE_BAR0_EQ))
    {

        //
        // Intentionally remove the sanity check.
        // The sanity check is added in CL#25171333
        // and it caught some issue in the in field fake I2C design.
        // See bug#200520044 and bug#2040582, http://lwbugs/2540582/231
        // Given this impacts shipping product in GC6 path we will markout the sanity check here.
        // The reason to not disable fake I2C in a common fashion is some early shipped machine rely on
        // fake I2C to reply GPU temp if EC is querying during GC6.
        // If no fake I2C the return value will be 0xff and that causes EC think GPU is overheat
        // So markout the specific check here.
        // Tracking bug please see bug#200532515
        //
    }

    // Step 2.Set LW_PGC6_SCI_GPU_I2CS_REPLAY_THERM_I2CS_STATE to VALID
    data = REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_REPLAY);
    data = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_REPLAY, _THERM_I2CS_STATE, _VALID, data);
    REG_WR32(FECS, LW_PGC6_SCI_GPU_I2CS_REPLAY, data);

    // Step 3.Set LW_PGC6_SCI_GPU_I2CS_CFG_I2C_ADDR_VALID to YES
    data =  REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG);
    data = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_CFG, _I2C_ADDR_VALID, _YES, data);
    REG_WR32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG, data);

    // Step 4.Restore LW_PGC6_SCI_DEBUG_I2CS.
    data =  REG_RD32(FECS, LW_PGC6_SCI_LW_SCRATCH_GC6_I2C_SCRATCH);
    REG_WR32(FECS, LW_PGC6_SCI_DEBUG_I2CS, data);

    return status;
}

