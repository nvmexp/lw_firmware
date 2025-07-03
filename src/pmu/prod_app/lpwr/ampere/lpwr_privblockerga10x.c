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
 * @file   lpwrprivblockerga10x.c
 * @brief  LPWR Priv Blocker Control File
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
#include "dev_lw_xve.h"
#endif
#include "dev_disp.h"
#include "dev_pwr_pri.h"
#include "dev_sec_pri.h"
#include "dev_graphics_nobundle.h"
#include "dev_smcarb.h"
#include "priv_blocker_wl_bl_ranges_init.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_objic.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
static LwU32 s_lpwrPrivBlockerXveIntrFieldIgnoreMaskGet_GA10X(void);
#endif
/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Init and Enable the XVE Priv Blocker interrupts
 */
void
lpwrPrivBlockerXveIntrInit_GA10X(void)
{
    LPWR_PRIV_BLOCKER *pLpwrPrivBlocker = LPWR_GET_PRIV_BLOCKER();
    LwU32              regVal = 0;

    appTaskCriticalEnter();
    {
        regVal = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);

        // MS Profile Interrupt
        regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                             _XVE_BAR_BLOCKER, _ENABLED, regVal);
        // GR Profile Interrupt
        regVal = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN,
                             _XVE_BAR_GR_BLOCKER, _ENABLED, regVal);

        REG_WR32_STALL(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, regVal);

        // Cache the XVE MS PROFILE Interrupt enable Mask
        pLpwrPrivBlocker->gpio1IntrEnMask = FLD_SET_DRF(_CPWR,
                                                   _PMU_GPIO_1_INTR_RISING_EN,
                                                   _XVE_BAR_BLOCKER, _ENABLED,
                                                   pLpwrPrivBlocker->gpio1IntrEnMask);

        // Cache the XVE GR PRFOFILE Interrupt enable Mask
        pLpwrPrivBlocker->gpio1IntrEnMask = FLD_SET_DRF(_CPWR,
                                                   _PMU_GPIO_1_INTR_RISING_EN,
                                                   _XVE_BAR_GR_BLOCKER, _ENABLED,
                                                   pLpwrPrivBlocker->gpio1IntrEnMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief Process the XVE Bar Blocker intrrupt
 *
 * This function will process the two types of
 * XVE bar blocker interrupt
 * 1. GR_PROFILE interrupt
 * 2. MS_PROFILE interrupt
 *
 * These interrupt will be generated based on
 * the PROFILE enabled in the blocker.
 */
void
lpwrPrivBlockerXveIntrProcess_GA10X
(
    LwU32 intrStat
)
{
    // GR Profile Interrupt
    if (PENDING_IO_BANK1(_XVE_BAR_GR_BLOCKER, _RISING, intrStat))
    {
        // Set the Wakeup CtrlId and Wakeup Reason
        lpwrPrivBlockerWakeupMaskSet(lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_GR),
                                     PG_WAKEUP_EVENT_XVE_PRIV_BLOCKER);
    }

    // MS Profile Interrupt
    if (PENDING_IO_BANK1(_XVE_BAR_BLOCKER, _RISING, intrStat))
    {
        // Set the Wakeup CtrlId and Wakeup Reason
        lpwrPrivBlockerWakeupMaskSet(lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS),
                                    PG_WAKEUP_EVENT_XVE_PRIV_BLOCKER);
    }
}

/*!
 * @brief Update the XVE Priv Blocker HW State
 */
void
lpwrPrivBlockerXveModeSet_GA10X
(
    LwU32 targetBlockerMode,
    LwU32 targetProfileMask
)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
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
    //  acknowledgment and will pass the transaction which
    //  we do not want.
    //
    val = val & s_lpwrPrivBlockerXveIntrFieldIgnoreMaskGet_GA10X();

     // Set the Blocker Mode Overwrite Bit
    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _CTRL_BLOCKER_MODE_OVERWRITE,
                      _ENABLED, val);

    // Check for target blocker Mode
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

    // Check for target profile
    switch (targetProfileMask)
    {
        case LPWR_PRIV_BLOCKER_PROFILE_NONE:
        {
            // Disable both the profiles.
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _GR_PROFILE, _DISABLE, val);
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _MS_PROFILE, _DISABLE, val);
            break;
        }

        // Only GR Profile enabled
        case (BIT(LPWR_PRIV_BLOCKER_PROFILE_GR)):
        {
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _GR_PROFILE, _ENABLE, val);
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _MS_PROFILE, _DISABLE, val);
            break;
        }

        // Only MS Profile enabled
        case (BIT(LPWR_PRIV_BLOCKER_PROFILE_MS)):
        {
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _GR_PROFILE, _DISABLE, val);
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _MS_PROFILE, _ENABLE, val);
            break;
        }

        // Both GR and MS Profile enabled
        case (BIT(LPWR_PRIV_BLOCKER_PROFILE_GR) | BIT(LPWR_PRIV_BLOCKER_PROFILE_MS)):
        {
            // Enable both the profiles.
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _GR_PROFILE, _ENABLE, val);
            val = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _MS_PROFILE, _ENABLE, val);
            break;
        }
    }

    REG_WR32_STALL(FECS_CFG, LW_XVE_BAR_BLOCKER, val);
#endif
}

/*!
 * @brief Program and enable Allowlist Ranges for XVE Blocker MS Profile
 *
 * Allowlist allows us to access a register or a range which has been
 * disallowed in the XVE blocker by HW. This is essentially a CYA.
 */
void
lpwrPrivBlockerXveMsProfileAllowRangesInit_GA10X(void)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    LwU32 val;

    val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);

    // Part 1: Program the WL Ranges and enable them

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2) &&
        lpwrGrpCtrlIsSfSupported(RM_PMU_LPWR_GRP_CTRL_ID_MS,
                                 RM_PMU_LPWR_CTRL_FEATURE_ID_MS_SEC2))
    {
        //
        // When SEC2 RTOS is supported in the MSCG sequence, add LW_PSEC
        // registers into XVE BAR blocker Allowlist range. SEC2 RTOS will
        // make us of dedicated FB and priv blockers in order to wake us
        // up from MSCG. By disallowing them, we would end up waking MSCG
        // unnecessarily when, for example, a command is posted to SEC2,
        // that doesn't result in accesses to memory or other clock gated domains.
        //

        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(0),
                           DEVICE_BASE(LW_PSEC));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(0),
                           DEVICE_EXTENT(LW_PSEC));

        // Enable ALLOW_RANGE0
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE0, _ENABLED, val);
    }

    //
    // Bug: 200435216 requires below PDISP and UDISP ranges to be in Allowlist
    //      for Win7 aero off + OGL app which causes residency drop to 0.8%.
    //
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(1),
                       LW_PDISP_FE_RM_INTR_EN_HEAD_TIMING(0));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(1),
                       LW_PDISP_FE_RM_INTR_EN_OR);

    // Enable ALLOW_RANGE1
    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE1, _ENABLED, val);

    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(2),
                       DEVICE_BASE(LW_UDISP));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(2),
                       DEVICE_EXTENT(LW_UDISP));

    // Enable ALLOW_RANGE2
    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE2, _ENABLED, val);

    // Program the Allow Range settings into HW
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, val);
#endif
}

/*!
 * @brief Program the disallow access Ranges for XVE Blocker for GR Profile
 *
 * Disallow Range allows us to disallow access to a register or a range which has been
 * listed in Allowlist in the XVE blocker by HW. This is essentially a CYA.
 */
void
lpwrPrivBlockerXveGrProfileDisallowRangesInit_GA10X(void)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))

    LwU32 val;

    val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA);

    // Configure disallow access Range 0
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA_DISALLOW_RANGE_START_ADDR(0),
                       PRIV_BLOCKER_XVE_GR_BL_RANGE_START_0);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA_DISALLOW_RANGE_END_ADDR(0),
                       PRIV_BLOCKER_XVE_GR_BL_RANGE_END_0);

    // Enable DISALLOW_RANGE0
    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_GR_CYA, _DISALLOW_RANGE0, _ENABLED, val);

    // Program the Disallow Range settings into HW
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA, val);

#endif // (!PMU_PROFILE_GH20X)
}

/*!
 * @brief Program the Allow Ranges for XVE Blocker for GR Profile
 *
 * Allowlist Range allows us to access a register or a range which has been
 * Denied in the XVE blocker by HW. This is essentially a CYA.
 *
 * Following ranges are allowed access -
 *
 *      RANGE 0:
 *          LW_PGRAPH_PRI_FECS_LWRRENT_CTX
 *          LW_PGRAPH_PRI_FECS_NEW_CTX
 *
 *      RANGE 1:
 *          LW_PGRAPH_PRI_FECS_CTXSW_GFID
 */
void
lpwrPrivBlockerXveGrProfileAllowRangesInit_GA10X(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    // Configure WL Range 0
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA_ALLOW_RANGE_START_ADDR(0),
                       LW_PGRAPH_PRI_FECS_LWRRENT_CTX);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA_ALLOW_RANGE_END_ADDR(0),
                       LW_PGRAPH_PRI_FECS_NEW_CTX);

    // Configure WL Range 1
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA_ALLOW_RANGE_START_ADDR(1),
                       LW_PGRAPH_PRI_FECS_CTXSW_GFID);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA_ALLOW_RANGE_END_ADDR(1),
                       LW_PGRAPH_PRI_FECS_CTXSW_GFID);

    // Keep the ranges disabled by default
    lpwrPrivBlockerXveGrProfileAllowRangesEnable_HAL(&Lpwr, LW_FALSE);
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
}

/*!
 * @brief Enable/disable WL Ranges for XVE Blocker GR Profile
 *
 * @param[in]  bEnable     Enable/Disable WL ranges for GR profile
 */
void
lpwrPrivBlockerXveGrProfileAllowRangesEnable_GA10X(LwBool bEnable)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    LwU32 val;

    val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA);

    if (bEnable)
    {
        // Enable ALLOW_RANGE0 and RANGE1
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_GR_CYA, _ALLOW_RANGE0, _ENABLED, val);
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_GR_CYA, _ALLOW_RANGE1, _ENABLED, val);
    }
    else
    {
        // Disable ALLOW_RANGE0 and RANGE1
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_GR_CYA, _ALLOW_RANGE0, _DISABLED, val);
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_GR_CYA, _ALLOW_RANGE1, _DISABLED, val);
    }

    // Program the Allow Range settings into HW
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_CYA, val);
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
}

/*!
 * @brief Init and Enable the XVE Priv Blocker timeout
 */
void
lpwrPrivBlockerXveTimeoutInit_GA10X(void)
{
#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
    LwU32 msRegVal;
    LwU32 grRegVal;

    msRegVal = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_INTR_TIMEOUT);
    grRegVal = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_INTR_TIMEOUT);

    //
    // For emulation run, we should keep the timeout to max possible value.
    // Timeout field is 16 bit value, so max timeout is 0xFFFF. Emulation
    // runs are slow and we might hit timeout during LowPower entry. So we are
    // keeping the timeout to max possible value.
    //
    if (IsEmulation())
    {
        msRegVal = FLD_SET_DRF_NUM(_XVE, _BAR_BLOCKER_INTR_TIMEOUT, _VAL,
                                  LPWR_PRIV_BLOCKERS_XVE_TIMEOUT_EMU_US, msRegVal);
        grRegVal = FLD_SET_DRF_NUM(_XVE, _BAR_BLOCKER_GR_INTR_TIMEOUT, _VAL,
                                  LPWR_PRIV_BLOCKERS_XVE_TIMEOUT_EMU_US, grRegVal);

    }

    //
    // Enable the BAR Blocker interrupt timeout. If PMU SW is hosed with
    // GR/MS engaged for any reason (like a crash due to stack overrun or
    // a and breakpoint being hit), then this timeout will fire after _VAL
    // usec disengage the XVE blocker so that it does not become a fatal
    // error and hang the PCIe bus.
    //
    msRegVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_INTR_TIMEOUT, _EN, _ENABLED, msRegVal);
    grRegVal = FLD_SET_DRF(_XVE, _BAR_BLOCKER_GR_INTR_TIMEOUT, _EN, _ENABLED, grRegVal);

    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_INTR_TIMEOUT, msRegVal);
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_GR_INTR_TIMEOUT, grRegVal);
#endif
}

/* ------------------------ Private Functions  ------------------------------ */

#if (!(PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV))
/*!
 * @brief Get the XVE Blocker Interrupt Bit Ingore Mask
 */
LwU32
s_lpwrPrivBlockerXveIntrFieldIgnoreMaskGet_GA10X(void)
{
    LwU32 xveIntrFieldIgnoreMask = LW_U32_MAX;

    // Create the Interrupt bit's values which we should not touch
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _INTR_ACK, _INIT,
                                         xveIntrFieldIgnoreMask);
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _INTR_STAT,
                                         _NOT_PENDING, xveIntrFieldIgnoreMask);
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _INTR_OVERRIDE_VAL,
                                         _NOOP, xveIntrFieldIgnoreMask);
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _GR_INTR_ACK,
                                         _INIT, xveIntrFieldIgnoreMask);
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _GR_INTR_STAT,
                                         _NOT_PENDING, xveIntrFieldIgnoreMask);
    xveIntrFieldIgnoreMask = FLD_SET_DRF(_XVE, _BAR_BLOCKER, _GR_INTR_OVERRIDE_VAL,
                                         _NOOP, xveIntrFieldIgnoreMask);
    return xveIntrFieldIgnoreMask;
}
#endif
