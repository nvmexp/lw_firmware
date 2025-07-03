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
 * @file   pmu_pgislandgp10x.c
 * @brief  PMU PGISLAND related Hal functions for GP10X.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwosreg.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "pmu_objpsi.h"
#include "pmu_objdi.h"
#include "pmu_objfb.h"
#include "pmu_gc6.h"
#include "dev_gc6_island.h"
#include "dev_pwr_pri.h"
#include "dev_trim.h"
#include "dev_therm.h"
#include "dev_pwr_csb.h"
#include "volt/objvolt.h"
#include "lib/lib_pwm.h"
#include "dbgprintf.h"
#include "rmlsfm.h"

#include "config/g_pg_private.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

/* ------------------------- Type Definitions ------------------------------- */
#ifndef LW_CPWR_THERM
#define LW_CPWR_THERM 0x17FF:0x1000
// sanitize the define
ct_assert(LW_CPWR_THERM_ARP_STATUS - DRF_BASE(LW_CPWR_THERM) ==
          LW_THERM_ARP_STATUS - DRF_BASE(LW_THERM));
#endif

// Size of BSI descriptor in dwords
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE              5

// Sections of each dword in the descriptor
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_IMEM0      0
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_IMEM1      1
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_DMEM0      2
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_DMEM1      3
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_PRIV       4

#define LW_PGC6_BAR0_OP_SIZE_DWORD 3
#define LW_PGC6_PRIV_OP_SIZE_DWORD 2

#define LW_PGC6_BSI_PHASE_OFFSET                         1
#define LW_PGC6_BSI_PHASE_SCRIPT                         5

// BSIRAM phase alignment size
#define BSI_RAM_PHASE_ALIGNMENT_SIZE_BYTES             0x4

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Update PSI for SCI
 *
 * @param[in]   sciMask         Mask of PSI_SCI_STEP*
 */
void
pgIslandSciDiPsiSeqUpdate_GP10X
(
    LwU32 sciStepMask
)
{
    LwU32 oldOverrideReg;
    LwU32 newOverrideReg;
    LwU32 oldVoltRampDownTime;
    LwU32 newVoltRampDownTime;
    LwU32 oldPsiSettleTime;
    LwU32 newPsiSettleTime;

    oldOverrideReg = newOverrideReg =
        REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_OVERRIDE_SEQ_STATE);

    //
    // Cache state of VR_SETTLEMENT_DELAY_TABLE
    // *_PSI_VR_SETTLEMENT indexes to delay table which provides necessary
    // delay after exiting PSI. This should be set to voltage regulator
    // settlement time if PSI engages. It should be 0 if PSI is disabled
    //
    oldPsiSettleTime = newPsiSettleTime = REG_RD32(FECS,
                       LW_PGC6_SCI_PWR_SEQ_DELAY_TABLE(
                       LW_PGC6_SCI_DELAY_TABLE_IDX_PSI_VR_SETTLEMENT));

    //
    // Cache state of VOLTAGE_SETTLEMENT_RAMP_DOWN_DELAY_TABLE
    // *VOLTAGE_SETTLEMENT_RAMP_DOWN indexes to delay table which provides
    // necessary delay before entering PSI for voltage to ramp down correctly.
    // it should be 0 if PSI is disabled
    //
    oldVoltRampDownTime = newVoltRampDownTime =  REG_RD32(FECS,
                          LW_PGC6_SCI_PWR_SEQ_DELAY_TABLE(
                          LW_PGC6_SCI_DELAY_TABLE_IDX_VOLTAGE_SETTLEMENT_RAMP_DOWN));

    //
    // Do not change PSI sequence for SCI. This will ensure default sequence
    // for PSI with SCI i.e no SW overrides. PSI will be engaged by SCI in
    // powerdown and disengaged in powerup
    //
    if (PSI_SCI_STEP_ENABLED(sciStepMask, INSTR))
    {
        //
        // If PSI is enabled, SCI script should engage PSI. Switch SW override
        // to _INSTR so that default sequence is considered.
        //
        newOverrideReg = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE,
                            _PSI_EN, _INSTR, newOverrideReg);

        //
        // Modify SCI delay table to update Voltage settlement time
        // When SCI changes voltage, PSI should wait till voltage becomes
        // steady before changing phases
        //
        if (PG_IS_SF_ENABLED(Di, DI, VOLTAGE_UPDATE))
        {
            newVoltRampDownTime = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                                  _PERIOD, DI_SEQ_VOLT_SETTLE_TIME_US,
                                  newVoltRampDownTime);
            newVoltRampDownTime = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                                  _SCALE, _1US, newVoltRampDownTime);
        }
        else
        {
            newVoltRampDownTime = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                                              _PERIOD, _NONE, newVoltRampDownTime);
        }

        //
        // Modify SCI delay table to update Voltage Regulator settlement time
        // When disabling PSI (legacy or auto):  SW needs to wait for 15us before
        // increasing GPU load. This is to guarantee VR is already
        // in multi-phase mode before increasing load.
        //
        newPsiSettleTime = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                           _PERIOD, psiVRSettleTimeGet(), newPsiSettleTime);
        newPsiSettleTime = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE, _SCALE,
                           _1US, newPsiSettleTime);
    }
    else if (PSI_SCI_STEP_ENABLED(sciStepMask, ENABLE))
    {
        //
        // If PSI is engaged, SCI script should engage PSI. and never disengage
        // it Switch SW override _ASSERT so that default sequence changed to
        // always engaged.
        //
        newOverrideReg      = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE,
                              _PSI_EN, _ASSERT, newOverrideReg);

        // No need to wait for voltage to ramp down
        newVoltRampDownTime = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                              _PERIOD, _NONE, newVoltRampDownTime);

        //
        // No need to wait for VR to settle down since we are not changing
        // state of PSI
        newPsiSettleTime    = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                              _PERIOD, _NONE, newPsiSettleTime);
    }

    else if (PSI_SCI_STEP_ENABLED(sciStepMask, DISABLE))
    {
        //
        // If PSI is disabled SCI script should disable PSI. Change
        // SW override _DEASSERT so that default sequence is changed to
        // always disengaged.
        //
        newOverrideReg      = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE,
                              _PSI_EN, _DEASSERT, newOverrideReg);

        // No need to wait for voltage to ramp down
        newVoltRampDownTime = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                              _PERIOD, _NONE, newVoltRampDownTime);

        //
        // No need to wait for VR to settle down since we are disabling
        // state of PSI
        newPsiSettleTime    = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_DELAY_TABLE,
                              _PERIOD, _NONE, newPsiSettleTime);
    }

    // Write back SCI overrides if anything has changed
    if (newOverrideReg != oldOverrideReg)
    {
        REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_OVERRIDE_SEQ_STATE, newOverrideReg);
    }

    if (newVoltRampDownTime != oldVoltRampDownTime)
    {
        REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_DELAY_TABLE(
        LW_PGC6_SCI_DELAY_TABLE_IDX_VOLTAGE_SETTLEMENT_RAMP_DOWN),
        newVoltRampDownTime);
    }

    if (newPsiSettleTime != oldPsiSettleTime)
    {
        REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_DELAY_TABLE(
        LW_PGC6_SCI_DELAY_TABLE_IDX_PSI_VR_SETTLEMENT), newPsiSettleTime);
    }
}

// This code does not compile on HBM-supported builds.
#if PMUCFG_FEATURE_ENABLED(PMU_FB_DDR_SUPPORTED)
/*!
 *  @brief Set any FB/regulator state before GC6 power down
 *  TODO-SC remove the if-else check and correct the function prototype
 */
GCX_GC6_ENTRY_STATUS
pgGc6PrepareFbForGc6_GP10X(LwU8 ramType)
{
    LwU32                fbioCtl;
    LwU32                fbioCtlClear;
    LwU32                val;
    LwU32                val2;

    if ((PMUCFG_FEATURE_ENABLED(PMU_GC6_FBSR_IN_PMU)) &&
        (ramType != LW2080_CTRL_FB_INFO_RAM_TYPE_UNKNOWN))
    {
        //
        // Error logging the FBSR sequence.
        // There may be multiple error been logged here and
        // plesae start with the first error seen.
        //
        pgGc6LogErrorStatus(fbFlushL2AllRamsAndCaches_HAL(&Fb));

        pgGc6LogErrorStatus(fbForceIdle_HAL(&Fb));

        pgGc6LogErrorStatus(fbEnterSrAndPowerDown_HAL(&Fb, ramType));

        pgGc6LogErrorStatus(fbPreGc6Clamp_HAL(&Fb));
    }

    // Assert FBCLAMP first on Pascal - see 1713528 #83
    val  = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE);
    val  = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_ARMED_SEQ_STATE, _FB_CLAMP,
                       _ASSERT, val);
    val2 = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE);
    val2 = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_STATE, _UPDATE, _TRIGGER, val2);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE, val);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE, val2);

    OS_PTIMER_SPIN_WAIT_NS(1000);

    //
    // For details, refer to 1664838 #93
    // Power down regulators for all FB partitions
    // including Floorswept
    //
    fbioCtl     = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_SPARE);
    fbioCtlClear = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_SPARE, _FBIO_SPARE, 0x0, fbioCtl);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, fbioCtlClear);

    return GCX_GC6_OK;
}
#endif      // PMUCFG_FEATURE_ENABLED(PMU_FB_DDR_SUPPORTED)

/*!
 * @brief Configures BSI RAM
 *
 * Writes to LW_PGC6_BSI_BOOTPHASES and LW_PGC6_BSI_CTRL as RM cannot access
 * these any more because of priv security.
 *
 * @param[in]  bsiBootPhaseInfo    Configure phases in LW_PGC6_BSI_BOOTPHASES.
 */
void
pgIslandBsiRamConfig_GP10X
(
    LwU32 bsiBootPhaseInfo
)
{
#if (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV)))
    LwU32 val = 0;

    REG_WR32(FECS, LW_PGC6_BSI_BOOTPHASES, bsiBootPhaseInfo);

    //
    // This was being set initially in pgislandBSIRamValid_HAL,
    // but after GM20x for priv security, only PMU can write to
    // LW_PGC6_BSI_CTRL.
    //
    val = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_CTRL, _RAMVALID, _TRUE, val);
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, val);

    // Enable the BAR0 OPCODE and PRIV LEVEL TRANSFER features as well
    val = REG_RD32(FECS, LW_PGC6_BSI_PWRCLK_DOMAIN_FEATURES);
    val = FLD_SET_DRF(_PGC6, _BSI_PWRCLK_DOMAIN_FEATURES, _BAR0_OPCODE, _ENABLE, val);
    val = FLD_SET_DRF(_PGC6, _BSI_PWRCLK_DOMAIN_FEATURES, _BSI_RAM_OPCODE, _ENABLE, val);
    val = FLD_SET_DRF(_PGC6, _BSI_PWRCLK_DOMAIN_FEATURES, _PRIV_LEVEL_TRANSFER, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_PWRCLK_DOMAIN_FEATURES, val);
#endif // (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV)))
}

/*!
 * @brief Reads the BSI ram from provided offset and
 * creates a save state for privs defined in the ram.
 *
 * @param[in]  ramOffset   Offset to the save state in ram
 * @param[in]  nPrivs      Num of privs (addr-data) pairs in the state
 */
void
pgCreateBsiThermState_GP10X
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
            // Clear OPCODE field
            payload &= ~(BIT(0)|BIT(1));

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

        // dummy increment to deal with the CTRL dword
        payload = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
    }
}

/*!
 * @brief : Update Sleep voltage device
 *
 * @param[in]   railIdx Index of rail for which
 *              device voltage needs to be updated
 */
void
pgIslandSleepVoltageUpdate_GP10X
(
    LwU32   railIdx
)
{
    VOLT_DEVICE    *pDevice;
    FLCN_STATUS     status;

    if (PG_VOLT_IS_RAIL_SUPPORTED(railIdx))
    {
        pDevice = VOLT_DEVICE_GET(PG_VOLT_GET_SLEEP_VOLTDEV_IDX(railIdx));
        if (pDevice == NULL)
        {
            PMU_BREAKPOINT();
            goto pgIslandSleepVoltageUpdate_GP10X_exit;
        }

        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libVolt));

        status =  voltDeviceSetVoltage(pDevice, PG_VOLT_GET_SLEEP_VOLTAGE_UV(railIdx),
                          LW_FALSE, LW_FALSE, LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH);
        if (status != FLCN_OK)
        {
            // Voltage has not been set correctly. Halt the PMU
            PMU_HALT();
        }

        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libVolt));
    }

pgIslandSleepVoltageUpdate_GP10X_exit:
    lwosNOP();
}

/*!
 * @brief Check the data that is being requested to copy into BSI RAM
 *
 * This function checks the copy request and data destined for the BSI RAM
 * and is to be used for security reasons when the data is compiled by RM.
 */
FLCN_STATUS
pgIslandBsiRamCopyCheck_GP10X
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
    LwU32 maxSize;
    LwU32 bsiRamSize;
    LwU32 brssHeaderSz   = sizeof(BSI_RAM_SELWRE_SCRATCH_HDR_V2);
    LwU32 brssHeaderSzDw = brssHeaderSz / sizeof(LwU32);
    LwU32 scriptPhase    = LW_PGC6_BSI_PHASE_OFFSET + LW_PGC6_BSI_PHASE_SCRIPT;
    BSI_RAM_SELWRE_SCRATCH_HDR_V2 brssHeader;
    FLCN_STATUS status;

    // first check if we have UDE
    if (REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP, bsiBootPhaseInfo) < scriptPhase)
    {
        // this is pre-silicon testing only so it is valid
        return FLCN_OK;
    }

    bsiRamSize = pgIslandGetBsiRamSize_HAL(&Pg);
    maxSize = bsiRamSize;
    status = pgIslandBsiRamRW_HAL(&Pg, (LwU32*)&brssHeader, brssHeaderSzDw, bsiRamSize - brssHeaderSz, LW_TRUE);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // If the last dword matches the identifier, then we need to adjust
    // account for the space.
    //
    if (brssHeader.identifier == LW_BRSS_IDENTIFIER)
    {
        // If V2, the compacted and rsvd size is present so use that for size check
        if (brssHeader.version == LW_BRSS_VERSION_V2)
        {
            maxSize = bsiRamSize -
                      brssHeader.compactedDevinitScriptSize -
                      brssHeader.hubEncryptionStructSize -
                      sizeof(BSI_RAM_SELWRE_SCRATCH_V2);
        }
        else
        {
            // Best guess : offset BRSS and HUB reserved space to ensure they do not get clobbered.
            maxSize = bsiRamSize -
                      sizeof(BSI_RAM_SELWRE_SCRATCH_V1) -
                      sizeof(ACR_BSI_HUB_DESC_ARRAY);
        }
    }

    // Make sure that the size does not overrun
    if ((size == 0) || (size > maxSize))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Make sure that the size is aligned
    if (!LW_IS_ALIGNED(size, DMA_MIN_READ_ALIGNMENT))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Check the phase pointers
    gc5Start =  REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC5START, bsiBootPhaseInfo);
    gc5Stop  =  REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC5STOP,  bsiBootPhaseInfo);
    gc6Start =  REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6START, bsiBootPhaseInfo);
    gc6Stop  =  REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP,  bsiBootPhaseInfo);

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
 * @brief Check the data that is being requested to copy into BSI RAM
 *
 * This function checks the copy request and data destined for the BSI RAM
 * and is to be used for security reasons when the data is compiled by RM.
 */
FLCN_STATUS
pgIslandSanitizeBsiRam_GP10X
(
    LwU32 bsiBootPhaseInfo
)
{
#if (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV)))
    LwU32 val;
    LwU32 i;
    LwU32 numPhases;
    LwU32 numDwords;
    LwU32 srcOffset;
    LwU32 count;
    LwU32 ramCtrl;
    LwU32 descOffset;
    LwU32 bsiPadCtrl = LW_PGC6_BSI_PADCTRL | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0;

    // The bar0ctl value we are using to override to prevent RM from sending level 2 requests
    LwU32 bar0Ctl =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                      DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _TRIG, _TRUE) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _0 ));

    // The bar0ctl value for level 2 requests
    LwU32 bar0CtlL2 =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                       DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |
                       DRF_DEF(_PPWR_PMU, _BAR0_CTL, _TRIG, _TRUE) |
                       DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _2 ));
    //
    // Offset BRSS and HUB reserved space to ensure they do not get clobbered.
    // Todo - size check info should be taken from a secure entity
    //
    LwU32 maxSize = pgIslandGetBsiRamSize_HAL(&Pg) -
                        sizeof(BSI_RAM_SELWRE_SCRATCH_V1) -
                        sizeof(ACR_BSI_HUB_DESC_ARRAY);

    // Go through the payload and check the priv phases to ensure that we only send level 0 writes.
    // Parse the descriptors to find the priv start and number of privs

    // Last GC6 phase is always the max phase
    numPhases = REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP, bsiBootPhaseInfo) + 1;

    for (i = 0; i < numPhases; i++)
    {
        // Get the offset for the priv info of the i-th phase descriptor in the ram
        descOffset = (i * LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE) + LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_PRIV;

        // Set address in BSI RAM to read out the priv info
        ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
        ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, descOffset*sizeof(LwU32), ramCtrl);
        REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);

        // parse the number of dwords and location of actual writes in the priv section
        val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
        numDwords = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_NUMWRITES, val);
        srcOffset = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_SRCADDR, val);

        // Validate the number of dwords
        if (srcOffset + numDwords*sizeof(LwU32) > maxSize)
        {
            return FLCN_ERROR;
        }

        // Set priv section address in BSI RAM
        ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
        ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _ENABLE, ramCtrl);
        ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, ramCtrl);
        ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, srcOffset, ramCtrl);
        REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);
        count = 0;

        while (count < numDwords)
        {
            // Read the first dword in the sequence
            val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

            // Check if this is a BAR0 or priv write
            if (FLD_TEST_DRF(_PGC6, _BSI_RAMDATA, _OPCODE, _BAR0, val))
            {
                // Make sure that we have enough dwords for this op
                if ((numDwords - count) < LW_PGC6_BAR0_OP_SIZE_DWORD)
                {
                    return FLCN_ERROR;
                }

               //
               // Add a special case to reset the BSI registers if needed.
               // Todo: move this to a better spot.
               //
                if (val == bsiPadCtrl)
                {
                    // make sure this is a reset
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, 0x8);
                    // override the ctl to priv level 2
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0CtlL2);
                }
                else
                {
                    // dummy read to skip data section
                    REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
                    // override the ctl to priv level 0
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);
                }
                count += LW_PGC6_BAR0_OP_SIZE_DWORD;
            }
            else
            {
                // Make sure that we have enough dwords for this op
                if ((numDwords - count) < LW_PGC6_PRIV_OP_SIZE_DWORD)
                {
                    return FLCN_ERROR;
                }

                // Check if we are writing to BAR0_CTL
                if (val == LW_PPWR_PMU_BAR0_CTL)
                {
                    // force priv level 0 here too
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);
                }
                else
                {
                    // dummy read to skip data section
                    REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
                }

                count += LW_PGC6_PRIV_OP_SIZE_DWORD;
            }
        }
    }

#endif // (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV)))
    return FLCN_OK;
}

/*!
 * @brief Check and patch descriptors if BRSS is present
 */
void
pgIslandPatchForBrss_GP10X
(
    LwU32 bsiBootPhaseInfo
)
{
    LwU32               val         = 0;
    LwU32               addr        = 0;
    LwU32               dmemSrc     = 0;
    LwU32               bsiRamSize  = 0;
    LwU32               scriptPhase = LW_PGC6_BSI_PHASE_OFFSET + LW_PGC6_BSI_PHASE_SCRIPT;
    LwU32               alignedSize = 0;
    LwU32 brssHeaderSz              = sizeof(BSI_RAM_SELWRE_SCRATCH_HDR_V2);
    LwU32 brssHeaderSzDw            = brssHeaderSz / sizeof(LwU32);
    BSI_RAM_SELWRE_SCRATCH_HDR_V2 brssHeader;

    // first check if we have UDE
    if (REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP, bsiBootPhaseInfo) < scriptPhase)
    {
        return;
    }

    bsiRamSize = pgIslandGetBsiRamSize_HAL(&Pg);

    // Read the BRSS Header structure (no need to read the whole struct)
    (void)pgIslandBsiRamRW_HAL(&Pg, (LwU32*)&brssHeader, brssHeaderSzDw, bsiRamSize - brssHeaderSz, LW_TRUE);

    //
    // If the last dword matches the identifier, then we need to adjust
    // the devinit script phase descriptor because compaction size does not
    // account for the space.
    //
    if (brssHeader.identifier == LW_BRSS_IDENTIFIER)
    {
        //
        // Each descriptor is 5 dwords. We want to adjust the dmem src address,
        // which is 15:0 in the third dword of the descriptor.
        // See manuals for more info on descriptor layout.
        //
        addr = ((scriptPhase * LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE) + LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_DMEM0) * sizeof(LwU32);
        (void)pgIslandBsiRamRW_HAL(&Pg, &val, 1 , addr, LW_TRUE); // read one dword

        //
        // If V2, the compacted and rsvd size is present, so use that to adjust src addr
        // Also check if compacted devinit size is 0 and bootphase register is already set
        // to take care of bug 1811575- horrible hack but unavoidable without vbios fix.
        //
        if (brssHeader.version == LW_BRSS_VERSION_V2 &&
            brssHeader.compactedDevinitScriptSize != 0 &&
            REG_RD32(FECS, LW_PGC6_BSI_BOOTPHASES) == 0)
        {
            alignedSize = (brssHeader.compactedDevinitScriptSize +
                           (BSI_RAM_PHASE_ALIGNMENT_SIZE_BYTES-1)) &
                          ~(BSI_RAM_PHASE_ALIGNMENT_SIZE_BYTES-1);

            dmemSrc = bsiRamSize -
                      alignedSize -
                      brssHeader.hubEncryptionStructSize -
                      sizeof(BSI_RAM_SELWRE_SCRATCH_V2);
        }
        else
        {
            // Parse the value for the src address for compacted devinit
            dmemSrc = DRF_VAL(_PGC6, _BSI_RAMDATA, _DMEM0_SRCADDR, val);

            //
            // Subtract the size of the structure (because the ucode grows up)
            //
            dmemSrc -= sizeof(BSI_RAM_SELWRE_SCRATCH_V1);
        }

        val = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMDATA, _DMEM0_SRCADDR, dmemSrc, val);

        (void)pgIslandBsiRamRW_HAL(&Pg, &val, 1, addr, LW_FALSE);
    }
}

/*!
 * @brief Constructs PWM Source Descriptor for SCI PWM's devices
 *
 * @description : This function constructs the PWM source descriptor for SCI
 *                VID PWM's source.
 *                Source descriptor is used by VOLT to set voltage for the device.
 *
 * @note    This function assumes that the overlay described by @ref ovlIdxDmem
 *          is already attached by the caller.
 *
 * @param[in] pwmSource   Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 * @param[in] ovlIdxDmem  DMEM Overlay index in which to construct the PWM
 *                        source descriptor
 *
 * @return  PWM_SOURCE_DESCRIPTOR   Pointer to PWM_SOURCE_DESCRIPTOR structure.
 * @return  NULL                    In case of invalid PWM source.
 */
PWM_SOURCE_DESCRIPTOR *
pgIslandPwmSourceDescriptorConstruct_GP10X
(
    RM_PMU_PMGR_PWM_SOURCE pwmSource,
    LwU8                   ovlIdxDmem
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = NULL;
    PWM_SOURCE_DESCRIPTOR  pwmSrcDesc;

    switch (pwmSource)
    {
        case RM_PMU_PMGR_PWM_SOURCE_SCI_VID_PWM_0:
        {
            pwmSrcDesc.addrPeriod    = LW_PGC6_SCI_VID_CFG0(PG_ISLAND_SCI_RAIL_IDX_LWVDDL);
            pwmSrcDesc.addrDutycycle = LW_PGC6_SCI_VID_CFG1(PG_ISLAND_SCI_RAIL_IDX_LWVDDL);
            // Trigger for Steady State Devices (Previous update should be cancelled)
            pwmSrcDesc.bTrigger      = LW_TRUE;
            pwmSrcDesc.bCancel       = LW_TRUE;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_SCI_VID_PWM_1:
        {
            pwmSrcDesc.addrPeriod    = LW_PGC6_SCI_VID_CFG0(PG_ISLAND_SCI_RAIL_IDX_LWVDDS);
            pwmSrcDesc.addrDutycycle = LW_PGC6_SCI_VID_CFG1(PG_ISLAND_SCI_RAIL_IDX_LWVDDS);
            // Trigger for Steady State Devices (Previous update should be cancelled)
            pwmSrcDesc.bTrigger      = LW_TRUE;
            pwmSrcDesc.bCancel       = LW_TRUE;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_SCI_VID_PWM_SLEEP_STATE_0:
        {
            pwmSrcDesc.addrPeriod    = LW_PGC6_SCI_VID_CFG0(PG_ISLAND_SCI_RAIL_IDX_LWVDDL);
            pwmSrcDesc.addrDutycycle = LW_PGC6_SCI_PWR_SEQ_VID_TABLE(LW_PGC6_SCI_VID_TABLE_IDX_GC5_LWVDDL_SLEEP);
            // Do not trigger for Sleep Devices (Cancellation of previous request not required)
            pwmSrcDesc.bTrigger      = LW_FALSE;
            pwmSrcDesc.bCancel       = LW_FALSE;
            break;
        }
        case RM_PMU_PMGR_PWM_SOURCE_SCI_VID_PWM_SLEEP_STATE_1:
        {
            pwmSrcDesc.addrPeriod    = LW_PGC6_SCI_VID_CFG0(PG_ISLAND_SCI_RAIL_IDX_LWVDDS);
            pwmSrcDesc.addrDutycycle = LW_PGC6_SCI_PWR_SEQ_VID_TABLE(LW_PGC6_SCI_VID_TABLE_IDX_GC5_LWVDDS_SLEEP);
            // Do not trigger for Sleep Devices (Cancellation of previous request not required)
            pwmSrcDesc.bTrigger      = LW_FALSE;
            pwmSrcDesc.bCancel       = LW_FALSE;
            break;
        }
        default:
        {
            goto pgIslandPwmSourceDescriptorConstruct_GP10X_exit;
        }
    }
    pwmSrcDesc.type       = PWM_SOURCE_DESCRIPTOR_TYPE_DEFAULT;
    pwmSrcDesc.mask       = DRF_MASK(LW_PGC6_SCI_VID_CFG0_PERIOD);
    pwmSrcDesc.triggerIdx = DRF_BASE(LW_PGC6_SCI_VID_CFG1_UPDATE);
    pwmSrcDesc.doneIdx    = DRF_BASE(LW_PGC6_SCI_VID_CFG1_UPDATE);
    pwmSrcDesc.bus        = REG_BUS_FECS;

    pPwmSrcDesc = pwmSourceDescriptorAllocate(ovlIdxDmem, pwmSource, &pwmSrcDesc);

pgIslandPwmSourceDescriptorConstruct_GP10X_exit:
    return pPwmSrcDesc;
}
