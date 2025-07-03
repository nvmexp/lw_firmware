/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwrprivblockertu10x.c
 * @brief  LPWR Priv Blocker Control File
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_gsp.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_lw_xve.h"
#endif
#include "dev_sec_pri.h"
#include "priv_blocker_wl_bl_ranges_init.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_objic.h"
#include "dbgprintf.h"

#include "config/g_lpwr_private.h"

//
// Compile time check to ensure that SEC and GSP Priv blocker range enabled defined
// are having fixed values
//
ct_assert(LW_PSEC_PRIV_BLOCKER_GR_WL_CFG_RANGE0_ENABLED == 1);
ct_assert(LW_PGSP_PRIV_BLOCKER_GR_BL_CFG_RANGE0_ENABLED == 1);
ct_assert(LW_PSEC_PRIV_BLOCKER_MS_WL_CFG_RANGE0_ENABLED == 1);
ct_assert(LW_PGSP_PRIV_BLOCKER_MS_BL_CFG_RANGE0_ENABLED == 1);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//
// Arrays for populating the list of Disallow list and Allowlist ranges
// for SEC2 and GSP Priv Blocker.
//
// This array is created from the file priv_blocker_wl_bl_range_init.h file
// which is provided by HW folks.
//

// List of Various Allow ranges for SEC/GSP Priv Blocker for GR Profile
LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE lpwrPrivBlockerGrAllowRanges[]
    GCC_ATTRIB_SECTION("dmem_lpwrPrivBlockerInitData", "lpwrPrivBlockerGrAllowRanges") =
{
    PRIV_BLOCKER_GR_PROFILE_WL_RANGES_INIT(LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGES_GET)
};

// List of Various Disallow ranges for SEC/GSP Priv Blocker for GR Profile
LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE lpwrPrivBlockerGrDisallowRanges[]
    GCC_ATTRIB_SECTION("dmem_lpwrPrivBlockerInitData", "lpwrPrivBlockerGrDisallowRanges") =
{
    PRIV_BLOCKER_GR_PROFILE_BL_RANGES_INIT(LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGES_GET)
};

// List of Various Allow ranges for SEC/GSP Priv Blocker for MS Profile
LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE lpwrPrivBlockerMsAllowRanges[]
    GCC_ATTRIB_SECTION("dmem_lpwrPrivBlockerInitData", "lpwrPrivBlockerMsAllowRanges") =
{
    PRIV_BLOCKER_MS_PROFILE_WL_RANGES_INIT(LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGES_GET)
};

// List of Various Disallow ranges for SEC/GSP Priv Blocker for MS Profile
LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE lpwrPrivBlockerMsDisallowRanges[]
    GCC_ATTRIB_SECTION("dmem_lpwrPrivBlockerInitData", "lpwrPrivBlockerMsDisallowRanges") =
{
    PRIV_BLOCKER_MS_PROFILE_BL_RANGES_INIT(LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGES_GET)
};

/* ------------------------- Prototypes ------------------------------------- */
static void  s_lpwrPrivBlockerSec2IntrInit_TU10X(void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPrivBlockerSec2IntrInit_TU10X");
static void  s_lpwrPrivBlockerGspIntrInit_TU10X(void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPrivBlockerGspIntrInit_TU10X");
static void  s_lpwrPrivBlockerSec2IntrProcess_TU10X(LwU32 intrStat);
static void  s_lpwrPrivBlockerGspIntrProcess_TU10X (LwU32 intrStat);
static void  s_lpwrPrivBlockerSec2ModeSet_TU10X    (LwU32 targetBlockerMode,
                                                    LwU32 targetProfileMask);
static void  s_lpwrPrivBlockerGspModeSet_TU10X     (LwU32 targetBlockerMode,
                                                    LwU32 targetProfileMask);
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
static LwU32 s_lpwrPrivBlockerXveIntrFieldIgnoreMaskGet_TU10X(void);
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
static void  s_lpwrPrivBlockerSec2RangesInit_TU10X (LwU8 profile, LwU8 rangeType,
                                                    LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE *pLpwrPrivBlockerRanges,
                                                    LwU8 size)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPrivBlockerSec2RangesInit_TU10X");
static void  s_lpwrPrivBlockerGspRangesInit_TU10X  (LwU8 profile, LwU8 rangeType,
                                                    LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE *pLpwrPrivBlockerRanges,
                                                    LwU8 size)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPrivBlockerGspRangesInit_TU10X");

/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Init and Enable the Priv Blocker interrupt
 */
FLCN_STATUS
lpwrPrivBlockerIntrInit_TU10X(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

    // Init the SEC2 Priv Block Interrupt
    if ((pLpwrPrivBlocker->supportMask & BIT(RM_PMU_PRIV_BLOCKER_ID_SEC2)) != 0)
    {
        s_lpwrPrivBlockerSec2IntrInit_TU10X();
    }

    // Init the GSP Priv Block Interrupt
    if ((pLpwrPrivBlocker->supportMask & BIT(RM_PMU_PRIV_BLOCKER_ID_GSP)) != 0)
    {
       s_lpwrPrivBlockerGspIntrInit_TU10X();
    }

    // Init the XVE Priv Blocker Interrupt
    if ((pLpwrPrivBlocker->supportMask & BIT(RM_PMU_PRIV_BLOCKER_ID_XVE)) != 0)
    {
        lpwrPrivBlockerXveIntrInit_HAL(&Lpwr);
    }

    return FLCN_OK;
}

/*!
 * @brief Init and Enable the XVE Priv Blocker interrupts
 */
void
lpwrPrivBlockerXveIntrInit_TU10X(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();
    LwU32              regVal = 0;

    appTaskCriticalEnter();
    {
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

        // MS Profile Interrupt
        regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                             _XVE_BAR_BLOCKER, _ENABLED, regVal);

        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, regVal);

        // Cache the XVE Interrupt enable Mask
        pLpwrPrivBlocker->gpio1IntrEnMask = FLD_SET_DRF(_CPWR,
                                                   _PMU_GPIO_1_INTR_RISING_EN,
                                                   _XVE_BAR_BLOCKER, _ENABLED,
                                                   pLpwrPrivBlocker->gpio1IntrEnMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief Init and Enable the XVE Priv Blocker timeout
 */
void
lpwrPrivBlockerXveTimeoutInit_TU10X(void)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    LwU32     val;
    //
    // Enable the BAR Blocker interrupt timeout. If PMU SW is hosed with
    // Lowpower engaged for any reason (like a crash due to stack overrun or
    // a and breakpoint being hit), then this timeout will fire after _VAL
    // usec disengage the XVE blocker so that it does not become a fatal
    // error and hang the PCIe bus.
    //
    val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_INTR_TIMEOUT);

    //
    // For emulation run, we should keep the timeout to max possible value.
    // Timeout field is 16 bit value, so max timeout is 0xFFFF. Emulation
    // runs are slow and we might hit timeout during LowPower entry. So we are
    // keeping the timeout to max possible value.
    //
    if (IsEmulation())
    {
        val = FLD_SET_DRF_NUM(_XVE, _BAR_BLOCKER_INTR_TIMEOUT, _VAL,
                              LPWR_PRIV_BLOCKERS_XVE_TIMEOUT_EMU_US, val);
    }

    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_INTR_TIMEOUT, _EN, _ENABLED, val);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_INTR_TIMEOUT, val);
#endif
}

/*!
 * @brief Process the Priv Blocker interrupts and colwert them to
 * SW Events
 *
 * @param[in]   pLpwrEvt        This contains eventType, and the event's
 *                              private data
 *
 * @return      FLCN_OK                 On success
 */
FLCN_STATUS
lpwrPrivBlockerIntrProcess_TU10X
(
    DISPATCH_LPWR   *pLpwrEvt
)
{
    LwU32              intrStat         = 0;
    LwU32              ctrlId           = 0;
    FLCN_STATUS        status           = FLCN_OK;
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

    // We should read/write interrupt register under critical section.
    appTaskCriticalEnter();
    {
        // Get the intterupt status from Event
        intrStat = pLpwrEvt->intr.intrStat;

        // Process the XVE Blocker Interrupt
        lpwrPrivBlockerXveIntrProcess_HAL(&Lpwr, intrStat);

        // Process the SEC2 Priv Blocker Interrupt
        s_lpwrPrivBlockerSec2IntrProcess_TU10X(intrStat);

        // Process the GSP Priv Blocker Interrupt
        s_lpwrPrivBlockerGspIntrProcess_TU10X(intrStat);

        //
        // Enable the Priv Blocker interrupt. These interrupts were disabled
        // during the ISR of priv blocker interrupts.
        //
        intrStat = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
        intrStat |= pLpwrPrivBlocker->gpio1IntrEnMask;
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, intrStat);
    }
    appTaskCriticalExit();

    // Loop Over all the supported LPWR_CTRLs
    FOR_EACH_INDEX_IN_MASK(32, ctrlId, Pg.supportedMask)
    {
        // Process wake-up event only if priv blocker Wakeup is pending
        if (lpwrPrivBlockerIsWakeupPending(ctrlId))
        {
            status = pgCtrlEventSend(ctrlId, PMU_PG_EVENT_WAKEUP,
                                     pLpwrPrivBlocker->wakeUpReason[ctrlId]);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto lpwrPrivBlockerIntrProcess_TU10X_exit;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Reset the Wakeup reason
    lpwrPrivBlockerWakeupMaskClear(Pg.supportedMask);

lpwrPrivBlockerIntrProcess_TU10X_exit:
    return status;
}

/*!
 * @brief Process the XVE Bar Blocker interrupt
 */
void
lpwrPrivBlockerXveIntrProcess_TU10X
(
    LwU32   intrStat
)
{
    //
    // On Turing, we only have one profile in XVE Blocker and
    // that is applicable for MS Range only.
    //
    if (PENDING_IO_BANK1(_XVE_BAR_BLOCKER, _RISING, intrStat))
    {
        // Set the Wakeup CtrlId and Wakeup Reason
        lpwrPrivBlockerWakeupMaskSet(lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS),
                                     PG_WAKEUP_EVENT_XVE_PRIV_BLOCKER);
    }
}

/*!
 * @brief API to update the Priv Blocker Mode in HW
 *
 * @param[in]   blockerId  Blocker Ids for which we need
 *                         to update the HW State
 */
void
lpwrPrivBlockerModeSet_TU10X
(
    LwU32 blockerId,
    LwU32 targetBlockerMode,
    LwU32 targetProfileMask
)
{
    if (blockerId == RM_PMU_PRIV_BLOCKER_ID_XVE)
    {
        // Set XVE Priv Blocker Mode
        lpwrPrivBlockerXveModeSet_HAL(&Lpwr, targetBlockerMode,
                                      targetProfileMask);
    }

    if (blockerId == RM_PMU_PRIV_BLOCKER_ID_SEC2)
    {
        // Set SEC Priv Blocker Mode
        s_lpwrPrivBlockerSec2ModeSet_TU10X(targetBlockerMode, targetProfileMask);
    }

    if (blockerId == RM_PMU_PRIV_BLOCKER_ID_GSP)
    {
        // Set GSP Priv Blocker Mode
        s_lpwrPrivBlockerGspModeSet_TU10X(targetBlockerMode, targetProfileMask);
    }
}

/*!
 * @brief API to poll for the priv blcoker mode to get
 * updated in HW
 *
 * @param[in]   blockerId  Blocker Id for which we need to
 *                         update the HW State
 *
 * @param[in]  timeOutNs   Timeout in Ns for polling
 */
FLCN_STATUS
lpwrPrivBlockerWaitForModeChangeCompletion_TU10X
(
    LwU32 blockerId,
    LwU32 timeOutNs
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LPWR_PRIV_BLOCKER_CTRL *pLpwrPrivBlockerCtrl;
    FLCN_TIMESTAMP          privBlockerPollStartTimeNs;
    LwBool                  bStopPolling  = LW_FALSE;
    LwBool                  bEquationMode = LW_FALSE;
    LwU32                   val;

    // Start time for Blocker PRIV FLUSH Path
    osPTimerTimeNsLwrrentGet(&privBlockerPollStartTimeNs);

    //Poll for the blokcers to get engaged.
    do
    {
        bStopPolling = LW_TRUE;

        if (blockerId == RM_PMU_PRIV_BLOCKER_ID_XVE)
        {
            pLpwrPrivBlockerCtrl = LPWR_GET_PRIV_BLOCKER_CTRL(blockerId);

            bEquationMode = (pLpwrPrivBlockerCtrl->lwrrentMode ==
                                        LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION) ?
                                        LW_TRUE : LW_FALSE;

            // Poll for the XVE Blocker
            val = REG_RD32(FECS_CFG, LW_XVE_BAR_BLOCKER);
            bStopPolling = bStopPolling && (bEquationMode ? FLD_TEST_DRF(_XVE,
                                                         _BAR_BLOCKER_CTRL,
                                                         _BLOCKER_MODE,
                                                         _BLOCK_EQUATION, val) :
                                            FLD_TEST_DRF(_XVE, _BAR_BLOCKER_CTRL,
                                                         _BLOCKER_MODE,
                                                         _BLOCK_EVERYTHING, val));
        }

        // Poll for SEC2 Priv Blocker
        if (blockerId == RM_PMU_PRIV_BLOCKER_ID_SEC2)
        {
            val          = REG_RD32(FECS, LW_PSEC_PRIV_BLOCKER_STAT);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PSEC,
                                                        _PRIV_BLOCKER_STAT,
                                                        _STATUS, _BLOCKED,
                                                        val);
        }

        // Poll for GSP Priv Blocker
        if (blockerId == RM_PMU_PRIV_BLOCKER_ID_GSP)
        {
            val          = REG_RD32(FECS, LW_PGSP_PRIV_BLOCKER_STAT);
            bStopPolling = bStopPolling && FLD_TEST_DRF(_PGSP,
                                                        _PRIV_BLOCKER_STAT,
                                                        _STATUS, _BLOCKED,
                                                         val);
        }

        //
        // Check if Priv Blocker Mode change took more time than allowed
        // timeout value If so, send the timeout error
        //
        if (osPTimerTimeNsElapsedGet(&privBlockerPollStartTimeNs) >=
                                     timeOutNs)
        {
            return FLCN_ERR_TIMEOUT;
        }
    }
    while (!bStopPolling);
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))

    return FLCN_OK;
}

/*!
 * @brief Update the XVE Priv Blocker HW State
 */
void
lpwrPrivBlockerXveModeSet_TU10X
(
    LwU32 targetBlockerMode,
    LwU32 targetProfileMask
)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32 val;

    val = REG_RD32(FECS_CFG, LW_XVE_BAR_BLOCKER);

    //
    // Mask out the interrupt specific bit i.e make them zero.
    // We should not write "1" to those bits in case interrupt
    // status is pending. Interrupt will be serviced
    // as a part of ISR and Blocker dis engagement
    // step.
    //
    // If interrupt related bit arr set and  if we write
    // 1 to them, HW will consider them as interrupt
    //  acknowledgement and will pass the transaction which
    //  we do not want.
    //
    val = val & s_lpwrPrivBlockerXveIntrFieldIgnoreMaskGet_TU10X();

    // Set the Blocker Mode Overwrite Bit
    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _CTRL_BLOCKER_MODE_OVERWRITE,
                      _ENABLED, val);

    switch (targetBlockerMode)
    {
        case LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE:
        {
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _CTRL_BLOCKER_MODE,
                              _BLOCK_NOTHING, val);
            break;
        }

        case LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL:
        {
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _CTRL_BLOCKER_MODE,
                              _BLOCK_EVERYTHING, val);
            break;
        }

        case LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION:
        {
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _CTRL_BLOCKER_MODE,
                              _BLOCK_EQUATION, val);
            break;
        }
    }

    REG_WR32_STALL(FECS_CFG, LW_XVE_BAR_BLOCKER, val);
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
}

/*!
 * @brief Program and enable Disallow Ranges for XVE Blocker
 *
 * Disallow Range - Help us to disallow access to a register or a range which has
 * been listed in Allowlist in XVE Blocker in HW. This is essentially a CYA
 */
void
lpwrPrivBlockerXveMsProfileDisallowRangesInit_TU10X(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    OBJPGSTATE *pMsState       = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU8        gpioMutexPhyId = MUTEX_LOG_TO_PHY_ID(LW_MUTEX_ID_GPIO);
    LwU32       val;

    //
    // In the true sense LW_PPWR_PMU_MUTEX(gpioMutexPhyId) is not really
    // a disallowed access to register for MSCG. We have added GPIO Mutex register
    // to Disallow list, so that any RM access to GPIO will disengage PSI by waking MSCG.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)   &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, PSI))
    {
        val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);

        // Add register in CYA_DISALLOW_RANGE0
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_START_ADDR(0),
                           LW_PPWR_PMU_MUTEX(gpioMutexPhyId));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_DISALLOW_RANGE_END_ADDR(0),
                           LW_PPWR_PMU_MUTEX(gpioMutexPhyId));

        // Enable DISALLOW_RANGE0
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _DISALLOW_RANGE0, _ENABLED,
                          val);

        // Program the Disallow Range settings into HW
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, val);
    }
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
}

/*!
 * @brief Program and enable Disallow list and Allowlist ranges for priv blockers
 */
void
lpwrPrivBlockerAllowDisallowRangesInit_TU10X(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD( _DMEM, _ATTACH, lpwrPrivBlockerInitData)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if ((pLpwrPrivBlocker->supportMask & BIT(RM_PMU_PRIV_BLOCKER_ID_GSP)) != 0)
        {
            // Program the GR profile Disallow Ranges
            s_lpwrPrivBlockerGspRangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_GR,
                                                 LPWR_PRIV_BLOCKER_RANGE_DISALLOW,
                                                 lpwrPrivBlockerGrDisallowRanges,
                                                 LW_ARRAY_ELEMENTS(lpwrPrivBlockerGrDisallowRanges));

            // Program the GR profile Allow Ranges
            s_lpwrPrivBlockerGspRangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_GR,
                                                 LPWR_PRIV_BLOCKER_RANGE_ALLOW,
                                                 lpwrPrivBlockerGrAllowRanges,
                                                 LW_ARRAY_ELEMENTS(lpwrPrivBlockerGrAllowRanges));

            // Program the MS profile Disallow Ranges
            s_lpwrPrivBlockerGspRangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_MS,
                                                 LPWR_PRIV_BLOCKER_RANGE_DISALLOW,
                                                 lpwrPrivBlockerMsDisallowRanges,
                                                 LW_ARRAY_ELEMENTS(lpwrPrivBlockerMsDisallowRanges));

            // Program the MS profile Allow Ranges
            s_lpwrPrivBlockerGspRangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_MS,
                                                 LPWR_PRIV_BLOCKER_RANGE_ALLOW,
                                                 lpwrPrivBlockerMsAllowRanges,
                                                 LW_ARRAY_ELEMENTS(lpwrPrivBlockerMsAllowRanges));
        }

        if ((pLpwrPrivBlocker->supportMask & BIT(RM_PMU_PRIV_BLOCKER_ID_SEC2)) != 0)
        {
            // Program the GR profile Disallow Ranges
            s_lpwrPrivBlockerSec2RangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_GR,
                                                  LPWR_PRIV_BLOCKER_RANGE_DISALLOW,
                                                  lpwrPrivBlockerGrDisallowRanges,
                                                  LW_ARRAY_ELEMENTS(lpwrPrivBlockerGrDisallowRanges));

            // Program the GR profile Allow Ranges
            s_lpwrPrivBlockerSec2RangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_GR,
                                                  LPWR_PRIV_BLOCKER_RANGE_ALLOW,
                                                  lpwrPrivBlockerGrAllowRanges,
                                                  LW_ARRAY_ELEMENTS(lpwrPrivBlockerGrAllowRanges));

            // Program the MS profile Disallow Ranges
            s_lpwrPrivBlockerSec2RangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_MS,
                                                  LPWR_PRIV_BLOCKER_RANGE_DISALLOW,
                                                  lpwrPrivBlockerMsDisallowRanges,
                                                  LW_ARRAY_ELEMENTS(lpwrPrivBlockerMsDisallowRanges));

            // Program the MS profile Allow Ranges
            s_lpwrPrivBlockerSec2RangesInit_TU10X(LPWR_PRIV_BLOCKER_PROFILE_MS,
                                                  LPWR_PRIV_BLOCKER_RANGE_ALLOW,
                                                  lpwrPrivBlockerMsAllowRanges,
                                                  LW_ARRAY_ELEMENTS(lpwrPrivBlockerMsAllowRanges));
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/* ------------------------ Private Functions  ------------------------------ */

/*!
 * @brief Process the SEC2 Priv Blocker Interrupt
 */
static void
s_lpwrPrivBlockerSec2IntrProcess_TU10X
(
    LwU32 intrStat
)
{
    LwU32 sec2PrivIntr = 0;

    if (PENDING_IO_BANK1(_SEC_PRIV_BLOCKER, _RISING, intrStat))
    {
        // Read the Interrupt register of SEC2 Priv blocker
        sec2PrivIntr = REG_RD32(FECS, LW_PSEC_PRIV_BLOCKER_INTR);

        //
        // Check which of the Profile caused the wakeup and then set the corressponding
        // ctrlIdMask.
        //
        // If GR Profile caused the wakeup, set GR Feature CtrlId for wakeup
        // If MS Profile caused the wakeup, set MSCG ctrlId for wakeup
        //
        if (FLD_TEST_DRF(_PSEC, _PRIV_BLOCKER, _INTR_GR, _PENDING, sec2PrivIntr))
        {
            // Reset the GR interrupt line
            sec2PrivIntr = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER, _INTR_GR,
                                       _RESET, sec2PrivIntr);

            // Set the Wakeup CtrlId and Wakeup Reason
            lpwrPrivBlockerWakeupMaskSet(lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_GR),
                                         PG_WAKEUP_EVENT_SEC2_PRIV_BLOCKER);
        }

        if (FLD_TEST_DRF(_PSEC, _PRIV_BLOCKER, _INTR_MS, _PENDING, sec2PrivIntr))
        {
            // Reset the MS interrupt line
            sec2PrivIntr = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER, _INTR_MS,
                                       _RESET, sec2PrivIntr);

            // Set the Wakeup CtrlId and Wakeup Reason
            lpwrPrivBlockerWakeupMaskSet(lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS),
                                         PG_WAKEUP_EVENT_SEC2_PRIV_BLOCKER);
        }

        // Clear the Blocker interrupt
        REG_WR32_STALL(FECS, LW_PSEC_PRIV_BLOCKER_INTR, sec2PrivIntr);
    }
}

/*!
 * @brief Process the GSP Priv Blocker Interrupt
 */
static void
s_lpwrPrivBlockerGspIntrProcess_TU10X
(
    LwU32 intrStat
)
{
    LwU32 gspPrivIntr = 0;

    if (PENDING_IO_BANK1(_GSP_PRIV_BLOCKER, _RISING, intrStat))
    {
        // Read the Interrupt register of GSP Priv blocker
        gspPrivIntr = REG_RD32(FECS, LW_PGSP_PRIV_BLOCKER_INTR);

        //
        // Check which of the Profile caused the wakeup and then set the corresponding
        // ctrlIdMask.
        //
        // If GR Profile caused the wakeup, set GR Feature CtrlId for wakeup
        // If MS Profile caused the wakeup, set MSCG ctrlId for wakeup
        //
        if (FLD_TEST_DRF(_PGSP, _PRIV_BLOCKER, _INTR_GR, _PENDING, gspPrivIntr))
        {
            // Reset the GR interrupt line
            gspPrivIntr = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER, _INTR_GR,
                                      _RESET, gspPrivIntr);

            // Set the Wakeup CtrlId and Wakeup Reason
            lpwrPrivBlockerWakeupMaskSet(lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_GR),
                                         PG_WAKEUP_EVENT_GSP_PRIV_BLOCKER);
        }

        if (FLD_TEST_DRF(_PGSP, _PRIV_BLOCKER, _INTR_MS, _PENDING, gspPrivIntr))
        {
            // Reset the MS interrupt line
            gspPrivIntr = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER, _INTR_MS,
                                      _RESET, gspPrivIntr);

            // Set the Wakeup CtrlId and Wakeup Reason
            lpwrPrivBlockerWakeupMaskSet(lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS),
                                         PG_WAKEUP_EVENT_GSP_PRIV_BLOCKER);
        }

        // Clear the Blocker interrupt
        REG_WR32_STALL(FECS, LW_PGSP_PRIV_BLOCKER_INTR, gspPrivIntr);
    }
}

/*!
 * @brief Init and Enable the SEC2 Priv Blocker interrupts
 */
static void
s_lpwrPrivBlockerSec2IntrInit_TU10X(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();
    LwU32              regVal = 0;

    //
    // We need to programm the blocker's BLOCKALL_CYA bit to
    // BLOCK_GR_MS/BLOCK_MS. This field will be in effect only when
    // blocker is in BLOCK_ALL mode.
    // Setting _BLOCKALL_CYA bit to _BLOCK_GR_MS/BLOCK_MS, will program
    // the blocker to generate the interrupt in BLOCK_ALL
    // mode only for GR/MS Disallow access.
    //
    regVal = REG_RD32(FECS, LW_PSEC_PRIV_BLOCKER_CTRL);
    regVal = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_CTRL, _BLOCKALL_CYA,
                         _BLOCK_GR_MS, regVal);

    REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_CTRL, regVal);

    appTaskCriticalEnter();
    {
        // Enable the SEC2 Priv Blocker interrupt on GPIO Lines
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
        regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                             _SEC_PRIV_BLOCKER, _ENABLED, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, regVal);

        // Cache the SEC2 Interrupt enable Mask
        pLpwrPrivBlocker->gpio1IntrEnMask = FLD_SET_DRF(_CPWR,
                                                   _PMU_GPIO_1_INTR_RISING_EN,
                                                   _SEC_PRIV_BLOCKER, _ENABLED,
                                                   pLpwrPrivBlocker->gpio1IntrEnMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief Init and Enable the GSP Priv Blocker interrupts
 */
static void
s_lpwrPrivBlockerGspIntrInit_TU10X(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();
    LwU32              regVal = 0;

    //
    // We need to programm the blocker's BLOCKALL_CYA bit to
    // BLOCK_GR_MS/BLOCK_MS. This field will be in effect only when
    // blocker is in BLOCK_ALL mode.
    // Setting _BLOCKALL_CYA bit to _BLOCK_GR_MS/BLOCK_MS, will program
    // the blocker to generate the interrupt in BLOCK_ALL
    // mode only for GR/MS Disallow access.
    //
    regVal = REG_RD32(FECS, LW_PGSP_PRIV_BLOCKER_CTRL);
    regVal = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_CTRL, _BLOCKALL_CYA,
                         _BLOCK_GR_MS, regVal);

    REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_CTRL, regVal);

    appTaskCriticalEnter();
    {
        // Enable the GSP Priv Blocker interrupt
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
        regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                             _GSP_PRIV_BLOCKER, _ENABLED, regVal);
        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, regVal);

        // Cache the GSP Interrupt enable Mask
        pLpwrPrivBlocker->gpio1IntrEnMask = FLD_SET_DRF(_CPWR,
                                                   _PMU_GPIO_1_INTR_RISING_EN,
                                                   _GSP_PRIV_BLOCKER, _ENABLED,
                                                   pLpwrPrivBlocker->gpio1IntrEnMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief Init the SEC Priv Blocker Disallow/Allow Ranges
 */
static void
s_lpwrPrivBlockerSec2RangesInit_TU10X
(
    LwU8                                   profile,
    LwU8                                   rangeType,
    LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE *pLpwrPrivBlockerWlBlAddr,
    LwU8                                   listSize
)
{
    LwU32 val;
    LwU32 idx;

    val = REG_RD32(FECS, LW_PSEC_PRIV_BLOCKER_RANGEC);

    // Set the parameteres for rang control register
    val = FLD_SET_DRF_NUM(_PSEC, _PRIV_BLOCKER_RANGEC, _OFFS, 0, val);
    val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_RANGEC, _AINCW, _TRUE, val);

    if (rangeType == LPWR_PRIV_BLOCKER_RANGE_DISALLOW)
    {
        val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_RANGEC, _BLK, _BL, val);
    }
    else
    {
        val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_RANGEC, _BLK, _WL, val);
    }

    if (profile == LPWR_PRIV_BLOCKER_PROFILE_MS)
    {
        val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_RANGEC, _DOMAIN, _MS, val);
    }
    else if (profile == LPWR_PRIV_BLOCKER_PROFILE_GR)
    {
        val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_RANGEC, _DOMAIN, _GR, val);
    }

    // Program the Range Control register
    REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_RANGEC, val);

    // Program the Ranges Start and End Addresses
    for (idx = 0; idx < listSize; idx++)
    {
        REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_RANGED, pLpwrPrivBlockerWlBlAddr[idx].startAddr);
        REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_RANGED, pLpwrPrivBlockerWlBlAddr[idx].endAddr);
    }

    // Set the Ranges Enabled Mask
    val = BIT(listSize) - 1;

    if (rangeType == LPWR_PRIV_BLOCKER_RANGE_DISALLOW)
    {
        if (profile == LPWR_PRIV_BLOCKER_PROFILE_MS)
        {
            REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_MS_BL_CFG, val);
        }
        else if (profile == LPWR_PRIV_BLOCKER_PROFILE_GR)
        {
            REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_GR_BL_CFG, val);
        }
    }
    else
    {
        if (profile == LPWR_PRIV_BLOCKER_PROFILE_MS)
        {
            REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_MS_WL_CFG, val);
        }
        else if (profile == LPWR_PRIV_BLOCKER_PROFILE_GR)
        {
            REG_WR32(FECS, LW_PSEC_PRIV_BLOCKER_GR_WL_CFG, val);
        }
    }
}

/*!
 * @brief Init the GSP Priv Blocker Disallow/Allow Ranges
 */
static void
s_lpwrPrivBlockerGspRangesInit_TU10X
(
    LwU8                                   profile,
    LwU8                                   rangeType,
    LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE *pLpwrPrivBlockerWlBlAddr,
    LwU8                                   listSize
)
{
    LwU32 val;
    LwU32 idx;

    val = REG_RD32(FECS, LW_PGSP_PRIV_BLOCKER_RANGEC);

    // Set the parameteres for rang control register
    val = FLD_SET_DRF_NUM(_PGSP, _PRIV_BLOCKER_RANGEC, _OFFS, 0, val);
    val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_RANGEC, _AINCW, _TRUE, val);

    if (rangeType == LPWR_PRIV_BLOCKER_RANGE_DISALLOW)
    {
        val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_RANGEC, _BLK, _BL, val);
    }
    else
    {
        val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_RANGEC, _BLK, _WL, val);
    }

    if (profile == LPWR_PRIV_BLOCKER_PROFILE_MS)
    {
        val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_RANGEC, _DOMAIN, _MS, val);
    }
    else if (profile == LPWR_PRIV_BLOCKER_PROFILE_GR)
    {
        val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_RANGEC, _DOMAIN, _GR, val);
    }

    // Program the Range Control register
    REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_RANGEC, val);

    // Program the Ranges Start and End Addresses
    for (idx = 0; idx < listSize; idx++)
    {
        REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_RANGED, pLpwrPrivBlockerWlBlAddr[idx].startAddr);
        REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_RANGED, pLpwrPrivBlockerWlBlAddr[idx].endAddr);
    }

    // Set the Ranges Enabled Mask
    val = BIT(listSize) - 1;

    if (rangeType == LPWR_PRIV_BLOCKER_RANGE_DISALLOW)
    {
        if (profile == LPWR_PRIV_BLOCKER_PROFILE_MS)
        {
            REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_MS_BL_CFG, val);
        }
        else if (profile == LPWR_PRIV_BLOCKER_PROFILE_GR)
        {
            REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_GR_BL_CFG, val);
        }
    }
    else
    {
        if (profile == LPWR_PRIV_BLOCKER_PROFILE_MS)
        {
            REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_MS_WL_CFG, val);
        }
        else if (profile == LPWR_PRIV_BLOCKER_PROFILE_GR)
        {
            REG_WR32(FECS, LW_PGSP_PRIV_BLOCKER_GR_WL_CFG, val);
        }
    }
}

/*!
 * @brief Update the SEC2 Priv Blocker HW State
 */
void
s_lpwrPrivBlockerSec2ModeSet_TU10X
(
    LwU32 targetBlockerMode,
    LwU32 targetProfileMask
)
{
    LwU32                   val;

    // Read the Blocker Control register
    val = REG_RD32(FECS, LW_PSEC_PRIV_BLOCKER_CTRL);

    switch (targetBlockerMode)
    {
        case LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE:
        {
            val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                              _BLOCK_NONE,  val);
            break;
        }

        case LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL:
        {
            val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                              _BLOCK_ALL, val);
            break;
        }

        case LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION:
        {
            switch (targetProfileMask)
            {
                case (BIT(LPWR_PRIV_BLOCKER_PROFILE_GR)):
                {
                    val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                                      _BLOCK_GR, val);
                    break;
                }

                case (BIT(LPWR_PRIV_BLOCKER_PROFILE_MS)):
                {
                    val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                                      _BLOCK_MS, val);
                    break;
                }

                case (BIT(LPWR_PRIV_BLOCKER_PROFILE_GR) | BIT(LPWR_PRIV_BLOCKER_PROFILE_MS)):
                {
                    val = FLD_SET_DRF(_PSEC, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                                      _BLOCK_GR_MS, val);
                    break;
                }

                default:
                {
                    PMU_BREAKPOINT();
                }
            }
            break;
        }
    }

    REG_WR32_STALL(FECS, LW_PSEC_PRIV_BLOCKER_CTRL, val);
}

/*!
 * @brief Update the GSP Priv Blocker HW State
 */
void
s_lpwrPrivBlockerGspModeSet_TU10X
(
    LwU32 targetBlockerMode,
    LwU32 targetProfileMask
)
{
    LwU32                   val;

    // Read the Blocker Control register
    val = REG_RD32(FECS, LW_PGSP_PRIV_BLOCKER_CTRL);

    switch (targetBlockerMode)
    {
        case LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE:
        {
            val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                              _BLOCK_NONE,  val);
            break;
        }

        case LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL:
        {
            val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                              _BLOCK_ALL, val);
            break;
        }

        case LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION:
        {
            switch (targetProfileMask)
            {
                case (BIT(LPWR_PRIV_BLOCKER_PROFILE_GR)):
                {
                    val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                                      _BLOCK_GR, val);
                    break;
                }

                case (BIT(LPWR_PRIV_BLOCKER_PROFILE_MS)):
                {
                    val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                                      _BLOCK_MS, val);
                    break;
                }

                case (BIT(LPWR_PRIV_BLOCKER_PROFILE_GR) | BIT(LPWR_PRIV_BLOCKER_PROFILE_MS)):
                {
                    val = FLD_SET_DRF(_PGSP, _PRIV_BLOCKER_CTRL, _BLOCKMODE,
                                      _BLOCK_GR_MS, val);
                    break;
                }

                default:
                {
                    PMU_BREAKPOINT();
                }
            }
            break;
        }
    }

    REG_WR32_STALL(FECS, LW_PGSP_PRIV_BLOCKER_CTRL, val);
}

#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
/*!
 * @brief Get the XVE Blocker Interrupt Bit Ingore Mask
 */
LwU32
s_lpwrPrivBlockerXveIntrFieldIgnoreMaskGet_TU10X(void)
{
    LwU32 xveIntrFieldIgnoreMask = LW_U32_MAX;

    // Create the Interrupt bit's values which we should not touch
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _INTR_ACK, _INIT,
                                         xveIntrFieldIgnoreMask);
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _INTR_STAT,
                                         _NOT_PENDING, xveIntrFieldIgnoreMask);
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _INTR_OVERRIDE_VAL,
                                         _NOOP, xveIntrFieldIgnoreMask);

    return xveIntrFieldIgnoreMask;
}
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
