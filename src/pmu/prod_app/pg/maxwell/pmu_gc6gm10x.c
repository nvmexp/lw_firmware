/*
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6gm10x.c
 * @brief  PMU Power State (GCx) Hal Functions for GM10X
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "pmu_gc6.h"
#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_bus.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/*-------------------------- Macros ------------------------------------------ */
/*-------------------------- Type Definitions ------------------------------------------ */

#ifndef LW_CPWR_THERM_I2CS_STATE_VALUE_SLAVE_IDLE_TRUE
#define LW_CPWR_THERM_I2CS_STATE_VALUE_SLAVE_IDLE_TRUE       0x1
#define LW_CPWR_THERM_I2CS_STATE_VALUE_SLAVE_IDLE_FALSE      0x0
#endif

#define GC6M_I2CS_STATE_SECONDARY_IDLE_TIMEOUT_USEC  2000

/* ------------------------- Prototypes ------------------------------------ */
static void s_gc6PmuDetachResident(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
    GCC_ATTRIB_SECTION("imem_rtd3Entry", "s_gc6PmuDetachResident")
    GCC_ATTRIB_NOINLINE();

/* ------------------------- Public Functions ------------------------------ */
/*!
 * @brief Write GC6 entry status to scratch register for debug purpose
 *        Any error detected in GC6 entry is fatal and we need to fall into
 *        error path and detect it in the next boot.
 *
 *        In order to log all the error encountered, need to transfer the
 *        status code into mask so we can have multiple error log.
 *
 * @param[in]   status    the checkpoint to update in the scratch register
 */
void
pgGc6UpdateErrorStatusScratch_GM10X()
{
    LwU32 status;

    status = REG_RD32(FECS, LW_PGC6_BSI_SCRATCH(LW_PGC6_BSI_SCRATCH_INDEX_GC6_ENTRY_ERROR));

    if ((GC6_ENTRY_STATUS_INIT_MAGIC == status) &&
        (GC6_ENTRY_STATUS_MASK_INIT != PmuGc6.gc6EntryStatusMask))
    {
        // The first write is a overwrite.
        REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(LW_PGC6_BSI_SCRATCH_INDEX_GC6_ENTRY_ERROR), PmuGc6.gc6EntryStatusMask);
        PmuGc6.gc6EntryStatusMask = GC6_ENTRY_STATUS_MASK_INIT;
    }
}

/*!
 * @brief Init GC6 entry status scratch register for debug purpose
 */
void
pgGc6InitErrorStatusLogger_GM10X()
{
    // Initial entry status mask for SW caching and the register to be written to.
    PmuGc6.gc6EntryStatusMask = GC6_ENTRY_STATUS_MASK_INIT;
    REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(LW_PGC6_BSI_SCRATCH_INDEX_GC6_ENTRY_ERROR), GC6_ENTRY_STATUS_INIT_MAGIC);
}

/*!
 * @brief GC6 wait for link state change and proceed SCI entry.
 * @param[in] gc6DetachMask This is for distinguish different link status
 * @param[out] The link state defined in pmu_gc6.h
 */
LwU8
pgGc6WaitForLinkState_GM10X(LwU16 gc6DetachMask)
{
    LwU32 val;

    // No FGC6 support on Pre-Pascal
    PMU_HALT_COND(!FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                         _FAST_GC6_ENTRY, _TRUE, gc6DetachMask));
    //
    // this needs to have a better approach.
    // If ACPI call fails; pmu should timeout and recover.
    //
    while (LW_TRUE)
    {
        val = REG_RD32(FECS, LW_XP_PL_LTSSM_STATE);
        if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_DISABLED, _TRUE, val))
        {
            break;
        }
    }

    // The delay below is needed after we saw link disabled true.
    OS_PTIMER_SPIN_WAIT_NS(4000);

    return LINK_IN_L2_DISABLED;
}

/*!
 * @brief GC6 SCI trigger for GMXXX, GPXXX chips.
 * @param[in]  isRTD3  This should be always false before Turing
 */
void
pgGc6IslandTrigger_GM10X(LwBool isRTD3, LwBool isFGC6)
{
    LwU32 val;
    LwU32 val2;

    // RTD3/FGC6 is NOT supported before Turing
    PMU_HALT_COND(!isRTD3);
    PMU_HALT_COND(!isFGC6);

    val = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_CTRL, _MODE, _GC6, val);

    val2 = REG_RD32(FECS, LW_PGC6_SCI_CNTL);
    val2 = FLD_SET_DRF(_PGC6, _SCI_CNTL, _ENTRY_TRIGGER, _GC6, val2);

    // trigger the writes close-by
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, val);
    REG_WR32(FECS, LW_PGC6_SCI_CNTL, val2);
}

void
pgGc6DisableHPDInterruptWAR1871547_GM10X(LwU8 internalPanelHPDPort)
{
    LwU32 val;

    //
    // WAR Disable HPD on internal panel - See lwbugs/1761980/62 and lwbugs/1871547
    // Disable the HPD interrupt before clearning SCI interrupts to avoid race
    // condition - internal panel HPD can be triggered after clearing the SCI
    // interrupts register and before disabling internal panel HPD. When this
    // happens SCI will trigger a selfwake exit.
    // When we exit GC6 we want to restore the default state which is to
    // re-enable HPD on internal panel.
    // Enable the WAR only if HPD port number valid i.e. less than max HPD
    // ports
    //
    if (internalPanelHPDPort < LW_PGC6_SCI_HPD_CONFIG__SIZE_1)
    {
        val = REG_RD32(FECS, LW_PGC6_SCI_WAKE_RISE_EN);
        val = FLD_IDX_SET_DRF_DEF(_PGC6, _SCI_WAKE_RISE_EN, _HPD,
            internalPanelHPDPort, _DISABLE, val);
        REG_WR32(FECS, LW_PGC6_SCI_WAKE_RISE_EN, val);
        val = REG_RD32(FECS, LW_PGC6_SCI_WAKE_FALL_EN);
        val = FLD_IDX_SET_DRF_DEF(_PGC6, _SCI_WAKE_FALL_EN, _HPD,
            internalPanelHPDPort, _DISABLE, val);
        REG_WR32(FECS, LW_PGC6_SCI_WAKE_FALL_EN, val);
        val = REG_RD32(FECS, LW_PGC6_SCI_WAKE_EN);
        val = FLD_IDX_SET_DRF_DEF(_PGC6, _SCI_WAKE_EN, _HPD_IRQ,
            internalPanelHPDPort, _DISABLE, val);
        REG_WR32(FECS, LW_PGC6_SCI_WAKE_EN, val);
    }
}

/*!
 * @brief Restore LWVDD to boot voltage. See http://lwbugs/1597920
 * @param[in] PWM VID GPIO
 */
void
pgGc6RestoreBootVoltage_GM10X(LwU8 pwmVidGpioPin)
{
    LwU32 val;

    //
    // Save off the pin as we need to switch it
    // back to full drive on GC6 exit
    //
    REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(1), pwmVidGpioPin);

    // Set SCI PWM to tri-state so we will go to boot voltage
    val = REG_RD32(FECS, LW_PGC6_SCI_GPIO_OUTPUT_CNTL(pwmVidGpioPin));
    val = FLD_SET_DRF(_PGC6, _SCI_GPIO_OUTPUT_CNTL, _DRIVE_MODE, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_GPIO_OUTPUT_CNTL(pwmVidGpioPin), val);
}

/*!
 * @brief Handle I2C state before entering SCI.
 *        Fake I2C to prevent wake from SCI polling is taken cared as well
 * @param[in] bIsFakeI2C check if we are enabling fake I2C
 */
void
pgGc6I2CStateUpdate_GM10X(LwBool bIsFakeI2C)
{
    LwU32 val, val2;

    if (bIsFakeI2C)
    {
        // Set LW_PGC6_SCI_GPU_I2CS_CFG_I2C_ADDR_VALID_NO
        val = REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG);
        val = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_CFG, _I2C_ADDR_VALID, _NO, val);
        REG_WR32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG, val);

        // Copy LW_THERM_I2CS_STATE_VALUE_SLAVE_ADR to LW_PGC6_SCI_DEBUG_I2CS_DEV_ADDR
        // and set LW_PGC6_SCI_DEBUG_I2CS_ADDR_VALID_YES
        val = REG_RD32(CSB, LW_CPWR_THERM_I2CS_STATE);
        val2 = DRF_VAL(_CPWR_THERM, _I2CS_STATE_VALUE, _SLAVE_ADR, val);

        // save off the original value
        val = REG_RD32(FECS, LW_PGC6_SCI_DEBUG_I2CS);
        REG_WR32(FECS, LW_PGC6_SCI_LW_SCRATCH_GC6_I2C_SCRATCH, val);

        val = FLD_SET_DRF_NUM(_PGC6,_SCI_DEBUG_I2CS, _DEV_ADDR, val2, val);
        val = FLD_SET_DRF(_PGC6, _SCI_DEBUG_I2CS, _ADDR_VALID, _YES, val);
        REG_WR32(FECS, LW_PGC6_SCI_DEBUG_I2CS, val);
    }
    else
    {
        // Update I2CS state in SCI
        // Read I2CS secondary state from Therm registers
        val = REG_RD32(CSB, LW_CPWR_THERM_I2CS_STATE);
        val2 = DRF_VAL(_CPWR_THERM, _I2CS_STATE_VALUE, _SLAVE_ADR, val);

        val = REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG);
        val = FLD_SET_DRF_NUM(_PGC6,_SCI_GPU_I2CS_CFG, _I2C_ADDR, val2, val);
        val = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_CFG, _I2C_ADDR_VALID, _YES,
                          val);
        REG_WR32(FECS, LW_PGC6_SCI_GPU_I2CS_CFG, val);
    }
}

/*!
 * @brief disable GC6 abort and clear interrupt
 * @param[in] gc6DetachMask This is for distinguish different link status
 */
void
pgGc6DisableAbortAndClearInterrupt_GM10X()
{
    LwU32 val;

    // Disable GC6 ABORT
    val = REG_RD32(FECS, LW_PGC6_SCI_FSM_CFG);
    val = FLD_SET_DRF(_PGC6, _SCI_FSM_CFG, _SKIP_GC6_ABORT, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_FSM_CFG, val);

    //
    // Clear the SCI interrupt status so that we get the correct wakeup
    // events on GC6 exit
    //
    val = REG_RD32(FECS, LW_PGC6_SCI_CNTL);
    val = FLD_SET_DRF_NUM(_PGC6, _SCI_CNTL, _SW_STATUS_COPY_CLEAR,
                          LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER, val);

    REG_WR32(FECS, LW_PGC6_SCI_CNTL, val);
}


/*!
 * @brief power island configuration function before trigger
 *
 * This function sets up BSI/SCI settings so SCI/BSI GPIO/INTR can work
 * as configured.
 */
void
pgGc6IslandConfig_GM10X
(
    RM_PMU_GC6_DETACH_DATA *pGc6Detach
)
{
    // clear interrupt and disable abort
    pgGc6DisableAbortAndClearInterrupt_HAL();
}

/*!
 * @brief WAR to save/restore clkreq override enable
 */
void
pgGc6ClkReqEnWAR1279179_GM10X(void)
{
    LwU32 val, val2;

    //
    // SW WAR for bug 1279179
    // Read the clkreq input
    //
    val = REG_RD32(FECS, LW_PGC6_BSI_DEBUG2);
    val2 = DRF_VAL(_PGC6, _BSI_DEBUG2, _CLKREQIN, val);

    // write the val into OVERRIDE register and enable override
    val = REG_RD32(FECS, LW_PGC6_BSI_PADCTRL);
    val = FLD_SET_DRF_NUM(_PGC6, _BSI_PADCTRL, _CLKREQEN, val2, val);
    val = FLD_SET_DRF(_PGC6, _BSI_PADCTRL, _OVERRIDE_CLKREQEN, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_PADCTRL, val);
}

/*!
 * @brief GC6 entry sequence for GMXXX chips.
 *
 * @param[in]  pGc6Detach  The RM_PMU_GC6_DETACH_DATA sent by RPC from RM.
 */
void
pgGc6PmuDetach_GM10X(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
{

    LwU16 gc6DetachMask = pGc6Detach->gc6DetachMask;;
    OSTASK_OVL_DESC ovlDesc[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, rtd3Entry)
    };

    if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                     _ISLAND_DISABLED, _TRUE, gc6DetachMask))
    {
        //
        // This should never happen as the islands are required
        // to support GC6 on Maxwell and later
        //
        PMU_HALT();

    }

    //
    // Manually acquire the message queue mutex to ensure that
    // its not held by any task that may become DMA suspended.
    // This is required to ensure that this task will be able
    // to send the detach acknowledgement to the RM (below).
    //
    osCmdmgmtRmQueueMutexAcquire();

    if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                     _RTD3_GC6_ENTRY, _TRUE, gc6DetachMask))
    {
        pgGc6RTD3PreEntryConfig(gc6DetachMask);
    }

    pgGc6LoggingStats(gc6DetachMask);

    // Enter critical section - no other task should execute past this point.
    appTaskCriticalEnter();

    //
    // Lower reset etc. PLM here! Make sure that we do this once we're sure
    // no other tasks will run. This way we know that nothing will be able
    // to interrupt some important task code via RESET after we lower its PLM.
    // Especially important for memtune throttling case (which is also applied here).
    //
    rtd3Gc6EntryActionPrepare(pGc6Detach);

    //
    // Make sure the PMU hardware (interrupts for instance) is
    // configured properly for detached mode operation.
    //
    pmuDetach_HAL(&Pmu);

    pgGc6DetachedMsgSend();

    // Load imem for rtd3 entry code
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDesc);
    {
        // Start of resident code
        s_gc6PmuDetachResident(pGc6Detach);
    }
    // Won't reach here, add for symmetry
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDesc);

    // Control should not reach here
    PMU_HALT();
}

/*!
 * @brief Set therm I2CS state as invalid when state machine is not
 * in the middle of replay
 */
void
pgIlwalidateThermI2csState_GM10X()
{
    LwU32 val;

    //
    // SW WAR to set LW_PGC6_SCI_GPU_I2CS_REPLAY_STATUS to INVALID before
    // GC6 entry due to RTL bug explained below
    //
    // Reference Bug  1255637
    // On GC6 exit, the SCI is supposed to hold off on replaying any
    // SMBus/I2C transaction to THERM until SW restores the THERM state.
    // SW notifies the SCI that THERM is ready by writing
    // LW_PGC6_SCI_GPU_I2CS_REPLAY_THERM_I2CS_STATE to VALID. Normally,
    // the HW sets this field to INVALID when it starts a GC5 or GC6
    // powerdown. However, there is an RTL bug where HW does not set this
    // field to INVALID for GC6 powerdown when Self-Driven Reset mode is
    // enabled (see LW_PGC6_SCI_FSM_CFG_SELF_DRIVE_RST). Because of this
    // RTL bug, on GC6 exit,the SCI wll replay the SMBus/I2C transaction
    // to THERM while THERM is still in reset. This causes the transaction
    // to be not-acknowledged (NACK).
    //
    // WAR is SW to set THERM_I2CS_STATE to INVALID before every entry to
    // GC6, although it is strictly required only in Self-Driven Reset
    // Mode, upon checking State Machine is not middle of replay(By
    // checking REPLAY_STATUS is not PENDING)
    //
    val = REG_RD32(FECS, LW_PGC6_SCI_GPU_I2CS_REPLAY);
    if (FLD_TEST_DRF(_PGC6, _SCI_GPU_I2CS_REPLAY,
                     _STATUS, _NOT_PENDING, val))
    {
        val = FLD_SET_DRF(_PGC6, _SCI_GPU_I2CS_REPLAY,
                          _THERM_I2CS_STATE, _ILWALID, val);

        REG_WR32(FECS,  LW_PGC6_SCI_GPU_I2CS_REPLAY, val);
    }
    else
    {
        //
        // This case, where SCI still replaying the earlier pending I2CS
        // transaction till next GC6 cycle entry is not supposed to happen
        // TODO: Abort GC6 sequence here
        //
        PMU_HALT();
    }
}

/*!
 * @brief Write LW_XVE_PRIV_XV_0 register in GC6 exit
 */
void
pgGc6ExitSeq_GM10X(void)
{
    //
    // TO incoporate manual change
    // where LW_XVE_PRIV_XV_0_CYA_CFG_DL_UP will be moving
    // to a new register and deprecate this field.
    // This is to fix a compile error as this file
    // will be compied in Ampere profile
    // To totally remove the #ifdef we need to seperate the file
    // or fork the whole HAL
    //
#ifdef LW_XVE_PRIV_XV_0_CYA_CFG_DL_UP
    LwU32 val = REG_RD32(BAR0_CFG, LW_XVE_PRIV_XV_0);

    val = FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L0S_ENABLE, _DISABLED, val);
    val = FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L1_ENABLE,  _DISABLED, val);
    val = FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_CFG_DL_UP,  _NORESET,  val);
    REG_WR32(BAR0_CFG, LW_XVE_PRIV_XV_0, val);
#endif
}

/*!
 * @brief restore CFG space register, bypass if L3
 */
void
pgGc6CfgSpaceRestore_GM10X(LwU32 regOffset, LwU32 regValue)
{
    //
    // TEMP: xve_override_id and xve_rom_ctrl is priv protected to level 3.
    // This code is to unblock tot emu testing until
    // the valid bit map is updated by HW to take PLMs into account
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_GC6_PRIV_AWARE_CONFIG_SPACE_RESTORE)) &&
         ((regOffset == LW_XVE_OVERRIDE_ID) ||
          (regOffset == LW_XVE_ROM_CTRL)))
    {
        goto pgGc6CfgSpaceRestore_GM10X_EXIT;
    }

    REG_WR32(BAR0_CFG, regOffset, regValue);

pgGc6CfgSpaceRestore_GM10X_EXIT:
    return;
}

/*!
 * @brief Polling I2C idle before entering GC6
 */
GCX_GC6_ENTRY_STATUS pgGc6PollForI2cIdle_GM10X()
{
    if (pgGc6IsOnRtl_HAL())
    {
        return GCX_GC6_OK;
    }

    if (!PMU_REG32_POLL_US(
        LW_CPWR_THERM_I2CS_STATE,
        DRF_SHIFTMASK(LW_CPWR_THERM_I2CS_STATE_VALUE_SLAVE_IDLE),
        DRF_DEF(_CPWR_THERM, _I2CS_STATE, _VALUE_SLAVE_IDLE, _TRUE),
        GC6M_I2CS_STATE_SECONDARY_IDLE_TIMEOUT_USEC,
        PMU_REG_POLL_MODE_CSB_EQ))
    {
        return GCX_GC6_I2C_IDLE_TIMEOUT;
    }

    return GCX_GC6_OK;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Wrapper funciton to put PMU code after PMU replies GC6 detach message to RM.
 */

static void
s_gc6PmuDetachResident(RM_PMU_GC6_DETACH_DATA *pGc6Detach)
{
    LwU32  val;
    LwU16  gc6DetachMask = pGc6Detach->gc6DetachMask;

    // Suspend DMA by ignoring DMA lock.  Assert if DMA suspension failed.
    (void) lwosDmaSuspend(LW_TRUE);

    // WAR to disable eDP HPD interrupt, must be called before clearing SCI interrupt
    pgGc6DisableHPDInterruptWAR1871547_HAL(&Pg, pGc6Detach->internalPanelHPDPort);

    //
    // If the vbios supports feature control, we need to set up the SCI vectors
    // Lwrrently it's default enabled in Turing and later
    // RTD3 12V IDLE_IN_SW is taken cared here
    // SCI interrupt and GC6 abort will also be disabled in this function
    //
    pgGc6IslandConfig_HAL(&Pg, pGc6Detach);

    //
    // GC6 entry steps:
    // 1. We can poll on the link status to go to disabled.
    // 1.5. Poll for i2c secondary to be idle for a max of 2 msec
    // 2. Save off I2CS state in BSI ram.
    // 3. Program SCI with latest I2C state.
    // 4. Turn off therm sensor.
    // 5. Do SCI/BSI handover.
    // 6. TODO: Potentially, we can grab a PC store it in BSI ram.
    // We can return to life at that PC by BSI if we hit abort.
    // followed by sending an event back to RM to reattach pmu. RM can then do
    // stateLoad without having to go through devinit.
    //

    // start the logging process
    pgGc6InitErrorStatusLogger_HAL();

    //
    // The start point from RTD3 D3Hot to RTD3 D3 Cold starts with PME message handshake.
    // Be aware that D3Hot along is a valid state so this PME_Turn_Off
    // message may never come if not entering D3Cold.
    // In short, PME msg isthe point of no return for D3Cold.
    // Once the PME_TO handshake is done, we can poll link status and proceed.
    //

    // Wait for link to switch to L1/L2/LinkDisable/D3Hot
    val = pgGc6WaitForLinkState_HAL(&Pg, gc6DetachMask);
    // Parse link state
    switch (val)
    {
        // Link in L1 (FGC6)
        case LINK_IN_L1:
        {
            goto SCI_ENTRY;
        }
        // D3 hot cycle
        case LINK_D3_HOT_CYCLE:
        {
            goto D3HOT_ENTRY;
        }
        // Error case. Should never reach here
        case LINK_ILWALID:
        {
            PMU_HALT();
        }
        // Fall through case: Link in L2 (RTD3) or LinkDisable(GC6)
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_GC6_WAR_BUG_1279179))
    {
        pgGc6ClkReqEnWAR1279179_HAL();
    }

    //
    // SW WAR to prevent LTSSM to goto Polling state after it is Disabled
    // and GPU is not Powered Down within 2ms
    //
    // Reference Bug 1268395
    // During GC6 entry PMU polls for link is disabled, Once link is
    // disabled PMU saves I2CS state and checks for Pending I2CS
    // transactions before triggering SCI to enter GC6. Upon which SCI
    // Powers down the GPU. if the delay between PMU finding Link is
    // Disabled and SCI powering Down GPU is >2ms, then LTSSM goes to
    // Polling State from Disabled, which is not needed as link at Upstream
    // port/Root Port is already disabled.
    //
    // WAR is SW to set the host2xp_hold_ltssm signal high, so LTSSM is
    // prevented to goto Polling State once it is disabled. This hold off
    // will be released as part of GC6 exit phase in BSI. Typically devinit
    // will release the holdoff at GC6 exit.
    //
    pgGc6LtssmHoldOffEnable_HAL();

SCI_ENTRY:

    //
    // List of actions needs to check after L2 and before FB self refresh,
    // 1. Turn off duel fan here
    // 2. Log if lwrent mode is not supported
    // 3. Log if SCI/BSI island trigger PLM is not L2 writable
    //
    rtd3Gc6EntryActionAfterLinkL2(pGc6Detach);

    if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                     _FAST_GC6_ENTRY, _TRUE, gc6DetachMask))
    {
        pgFgc6PreEntrySeq_HAL();
    }


    if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                     _EXIT_BOOT_VOLTAGE, _ENABLE, gc6DetachMask))
    {
        pgGc6RestoreBootVoltage_HAL(&Pg, pGc6Detach->pwmVidGpioPin);
    }

    if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                     _USE_HW_TIMER, _TRUE, gc6DetachMask))
    {
        pgIslandProgramSciWakeupTimerSet_HAL(&Pg, pGc6Detach->timeToWakeUs);
        //
        // Assert WAKE# pin if timer is set for board verification
        // This is not production path as wake timer won't be set
        //
        pgGc6AssertWakePin_HAL(&Pg, LW_TRUE);
    }

    // The FB related error will be logged inside each subfunction
    pgGc6PrepareFbForGc6_HAL(&Pg, pGc6Detach->fbRamType);

    OS_PTIMER_SPIN_WAIT_NS(1000);

    //
    // The steps after the poll are timing sensitive
    // and need to be done asap once the poll completes.
    // For a I2C freq at 100Khz; the next transaction can
    // start as soon as 80usec. So the triger needs to be
    // done before that.
    // For I2C freq at 400Khz; this latency becomes 20 usec
    // Lets enter critical section to remove distractions.
    //
    appTaskCriticalEnter();

    // Synchronise SCI GPIOs with PMGR GPIOs before SCI island handover
    if (PMUCFG_FEATURE_ENABLED(PMU_PGISLAND_SCI_PMGR_GPIO_SYNC))
    {
        pgIslandSciPmgrGpioSync_HAL(&Pg, Pg.gpioPinMask);
    }

    // STEP 1.5 Poll for I2C Secondary Idle
    pgGc6LogErrorStatus(pgGc6PollForI2cIdle_HAL(&Pg));

    // step 2. Save I2CS state into BSI RAM
    pgCreateBsiThermState_HAL(&Pg,
                              pGc6Detach->bsiOffsetThermState,
                              pGc6Detach->bsiPrivsThermState);
    //
    // Also save off BSI_CTRL register as these are not allowed
    // to be written by RM due to priv security.
    //
    pgGc6BsiStateSave_HAL(&Pmu);

    //
    // step 3. SCI I2C
    // TODO check for I2Cs idle
    // Check for fake therm sensor support, where we copy therm I2CS address
    // to the SCI debug secondary to prevent wakeups from therm sensor polling
    //
    pgGc6I2CStateUpdate_HAL(&Pg, FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                                              _FAKE_I2C, _ENABLE, gc6DetachMask));

    if (PMUCFG_FEATURE_ENABLED(PMU_GC6_WAR_BUG_1255637))
    {
        pgIlwalidateThermI2csState_HAL();
    }

    //
    // step 4. Trigger BSI to enter GC6
    //
    // NOTE: We dont do the copy+clear here to make sure SCI can still do exits
    // for events that have happened while the pmu was busy doing gc6 entry.
    // The intr status was cleaned some time back by the rm before doing link disable.
    // So we are essentially aclwmulating all wakeup interrupts untill SCI is in charge.
    //

    //
    // Run LWLINK entry sequence if lwlink is enabled and is a RTD3 entry
    // Moving LWLINK entry sequence after FB SR and I2C poll due to Bug219744
    // //
    if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                     _RTD3_GC6_ENTRY, _TRUE, gc6DetachMask) &&
        pGc6Detach->bLwlinkEnabled)
    {
        pgLwlinkBsiPatchExitSeq_HAL(&Pg, LW_PGC6_BSI_PHASE_OFFSET + LW_PGC6_BSI_PHASE_SETUP);
        pgGc6LwlinkL2Entry_HAL();
    }

    // update the error status to scratch
    pgGc6UpdateErrorStatusScratch_HAL(&Pg);

    // trigger the writes close-by
    pgGc6IslandTrigger_HAL(&Pg, FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                                             _RTD3_GC6_ENTRY, _TRUE, gc6DetachMask),
                                FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                                            _FAST_GC6_ENTRY, _TRUE, gc6DetachMask));
    //
    // The Gpu would reset at this point.
    // appTaskCriticalExit();
    // Adding this comment for symmetry.
    //

D3HOT_ENTRY:
#if UPROC_RISCV
    // Shutdown PMU - No return expected
    sysShutdown();
#else
    PMU_HALT();
#endif
}

