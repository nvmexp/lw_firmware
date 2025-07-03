/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgislandgm10x.c
 * @brief  HAL-interface for the GM10X PGISLAND
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"

#include "dev_gc6_island.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "lwuproc.h"
#include "regmacros.h"
#include "flcntypes.h"
#include "lwos_dma.h"
#include "pmu_didle.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "acr/pmu_objacr.h"
#include "pmu_objfuse.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_pri.h"
#include "dev_pmgr.h"
#include "dev_therm.h"
#include "dev_trim.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

#include "config/g_pg_private.h"
#include "config/g_acr_private.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_PGC6_BSI_PHASE_OFFSET    1
#define LW_PGC6_BSI_PHASE_SETUP     0

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void s_pgIslandEnableInterrupts_GM10X(void)
    GCC_ATTRIB_SECTION("imem_init", "pgIslandEnableInterrupts_GM20X");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Initialize PgIsland
 *
 * This function does the initialization of pgisland before scheduler schedules PG
 * task for the exelwtion.
 */
void
pgIslandPreInit_GM10X(void)
{
    // enable wakeup and status interrupts in SCI
    s_pgIslandEnableInterrupts_GM10X();

    // It is anticipated that LDO characterization will be performed during ATE binning and an
    // ideal threshold value be written into fuse TBD.SW has to copy fuse value into LW_PGC6_SCI_VTTGEN_CNTL_THRESHOLD
    // of register LW_PGC6_SCI_VTTGEN_CNTL

    // TODO: Read from the fuse
    // vttgenCntlThreshold = GPU_READ_REG32(pGpu,  LW_PGC6_BSI_VTTGEN_CNTL_FUSE);
    // GPU_FLD_WR_DRF_NUM(pGpu, _PGC6, _SCI_VTTGEN_CNTL, _THRESHOLD, vttgenCntlThreshold);

    //TODO :(Most of these should be Init Values or should be done by Devinit)
    // Fill the Timeout Intervals, used by SCI Primary State Machine to introduce minimum
    // delays or to detect hang in different steps in entry/exit sequence
    // Index 0 timeout is used for GPU_EVENT assertion duration by the SCI
    // Index 1 timeout is used for the hold off interval during a GC6 abort
    // Index 2 timeout is used for detecting a system reset from the EC
    // Index 3 timeout is used for the hold off interval for a GPU_EVENT retry during a GC6 exit
    // LW_PGC6_SCI_TIMEOUT_TMR_CNTL(i) needs to be written with timeouts read from VBIOS or other source.

    // These should have INIT valuees
    // TODO: Depending on the initial values of various exit/abort conditions in the registers
    // LW_PGC6_SCI_WAKE_EN, LW_PGC6_SCI_WAKE_RISE_EN, LW_PGC6_SCI_WAKE_FALL_EN, these registers
    // have to be configured appropriately to allow/disallow various exit/abort conditions
    // to cause GC5/GC6 exit or abort, IF REQUIRED

    // Update required I2C state in SCI
    pgIslandUpdateSCII2CState_HAL();
}

/*!
 * @brief Get BSI RAM size
 */
LwU32
pgIslandGetBsiRamSize_GM10X(void)
{
    return (DRF_VAL( _PGC6, _BSI_RAM, _SIZE, REG_RD32(BAR0, LW_PGC6_BSI_RAM))) << 10;
}

/*!
 * @brief Check the data that is being requested to copy into BSI RAM
 *
 * This function checks the copy request and data destined for the BSI RAM
 * and is to be used for security reasons when the data is compiled by RM.
 */
FLCN_STATUS
pgIslandBsiRamCopyCheck_GM10X
(
    void               *pDmaBuffer,
    LwU32               size,
    LwU32               bsiBootPhaseInfo
)
{
    LwU32 gc5Start;
    LwU32 gc5Stop;
    LwU32 gc6Start;
    LwU32 gc6Stop;

    //
    // Make sure that the size does not overrun
    // We do not offset by BRSS as it was not productized with GC6 on Maxwell
    //
    if ((size == 0) || (size > pgIslandGetBsiRamSize_HAL(&Pg)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Make sure that the size is aligned
    if (!LW_IS_ALIGNED(size, DMA_MIN_READ_ALIGNMENT))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check the phase pointers
    gc5Start = REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC5START, bsiBootPhaseInfo);
    gc5Stop  = REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC5STOP,  bsiBootPhaseInfo);
    gc6Start = REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6START, bsiBootPhaseInfo);
    gc6Stop  = REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP,  bsiBootPhaseInfo);

    // Conditions : gc5 is only one phase (0), and gc6 is phase 1 - n.
    if ((gc5Start != gc5Stop) ||
        (gc6Start > gc6Stop)  ||
        (gc5Start > gc6Stop)  ||
        (gc5Start > gc6Start))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return FLCN_OK;
}

/*!
 * @brief Copies data from FB/SYS MEM to BSI RAM
 *
 * Function copies pSrc->size bytes from FB/SYS MEM to BSI RAM. The source
 * buffer address is callwlated as :
 *                          (base << 8) + offset
 *
 * The function expects that source address should be aligned to 16-bytes and
 * the number of bytes requested should be a multiple of 16-bytes.
 *
 * @param[in]  destAddress       Address in BSI ram at which
 *                               we need to copy the data.
 * @param[in]  pSrcDesc          Pointer to Memory descriptor
 * @param[in]  size              Number of bytes to be transferred
 * @param[in]  bsiBootPhaseInfo  Phase information for configuring BSI
 *
 * @return FLCN_OK
 */
FLCN_STATUS
pgIslandBsiRamCopy_GM10X
(
    void               *pDmaBuffer,
    LwU32               size,
    LwU32               bsiBootPhaseInfo
)
{
    PMU_RM_RPC_STRUCT_GCX_PGISLAND_GC6_INIT_STATUS rpc;
    FLCN_STATUS status;
    LwU32       bytesCopied      = 0;
    LwU32       ramCtrl          = 0;
    LwU32       val              = 0;
    LwU32       timer            = 0;
    // Address in SS offset to indicate if SS data is valid
    LwU16       destAddress      = 0;
    // Address in SS offset at which we need to copy the data from
    LwU32       bsiRamSSOffset = 0;

    // Reset the BSI settings to avoid cycles before payload sanitation
    pgIslandResetBsiSettings_HAL(&Pg);

    timer = REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0);

    bsiRamSSOffset = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.bsiRam.bsiRamSurface.buffer);

    // Set destination address in BSI RAM
    ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, val);
    val = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, destAddress, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, val);
    DBG_PRINTF_DIDLE(("DI-GC6: BSI_COPY destAddress 0x%x, Size %d\n",
                      destAddress, size, 0, 0));

    while (bytesCopied < size)
    {
        if (FLCN_OK != ssurfaceRd(pDmaBuffer,
                               bsiRamSSOffset + bytesCopied,
                               PG_BUFFER_SIZE_BYTES))
        {
            // DMA tranfer failed
            PMU_HALT();
        }

        // yield before attempting next copy
        lwrtosYIELD();

        // 2) Copy data from DMEM to BSI RAM
        if ((size - bytesCopied) < PG_BUFFER_SIZE_BYTES)
        {
            for (val = 0; val < (size - bytesCopied)/4; val++)
            {
                REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, *((LwU32 *)pDmaBuffer + val));
            }
        }
        else
        {
            for (val = 0; val < PG_BUFFER_SIZE_DWORDS; val++)
            {
                REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, *((LwU32 *)pDmaBuffer + val));
            }
        }

        bytesCopied += PG_BUFFER_SIZE_BYTES;
        DBG_PRINTF_DIDLE(("DI-GC6: BSI_COPY bytesCopied %d, dmaOffset 0x%x, dmaBase 0x%x\n",
                            bytesCopied, dmaOffset, dmaBase, 0));
    }

    //
    // Sanitize the contents to prevent privilege escalation
    // All level-2 requests have to be patched after this step
    //
    pgIslandSanitizeBsiRam_HAL(&Pg, bsiBootPhaseInfo);

    // If ACR is enabled then patch ACR info in BSI RAM
    if (acrCheckRegionSetup_HAL(&Acr) == FLCN_OK &&
        (REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP, bsiBootPhaseInfo) > ACR_GC6_SEC_REGION_LOCKDOWN_PHASE_ID))
    {
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libAcr));

        acrPatchRegionInfo_HAL(&Acr, ACR_GC6_SEC_REGION_LOCKDOWN_PHASE_ID);

        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libAcr));
    }

    pgGc6PatchForOptimizedVoltage_HAL(&Pg, LW_PGC6_BSI_PHASE_OFFSET +
                                            LW_PGC6_BSI_PHASE_SETUP);

    pgIslandPatchForBrss_HAL(&Pg, bsiBootPhaseInfo);

    pgIslandBsiRamConfig_HAL(&Pg, bsiBootPhaseInfo);

    // Write timestamp
    REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(1),
             REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0)- timer);
    // Restore RAMCTRL
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);

    DBG_PRINTF_DIDLE(("DI-GC6: BSI_COPY Done. DidleLwrrState %u\n",
                        DidleLwrState, 0, 0, 0));

    rpc.status  = FLCN_OK;
    // Send the Event as an RPC.  RC-TODO Propery handle status here.
    PMU_RM_RPC_EXELWTE_BLOCKING(status, GCX, PGISLAND_GC6_INIT_STATUS, &rpc);

    return FLCN_OK;
}

/*!
 *  @brief Program SCI wakeup timer.
 */
void
pgIslandProgramSciWakeupTimerSet_GM10X
(
    LwU32         wakeupTimeUs
)
{
    LwU32 val;

    // Enable wake by wakeup timer
    val = REG_RD32(FECS, LW_PGC6_SCI_WAKE_EN);
    val = FLD_SET_DRF(_PGC6, _SCI_WAKE_EN, _WAKE_TIMER, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_WAKE_EN, val);

    // Also enable SW reporting
    val = REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_EN);
    val = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _WAKE_TIMER, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_SW_INTR0_EN, val);

    // Set wakeup time
    val = 0;
    val = FLD_SET_DRF_NUM(_PGC6, _SCI_WAKEUP_TMR_CNTL, _TIMER, wakeupTimeUs, val);
    REG_WR32(FECS, LW_PGC6_SCI_WAKEUP_TMR_CNTL, val);
}

// This code does not compile on HBM-supported builds.
#if PMUCFG_FEATURE_ENABLED(PMU_FB_DDR_SUPPORTED)
/*!
 *  @brief Set any FB/regulator state before GC6 power down
 */
GCX_GC6_ENTRY_STATUS
pgGc6PrepareFbForGc6_GM10X(LwU8 ramType)
{
    LwU32               fbioCtl;
    LwU32               fbioCtlInit;
    LwU32               val;
    LwU32               val2;

    //
    // For details, refer to 1282370 #26
    // Switch back to Main regulator for all FB partitions
    // including Floorswept
    //
    fbioCtl     = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_SPARE);
    fbioCtlInit = FLD_SET_DRF(_PTRIM, _SYS_FBIO_SPARE, _FBIO_SPARE, _INIT, fbioCtl);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, fbioCtlInit);
    OS_PTIMER_SPIN_WAIT_NS(1000);

    // Assert FBCLAMP
    val  = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE);
    val  = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_ARMED_SEQ_STATE, _FB_CLAMP,
                       _ASSERT, val);
    val2 = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE);
    val2 = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_STATE, _UPDATE, _TRIGGER, val2);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE, val);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE, val2);

    return GCX_GC6_OK;
}
#endif      // PMUCFG_FEATURE_ENABLED(PMU_FB_DDR_SUPPORTED)

/*!
 *  @brief Enable SCI STATUS and WAKEUP interrupts.
 *
 *  @return      void                   return nothing
 */
static void
s_pgIslandEnableInterrupts_GM10X(void)
{
    LwU32 mask = 0;

    // Enable all relevant wakeup interrupts.
    mask = (DRF_DEF(_PGC6, _SCI_WAKE_EN, _GPU_EVENT, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _I2CS,      _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _BSI_EVENT, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _HPD_IRQ_0, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _HPD_IRQ_1, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _HPD_IRQ_2, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _HPD_IRQ_3, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _HPD_IRQ_4, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_EN, _HPD_IRQ_5, _ENABLE));
    REG_WR32(FECS, LW_PGC6_SCI_WAKE_EN, mask);

    mask = (DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _HPD_0, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _HPD_1, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _HPD_2, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _HPD_3, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _HPD_4, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _HPD_5, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _POWER_ALERT, _ENABLE));
    //
    // Disable POWER_ALERT for self wake-up interrupt.
    // WAR for shipped BIOS force checking FB_EN status
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_GPIO_FORCE_ILWALID_WAR_1877803))
    {
        // Disable GPIO#12 self wake-up. Overwrite previous settings
        mask = FLD_SET_DRF(_PGC6, _SCI_WAKE_RISE_EN, _POWER_ALERT, _DISABLE, mask);
    }

    REG_WR32(FECS, LW_PGC6_SCI_WAKE_RISE_EN, mask);

    mask = (DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _HPD_0, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _HPD_1, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _HPD_2, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _HPD_3, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _HPD_4, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _HPD_5, _ENABLE)             |
            DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _POWER_ALERT, _ENABLE));

    if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_GPIO_FORCE_ILWALID_WAR_1877803))
    {
        // Disable GPIO#12 self wake-up. Overwrite previous settings
        mask = FLD_SET_DRF(_PGC6, _SCI_WAKE_FALL_EN, _POWER_ALERT, _DISABLE, mask);
    }

    REG_WR32(FECS, LW_PGC6_SCI_WAKE_FALL_EN, mask);

    // we need status for all interrupts. Just set to 0xffff
    mask = ~((LwU32)0);
    REG_WR32(FECS, LW_PGC6_SCI_SW_INTR1_RISE_EN, mask);
    REG_WR32(FECS, LW_PGC6_SCI_SW_INTR1_FALL_EN, mask);

   //
   // Dont enable wakeup timer expiry because it is reported
   // irrespective of whether it caused an exit or not.
   //
   mask = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _WAKE_TIMER, _DISABLE, mask);
   REG_WR32(FECS, LW_PGC6_SCI_SW_INTR0_EN, mask);
}

/*!
 *  @brief Update required SCI I2C state
 *  During GC5, SCI is resposbile for monitoring I2CS transactions
 *  to the GPU. SCI has to be initialized with I2C state required for
 *  monitring I2CS transaction.
 */
void
pgIslandUpdateSCII2CState_GM10X(void)
{
    LwU32 regVal;
    LwU32 i2cCfgSpike;
    LwU32 i2cClkDiv;
    LwU32 replaySpeed;

    //
    // Callwlate the replay speed
    // replay speed = ceiling (((1 + I2C_CFG_SPIKE * (I2C_CLK_DIV + 1)) / 4)) - 1
    //
    i2cCfgSpike = REG_FLD_RD_DRF(FECS, _THERM, _I2C_CFG, _SPIKE);
    i2cClkDiv   = REG_FLD_RD_DRF(FECS, _THERM, _I2C_CLK_DIV, _VAL);
    replaySpeed = LW_CEIL((1 + i2cCfgSpike * (i2cClkDiv + 1)), 4) - 1;
    regVal      = REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG);

    // Set the ARP Address Valid
    regVal = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_CFG, _ARP_ADDR_VALID, _YES, regVal);

    // Set the replay speed
    regVal = FLD_SET_DRF_NUM(_PGC6, _SCI_GPU_I2CS_CFG, _REPLAY_SPEED,
                              replaySpeed, regVal);

    REG_WR32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG, regVal);
}

/*!
 * @brief check if GC6 Island is enabled
 *
 *
 * @return  LW_TRUE
 *     If GC6 Island is enabled
 * @return  LW_FALSE
 *     Otherwise
 *
 */
LwBool
pgIslandIsEnabled_GM10X(void)
{
    LwU32    fuse   = 0;

    fuse = fuseRead(LW_FUSE_OPT_GC6_ISLAND_DISABLE);
    return FLD_TEST_DRF(_FUSE_OPT, _GC6_ISLAND_DISABLE, _DATA, _NO, fuse);
}
