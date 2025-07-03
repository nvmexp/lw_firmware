/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_msswasrgk10x.c
 * @brief  SW-ASR specific HAL-interface for the GK10X
 */

/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_fb.h"
#include "dev_fbpa.h"
#include "dev_fbpa_addendum.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"
#include "dev_pri_ringstation_fbp.h"
#include "hwproject.h"
#include "pmu/pmumailboxid.h"

/* ------------------------- Application Includes --------------------------- */

#include "pmu_objms.h"
#include "pmu_swasr.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "dbgprintf.h"
#include "config/g_ms_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* --------------------------- Prototypes ------------------------------------*/

static void s_msSelfRefreshEnter_GMXXX(LwU8 ctrlId);
static void s_msSelfRefreshExit_GMXXX(LwU8 ctrlId);
static void s_msMainRegulatorDisable_GMXXX(void);
static void s_msMainRegulatorEnable_GMXXX(void);
static void s_msSwAsrIssueFbPrivFlush_GMXXX(void);
static void s_msPeriodicTrainingDisable_GMXXX(void);

/* ------------------------ Public Functions  ------------------------------- */

/*!
 * @brief Exelwtes partial/complete SW-ASR Entry Sequence
 *
 * SW-ASR Entry sequence -
 * 1) Stop FB and put DRAM into Self Refresh
 * 2) Power down FB Pads
 * 3) Power down DRAM/REFM PLL
 * 4) Gate DRAM clock
 * 5) Disable Main Regulator.
 */
void
msSwAsrEntry_GMXXX
(
    LwU8 ctrlId
)
{
    // Put DRAM into self refresh
    s_msSelfRefreshEnter_GMXXX(ctrlId);

    // START of disable Regulator Sequence

    // Power down components in FB Pads
    msFbPadPwrDown_HAL(&Ms);

    // PMU will flush the PRIV bus by issuing a FECS PRI_FENCE reads
    s_msSwAsrIssueFbPrivFlush_GMXXX();

    // Gate DRAM clock
    msSwAsrDramClockGate_HAL(&Ms);

    //
    // Before Disabling the Regulator, powering down COMP pads
    // offers power benefits. Reference Bug 1345947
    //
    msSwAsrCompPadPwrDown_HAL();

    // Disable Main Regulator
    s_msMainRegulatorDisable_GMXXX();

    // END of disable Regulator Sequence

    msHostToFecsFenceInsert_HAL(&Ms);
}

/*!
 * @brief Exelwtes partial/complete SW-ASR Exit Sequence
 *
 * SW-ASR Exit sequence -
 * 1) Enable Main Regulator.
 * 2) Power-up DRAM/REFM PLL
 * 3) Ungate DRAM clock
 * 4) Power-up FB Pads
 * 5) Take out DRAM from Self Refresh and release FB Stop
 */
void
msSwAsrExit_GMXXX
(
    LwU8 ctrlId
)
{
    // START of enable regulator sequence
    s_msMainRegulatorEnable_GMXXX();

    msSwAsrDramClockUngate_HAL(&Ms);

    //
    // Add a 1 us delay to let new clocks reach FBIO and let the PRIV path
    // settle. Bug 1356383. This is only required when GC5 powers up/down the
    // PLLs and MSCG gates/ungates the DRAM clock. However, keeping this here
    // in order to keep the common SW-ASR code independent of GC5.
    //
    OS_PTIMER_SPIN_WAIT_NS(1000);

    msFbPadPwrUp_HAL(&Ms);
    // END of enable regulator sequence

    // Take-out DRAM from self refresh
    s_msSelfRefreshExit_GMXXX(ctrlId);
}

/*!
 * @brief ACPD action for MS
 *
 * Disable ACPD in SW-ASR entry
 * Restore ACPD settings in SW-ASR exit
 *
 * @param[in]    bEntering   LW_TRUE if we are entering into SW-ASR
 *                           LW_FALSE if we are exiting from SW-ASR
 */
void
msAcpdAction_GMXXX(LwBool bEntering)
{
    LwU32     fbpa   = 0;
    LwU32     val    = 0;
    LwU32     addr;
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    // Cache ACPD settings before starting SW-ASR sequence
    if (bEntering)
    {
        pSwAsr->regs.fbpaCfg0 = REG_RD32(FECS, LW_PFB_FBPA_CFG0);
    }

    if (FLD_TEST_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD,
                     _ACTIVE_POWERDOWN, pSwAsr->regs.fbpaCfg0))
    {
        //
        // Mixed-Mode memory configuration -
        // Enable/Disable ACPD using unicast read-modify-write.
        //
        if (pSwAsr->bMixedMemDensity)
        {
            FOR_EACH_INDEX_IN_MASK(32, fbpa, pSwAsr->regs.mmActiveFBIOs)
            {
                addr = LW_PFB_FBPA_0_CFG0 + (fbpa * LW_FBPA_PRI_STRIDE);
                val = REG_RD32(FECS, addr);

                if (bEntering)
                {
                    val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD,
                                      _NO_POWERDOWN, val);
                }
                else
                {
                    val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD,
                                      _ACTIVE_POWERDOWN, val);
                }

                REG_WR32(FECS, addr, val);
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
        //
        // Uniform(Non Mix-Mode) memory configuration -
        // Enable/Disable ACPD using broadcast read-modify-write.
        //
        else
        {
            if (bEntering)
            {
                val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD,
                                  _NO_POWERDOWN, pSwAsr->regs.fbpaCfg0);
                REG_WR32(FECS, LW_PFB_FBPA_CFG0, val);
            }
            else
            {
                REG_WR32(FECS, LW_PFB_FBPA_CFG0, pSwAsr->regs.fbpaCfg0);
            }
        }
    }
}

/*!
 * @brief Enable Periodic Training
 *
 * We do Periodic Training for GDDR5 when MCLK > 405MHz (in P5 and above
 * PState). If we are doing SW-ASR for MCLK greater than 405MHz then disable
 * Periodic Training before putting DRAM into Self Refresh.
 */
void
msPeriodicTrainingEnable_GMXXX(void)
{
    LwU32 val = 0;

    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _DISABLED, 0);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _QUSE, _DISABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _QUSE_PRIME, _DISABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _ENABLED, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _DLL, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _STEP, _INIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _INIT, val);


    // Write the training register with considering HALF FBPA mode
    msFbTrainingCommandUpdate_HAL(&Ms, val);
}

/*!
 * @brief update the training register
 */
void
msFbTrainingCommandUpdate_GMXXX
(
    LwU32 trainingCmd
)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     i;
    LwU32     fbpaIdx;
    LwU32     addr;

    // If Half-FBPA is enabled on any of the FBPAs
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_HALF_FBPA) &&
        (pSwAsr->halfFbpaMask != 0))
    {
        FOR_EACH_INDEX_IN_MASK(32, fbpaIdx, pSwAsr->regs.mmActiveFBIOs)
        {
            if ((pSwAsr->halfFbpaMask & BIT(fbpaIdx)) != 0)
            {
                // Update only sub-partition 1 if Half-FBPA is enabled on this FBPA
                addr = LW_PFB_FBPA_0_TRAINING_CMD(1) + (fbpaIdx * LW_FBPA_PRI_STRIDE);

                REG_WR32(FECS, addr, trainingCmd);
            }
            else
            {
                for (i = 0; i < LW_PFB_FBPA_TRAINING_CMD__SIZE_1; i++)
                {
                    // Update both sub-partitions if Half-FBPA is not enabled on this FBPA
                    addr = LW_PFB_FBPA_0_TRAINING_CMD(i) + (fbpaIdx * LW_FBPA_PRI_STRIDE);

                    REG_WR32(FECS, addr, trainingCmd);
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    else
    {
        for (i = 0; i < LW_PFB_FBPA_TRAINING_CMD__SIZE_1; i++)
        {
            REG_WR32(FECS, LW_PFB_FBPA_TRAINING_CMD(i), trainingCmd);
        }
    }
}

/*!
 * @brief: Put the Dram in Self Refresh mode
 */
void
msSwAsrIssueEntryCommand_GMXXX(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;

    val = REG_RD32(FECS, LW_PFB_FBPA_TESTCMD);

    // Prepare the Self refresh Entry 1 comand
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1,  _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,   _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY1, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY1, val);

    // If we do not have GDDRX type of Ram, override the CKE BIT
    if (!pSwAsr->bIsGDDRx)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,
                          _LEGACY_SELF_REF_ENTRY1, val);
    }

    // Check if we need to set _ADR Bit
    if (pSwAsr->bIsGDDR3MirrorCmdMapping)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR,
                _LEGACY_MIRR_SELF_REF_ENTRY1, val);
    }

    // Issue the Self refresh entry command 1
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, val);


    // Prepare the Self refresh Entry 2 comand
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1,  _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,   _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,   _DDR5_SELF_REF_ENTRY2, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,  _DDR5_SELF_REF_ENTRY2, val);

    // If we do not have GDDRX type of Ram, override the CKE BIT
    if (!pSwAsr->bIsGDDRx)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,
                          _LEGACY_SELF_REF_ENTRY2, val);
    }

    // Check if we need to set _ADR Bit
    if (pSwAsr->bIsGDDR3MirrorCmdMapping)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR,
                          _LEGACY_MIRR_SELF_REF_ENTRY2, val);
    }

   // Issue the Self refresh entry command 2
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, val);
}

/*!
 * @brief: Prepare the Self Refresh entry/exit commands
 */
void
msSwAsrIssueExitCommand_GMXXX(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     val;

    val = REG_RD32(FECS, LW_PFB_FBPA_TESTCMD);

    // Prepare the Self refresh EXIT command
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,  _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,  _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,  _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0,  _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1,  _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,   _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD, _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,   _DDR5_SELF_REF_EXIT, val);
    val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,  _DDR5_SELF_REF_EXIT, val);

    // If we do not have GDDRX type of Ram, override the CKE BIT
    if (!pSwAsr->bIsGDDRx)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,
                          _LEGACY_SELF_REF_EXIT, val);
    }

    // Check if we need to set _ADR Bit
    if (pSwAsr->bIsGDDR3MirrorCmdMapping)
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR,
                          _LEGACY_MIRR_SELF_REF_EXIT, val);
    }

    // Issue the Self refresh exit command
    REG_WR32(FECS, LW_PFB_FBPA_TESTCMD, val);
}

/*!
 * @brief function to initiate the FB Training
 *
 * This function initiate the FB Training and wait for
 * the training to get finished
 */
void
msFbTrainingInitiate_GMXXX
(
    LwU32 trainingCmd
)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();
    LwU32     regVal;
    LwU32     fbpaIdx;
    LwU32     addr;
    LwU32     i;

    // If Half-FBPA is enabled on any of the FBPAs
    if (PMUCFG_FEATURE_ENABLED(PMU_MS_HALF_FBPA) &&
        (pSwAsr->halfFbpaMask != 0))
    {
        FOR_EACH_INDEX_IN_MASK(32, fbpaIdx, pSwAsr->regs.mmActiveFBIOs)
        {
            if ((pSwAsr->halfFbpaMask & BIT(fbpaIdx)) != 0)
            {
                // Update only sub-partition 1 if Half-FBPA is enabled on this FBPA
                addr = LW_PFB_FBPA_0_TRAINING_CMD(1) + (fbpaIdx * LW_FBPA_PRI_STRIDE);

                REG_WR32(FECS, addr, trainingCmd);
                regVal = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _NOW, trainingCmd);
                REG_WR32(FECS, addr, regVal);
            }
            else
            {
                for (i = 0; i < LW_PFB_FBPA_TRAINING_CMD__SIZE_1; i++)
                {
                    // Update both sub-partitions if Half-FBPA is not enabled on this FBPA
                    addr = LW_PFB_FBPA_0_TRAINING_CMD(i) + (fbpaIdx * LW_FBPA_PRI_STRIDE);

                    REG_WR32(FECS, addr, trainingCmd);
                    regVal = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _NOW, trainingCmd);
                    REG_WR32(FECS, addr, regVal);
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

        FOR_EACH_INDEX_IN_MASK(32, fbpaIdx, pSwAsr->regs.mmActiveFBIOs)
        {
            // Get the address of training partition
            addr = LW_PFB_FBPA_0_TRAINING_STATUS + (fbpaIdx * LW_FBPA_PRI_STRIDE);

            if ((pSwAsr->halfFbpaMask & BIT(fbpaIdx)) != 0)
            {
                //
                // If Half-Fbpa is enabled on thsi FBPA, Poll LW_PFB_FBPA_TRAINING_STATUS
                // of SUBP1 to determine if dram training is done
                //
                if (!PMU_REG32_POLL_NS(
                            USE_FECS(addr),
                            DRF_SHIFTMASK(LW_PFB_FBPA_0_TRAINING_STATUS_SUBP1_STATUS),
                            DRF_DEF(_PFB, _FBPA_0_TRAINING_STATUS, _SUBP1_STATUS, _FINISHED),
                            SWASR_FB_TRAINING_TIMEOUT_NS,
                            PMU_REG_POLL_MODE_BAR0_EQ))
                {
                    PMU_BREAKPOINT();
                }
            }
            else
            {
                //
                // If Half-Fbpa is not enabled on this FBPA, Poll LW_PFB_FBPA_TRAINING_STATUS
                // of both SUBP0 and SUBP1 to determine if dram training is done
                //
                if (!PMU_REG32_POLL_NS(
                            USE_FECS(addr),
                            DRF_SHIFTMASK(LW_PFB_FBPA_0_TRAINING_STATUS_SUBP0_STATUS) |
                            DRF_SHIFTMASK(LW_PFB_FBPA_0_TRAINING_STATUS_SUBP1_STATUS),
                            DRF_DEF(_PFB, _FBPA_0_TRAINING_STATUS, _SUBP0_STATUS, _FINISHED) |
                            DRF_DEF(_PFB, _FBPA_0_TRAINING_STATUS, _SUBP1_STATUS, _FINISHED),
                            SWASR_FB_TRAINING_TIMEOUT_NS,
                            PMU_REG_POLL_MODE_BAR0_EQ))
                {
                    PMU_BREAKPOINT();
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    else
    {
        // Initiate the training
        for (i = 0; i < LW_PFB_FBPA_TRAINING_CMD__SIZE_1; i++)
        {
            REG_WR32(FECS, LW_PFB_FBPA_TRAINING_CMD(i), trainingCmd);
            regVal = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _NOW, trainingCmd);
            REG_WR32(FECS, LW_PFB_FBPA_TRAINING_CMD(i), regVal);
        }

        // Poll LW_PFB_FBPA_TRAINING_STATUS to determine if dram training is done
        if (!PMU_REG32_POLL_NS(
                    USE_FECS(LW_PFB_FBPA_TRAINING_STATUS),
                    DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS) |
                    DRF_SHIFTMASK(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS),
                    DRF_DEF(_PFB, _FBPA_TRAINING_STATUS, _SUBP0_STATUS, _FINISHED) |
                    DRF_DEF(_PFB, _FBPA_TRAINING_STATUS, _SUBP1_STATUS, _FINISHED),
                    SWASR_FB_TRAINING_TIMEOUT_NS,
                    PMU_REG_POLL_MODE_BAR0_EQ))
        {
            PMU_BREAKPOINT();
        }
    }
}

/*!
 * @brief Read any sys chiplete register to synchronize and avoid
 * auto fencing.
 */
void
msHostToFecsFenceInsert_GMXXX(void)
{
    //
    // Read any sys register to avoid an auto-injected host2fecs fence. This
    // access has to use host path. However from Ampere and later there is no
    // host path from PMU. So we replaced this read with any sys register
    // read over FECS with last register we write in MS SWASR Entry sequence.
    // As per dislwssion with HW Folks, that should be enough.
    //
    REG_RD32(BAR0, LW_PPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_EXCI));
}

/* ------------------------ Private Functions  ------------------------------ */

/*!
 * @brief Put DRAM into Self Refresh
 *
 * This function puts DRAM into Self Refresh. The sequence is -
 * 1) Stop FB
 * 2) Disable ACPD
 * 3) Disable Periodic Training for GDDR5 if MCLK > 405Mhz
 * 4) Disable Periodic Refreshes (REFCTRL)
 * 5) Disable ZQCS for SDDR3
 * 6) Issue Individual Refreshes and Prechanges so that TESTCMD will get
 *    sufficient time to put DRAM into Self Refresh
 * 7) Issue TESTCMD which will put DRAM into Self Refresh.
 */
static void
s_msSelfRefreshEnter_GMXXX
(
    LwU8    ctrlId
)
{
    LwU32       val;
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    OBJSWASR   *pSwAsr = MS_GET_SWASR();

    // Stop FB request and wait for ack
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_SET,
        DRF_DEF(_CPWR_PMU, _GPIO_OUTPUT_SET, _FB_STOP_REQ, _TRUE));

    if (!PMU_REG32_POLL_NS(
         LW_CPWR_PMU_GPIO_INPUT,
         DRF_SHIFTMASK(LW_CPWR_PMU_GPIO_INPUT_FB_STOP_ACK),
         DRF_DEF(_CPWR_PMU, _GPIO_INPUT, _FB_STOP_ACK, _TRUE),
         SWASR_FB_STOP_ENGAGE_TIMEOUT_NS,
         PMU_REG_POLL_MODE_CSB_EQ))
    {
        DBG_PRINTF_PG(("TO: FB Stop\n", 0, 0, 0, 0));
        PMU_HALT();
    }

    // Disable the FB Temperature Control
    msFbTemperatureControlDisable_HAL(&Ms, LW_TRUE);

    // Disable ACPD
    msAcpdAction_HAL(&Ms, LW_TRUE);

    //
    // Disable periodic training for GDDR5 if enabled
    // It was enabled for GDDR5 > 405MHz
    //
    if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, FB_WR_TRAINING))
    {
        s_msPeriodicTrainingDisable_GMXXX();
    }

    // Disable periodic REFCTRL
    val = pSwAsr->regs.fbpaRefCtrl =
        REG_RD32(FECS, LW_PFB_FBPA_REFCTRL);
    val = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _VALID, _0, val);
    REG_WR32(FECS, LW_PFB_FBPA_REFCTRL, val);

    // Disable zq_cs_period if needed and wait for a period
    if (!pSwAsr->bIsGDDRx)
    {
        val = pSwAsr->regs.fbpaZQ =
            REG_RD32(FECS, LW_PFB_FBPA_ZQ);
        if (FLD_TEST_DRF(_PFB, _FBPA_ZQ, _CS_PERIODIC, _ENABLED, val))
        {
            val = FLD_SET_DRF(_PFB, _FBPA_ZQ, _CS_PERIODIC, _DISABLED, val);
            val = FLD_SET_DRF(_PFB, _FBPA_ZQ, _CS_CMD,      _0,        val);
            val = FLD_SET_DRF(_PFB, _FBPA_ZQ, _CL_CMD,      _0,        val);
            REG_WR32(FECS, LW_PFB_FBPA_ZQ, val);
        }
    }

    //
    // TODO: callwlate wait time based on _CONFIG4 reg and memclk
    // val = REG_RD32(FECS, LW_PFB_FBPA_CONFIG4);
    // Wait: repeat ({fbpa_config4_reg[`LW_PFB_FBPA_CONFIG4_REFRESH],
    // fbpa_config4_reg[LW_PFB_FBPA_CONFIG4_REFRESH_LO]}) @(posedge memrefclk);
    // For now, just wait for 1us.
    //
    OS_PTIMER_SPIN_WAIT_NS(1000);

    // Issue a individual refresh
    REG_WR32(FECS, LW_PFB_FBPA_REF,
                      LW_PFB_FBPA_REF_CMD_REFRESH_1);

    // Issue a individual precharge
    REG_WR32(FECS, LW_PFB_FBPA_PRE,
                      LW_PFB_FBPA_PRE_CMD_PRECHARGE_1);

    // Issue a individual refresh
    REG_WR32(FECS, LW_PFB_FBPA_REF,
                      LW_PFB_FBPA_REF_CMD_REFRESH_1);

    //
    // Wait for 2us after refresh and before SR Exit entry. The absence of
    // delay causes cfg_AR2PDEN and cfg_RFC violations.
    //
    // TODO Actual delay should be modeled by an equation
    //
    OS_PTIMER_SPIN_WAIT_NS(2000);

    // Issue the Self Refresh Entry command
    msSwAsrIssueEntryCommand_HAL(&Ms);
}

/*!
 * @brief Take out DRAM from Self Refresh
 *
 * Sequence is -
 * 1) Issue TESTCMD which will take out DRAM from Self Refresh.
 * 2) Wait for 2us. Absence of delay causes XSNRD (Self Refresh Exit No Read)
 *    violations.
 * 3) Issue Individual Refreshes and Prechanges so that we will get
 *    sufficient time to restore Periodic Refreshes.
 * 3) Restore Periodic Refreshes
 * 4) Restore ZQCS settings for SDDR3
 * 5) Wait for 2us before starting trainings for GDDR5. Absence of delay
 *    causes XSNRW(Self Refresh Exit No Read/Write) violations.
 * 6) Initiate WCK training for MCLK < 405MHz otherwise initiate WCK and WR
 *    training.
 * 7) Enable periodic training for GDDR5 if MCLK > 405Mhz
 * 8) Restore ACPD settings
 * 9) Reset the IFIFO pointers
 * 10) Release FB stop
 * 11) Wait 5.970us (GDDR5) or 20us(SDDR3). Absence of delay causes XNRD
 */
static void
s_msSelfRefreshExit_GMXXX
(
    LwU8 ctrlId
)
{
    LwU32       val;
    LwU32       i;
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    OBJSWASR   *pSwAsr   = MS_GET_SWASR();

    //
    // If calibration cycle has been asserted during regulator
    // powerUp, check if it has completed or not.
    //
    if (pSwAsr->bCalibCycleAsserted)
    {
        if (!PMU_REG32_POLL_NS(
            USE_FECS(LW_PFB_FBIO_CALMASTER_RB),
            DRF_SHIFTMASK(LW_PFB_FBIO_CALMASTER_RB_CAL_CYCLE),
            DRF_DEF(_PFB, _FBIO_CALMASTER_RB, _CAL_CYCLE, _DONE),
            SWASR_AUTO_CALIB_TIMEOUT_NS,
            PMU_REG_POLL_MODE_BAR0_EQ))
        {
            DBG_PRINTF_PG(("TO: Cal Cycle Done\n", 0, 0, 0, 0));
            PMU_HALT();
        }

        pSwAsr->bCalibCycleAsserted = LW_FALSE;
    }

    // Issue unicast read to each FBPA to flush all previous writes.
    FOR_EACH_INDEX_IN_MASK(32, i, pSwAsr->regs.mmActiveFBIOs)
    {
        val = REG_RD32(FECS, LW_PFB_FBPA_0_TESTCMD + (i * LW_FBPA_PRI_STRIDE));
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Issue the Self refresh Exit command
    msSwAsrIssueExitCommand_HAL(&Ms);

    //
    // Wait for 2us after SR Exit and before Precharge. The absence of
    // delay causes XSNRD violations.
    //
    // TODO Actual delay should be modeled by this equation
    // DELAY_IN_US =
    // {`LW_PFB_FBPA_TIMING13_ASR2NRD_MSB, `LW_PFB_FBPA_TIMING13_ASR2NRD} *
    // CK_PERIOD_IN_US
    //
    OS_PTIMER_SPIN_WAIT_NS(2000);

    REG_WR32(FECS, LW_PFB_FBPA_PRE,
                      LW_PFB_FBPA_PRE_CMD_PRECHARGE_1);

    REG_WR32(FECS, LW_PFB_FBPA_REF,
                      LW_PFB_FBPA_REF_CMD_REFRESH_1);
    REG_WR32(FECS, LW_PFB_FBPA_REF,
                      LW_PFB_FBPA_REF_CMD_REFRESH_1);

    // Restore REFCTRL which will restore periodic refreshes
    REG_WR32(FECS, LW_PFB_FBPA_REFCTRL,
                      pSwAsr->regs.fbpaRefCtrl);

    // Enable ZQS on SDDR3 - ask Mahipal for location
    if (!pSwAsr->bIsGDDRx)
    {
        REG_WR32(FECS, LW_PFB_FBPA_ZQ, pSwAsr->regs.fbpaZQ);
    }

    if (pSwAsr->bIsGDDRx)
    {
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, FB_TRAINING))
        {
            //
            // Bug 744833. Wait a minimum of 14.925ns * 120dramclks
            // before starting WCK training to avoid a XSNRW failure.
            // Round up to 2000ns to provide some margin.
            // Note: The wait method will be updated when Bug 747748 is resolved.
            //
            OS_PTIMER_SPIN_WAIT_NS(2000);

            //
            // Update the VREF settings for Training
            //
            // This step is needed from Turing and Later Gpus
            //
            msSwAsrPadVrefSet_HAL(&Ms);

            // PMU initiates WCK training (in P8/P12 states)
            val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _ENABLED, 0);
            val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _STROBE, _ENABLED, val);

            // Initiate WR training for GDDR5 in case exiting at > 405MHz
            if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, FB_WR_TRAINING))
            {
                DBG_PRINTF_PG(("Performing GDDR5 WR training as MCLK Freq is "
                               "higher than 405MHz\n", 0, 0, 0, 0));

                val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED, val);
                val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _DLL, val);
            }

            // Initiate the FB Training
            msFbTrainingInitiate_HAL(&Ms, val);

            //
            // Restore the VREF settings
            //
            // This step is needed from Turing and Later Gpus
            //
            msSwAsrPadVrefRestore_HAL(&Ms);
        }

        // Reset the IFIFO pointers
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET, 1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET, 0, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG1, val);
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);

        // Enable periodic training for GDDR5/6/6x, if Exit freq is > 405Mhz
        if (LPWR_CTRL_IS_SF_ENABLED(pPgState, MS, FB_WR_TRAINING))
        {
            msPeriodicTrainingEnable_HAL(&Ms);
        }
    }

    // Restore ACPD
    msAcpdAction_HAL(&Ms, LW_FALSE);

    if (!pSwAsr->bIsGDDRx)
    {
        // Reset the IFIFO pointers
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET, 1, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG1, val);
        val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET, 0, val);
        REG_WR32(FECS, LW_PFB_FBPA_FBIO_CFG1, val);
        val = REG_RD32(FECS, LW_PFB_FBPA_FBIO_CFG1);
    }

    // Restore the FB Temperature Control - Turing and Later
    msFbTemperatureControlDisable_HAL(&Ms, LW_FALSE);

    // Release FB_STOP
    REG_WR32(CSB, LW_CPWR_PMU_GPIO_OUTPUT_CLEAR,
        DRF_DEF(_CPWR_PMU, _GPIO_OUTPUT_CLEAR, _FB_STOP_REQ, _TRUE));

    if (!PMU_REG32_POLL_NS(
        LW_CPWR_PMU_GPIO_INPUT,
        DRF_SHIFTMASK(LW_CPWR_PMU_GPIO_INPUT_FB_STOP_ACK),
        DRF_DEF(_CPWR_PMU, _GPIO_INPUT, _FB_STOP_ACK, _FALSE),
        SWASR_FB_STOP_RELEASE_TIMEOUT_NS,
        PMU_REG_POLL_MODE_CSB_EQ))
    {
        DBG_PRINTF_PG(("TO: Release FB Stop\n", 0, 0, 0, 0));
        PMU_HALT();
    }

    //
    // Bug 746015. Wait a minimum of 2.778ns * 5000dramclks
    // before releasing the holdoffs to avoid XSRD failures.
    // Round up to 20,000ns to provide some margin.
    // Note: The wait method will be updated when Bug 747748 is resolved.
    // Bug 744833: Wait a minimum of 14.925ns * 400dramclks for DDR5.
    //
    if (pSwAsr->bIsGDDRx)
    {
        OS_PTIMER_SPIN_WAIT_NS(5970);
    }
    else
    {
        OS_PTIMER_SPIN_WAIT_NS(20000);
    }
}

/*!
 * @brief Disable Periodic Training
 *
 * We do Periodic Training for GDDR5 when MCLK > 405MHz (in P5 and above
 * PState). If we are doing SW-ASR for MCLK greater than 405MHz then disable
 * Periodic Training before putting DRAM into Self Refresh.
 */
static void
s_msPeriodicTrainingDisable_GMXXX(void)
{
    LwU32 val;

    //
    // We always perform same training in all partitions for a definite
    // memory type, so read value from sub-partition 1. We chose sub-partition 1
    // as it is guaranteed to be available since Half-FBPA disables sub-partition 0.
    //
    val = REG_RD32(FECS, LW_PFB_FBPA_TRAINING_CMD(1));

    // Reset it only if it's enabled
    if (FLD_TEST_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _ENABLED, val))
    {
        val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _DISABLED, val);
        val = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _INIT, val);

        // Write the training register with considering HALF FBPA mods
        msFbTrainingCommandUpdate_HAL(&Ms, val);
    }
}

/*!
 * @brief Disable Main Regulator
 *
 * This function disables main regulator and shifts to bypass regulator.
 * Ensure that FB Pads are running in low voltage mode before shifting
 * to bypass regulator. i.e. We should enable bypass regulator after
 * completion of _msFbPadPwrDown sequence.
 */
static void
s_msMainRegulatorDisable_GMXXX(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    //
    // Disable main regulator LW_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE = 0
    // Clear all the bits of this register to make sure that all the partitions
    // are put on the bypass regulator. Log a bug with HW to add defines
    // for spare bits.
    //
    pSwAsr->regs.sysFbioSpare = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_SPARE);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, 0);

    // Read back the register to flush the pipe
    REG_RD32(FECS, LW_PTRIM_SYS_FBIO_SPARE);
}

/*!
 * @brief Issue FB Priv Flush (Or Fence)
 *
 * A polling read to the FB priv FENCE that ensure all commands have been
 * flushed to the FBPA/FBIO registers.
 */
static void
s_msSwAsrIssueFbPrivFlush_GMXXX(void)
{
    if (!PMU_REG32_POLL_NS(
         USE_FECS(LW_PPRIV_FBP_PRI_FENCE),
         DRF_SHIFTMASK(LW_PPRIV_FBP_PRI_FENCE_V),
         LW_PPRIV_FBP_PRI_FENCE_V_0,
         SWASR_PRIV_FENCE_TIMEOUT_NS,
         PMU_REG_POLL_MODE_BAR0_EQ))
    {
        DBG_PRINTF_PG(("TO: Fence\n", 0, 0, 0, 0));
        PMU_HALT();
    }
}

/*!
 * Enable Main Regulator
 *
 * This function enables Main FB Regulator and waits for 1us to settle down
 * the voltage of Main Regulator.
 */
static void
s_msMainRegulatorEnable_GMXXX(void)
{
    OBJSWASR *pSwAsr = MS_GET_SWASR();

    //
    // Disable the flag stating autoCalibration enable, as this will be
    // set later if calibration cycle has been asserted
    //
    pSwAsr->bCalibCycleAsserted = LW_FALSE;

    // Enable main regulator  LW_PTRIM_SYS_FBIO_SPARE_FBIO_SPARE[0] = 1
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, pSwAsr->regs.sysFbioSpare);

    // Wait for 1 us to let it settle
    OS_PTIMER_SPIN_WAIT_NS(1000);

    // Read back the register to flush the pipe
    REG_RD32(FECS, LW_PTRIM_SYS_FBIO_SPARE);

    // Enable the COMP pads, which were disabled during MSCG entry
    msSwAsrCompPadPwrUp_HAL();
}
