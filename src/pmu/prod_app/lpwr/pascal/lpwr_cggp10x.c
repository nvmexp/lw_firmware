/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cggp10x.c
 * @brief  LPWR Clock Gating related functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_cg.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// Defines to check the Alt path of clocks
#define LPWR_CG_SEL_VCO_GPC_CLK_OUT_BYPASS                               \
    FLD_SET_DRF(_PTRIM_GPC_BCAST, _SEL_VCO, _GPC2CLK_OUT, _BYPASS, 0)

#define LPWR_CG_SEL_VCO_XBAR_CLK_OUT_BYPASS                              \
    FLD_SET_DRF(_PTRIM_SYS, _SEL_VCO, _XBAR2CLK_OUT, _BYPASS, 0)

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

static void  s_lpwrCgGateAltPath_GP10X       (LwU32);
static void  s_lpwrCgUngateAltPath_GP10X     (LwU32);
static void  s_lpwrCgUpdateClkState_GP10X    (LwU32, LwBool);
static void  s_lpwrCgSwitchClkPath_GP10X     (LwU32, LwU8);
static LwU32 s_lpwrCgGetClkMaskAltPath_GP10X (LwU32);



/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @Brief : LPWR Clock Gating Powerdown API
 *
 * @Description : API handles clock Powering down operations. Powering down lwrrently
 *                includes 2 operations :
 *                      - VCO --> ALT switching
 *                      - ALT --> GATED switching
 *
 * @param[in] - clkMask    - Mask of clocks which need to be acted upon
 *              targetMode - Target modes for all clocks
 *                           targetMode can have the values:
 *                           LPWR_CG_TARGET_MODE_ALT  -> Ungate the clocks and
 *                                                     move them to alt path.
 *                           LPWR_CG_TARGET_MODE_GATED -> Gate the clocks.
 */
void
lpwrCgPowerdown_GP10X
(
    LwU32 clkMask,
    LwU8  targetMode
)
{
    LPWR_CG *pLpwrCg   = LPWR_GET_CG();
    LwU32    selVco    = 0;
    LwU32    gpcSelVco = 0;

    // Return early if LPWR CG is not supported
    if (pLpwrCg == NULL)
    {
        return;
    }

    if ((targetMode != LPWR_CG_TARGET_MODE_ALT) &&
        (targetMode != LPWR_CG_TARGET_MODE_GATED))
    {
        // Unsupported mode for this API.
        return;
    }

    // Mask out non supported clocks
    clkMask &= pLpwrCg->clkDomainSupportMask;

    // If no clocks are present return
    if (clkMask == 0)
    {
        return;
    }

    selVco    = REG_RD32(FECS, LW_PTRIM_SYS_STATUS_SEL_VCO);
    gpcSelVco = REG_RD32(FECS, LW_PTRIM_GPC_BCAST_STATUS_SEL_VCO);

    // Save the current state of clocks path
    if (!pLpwrCg->bCacheValid)
    {
        pLpwrCg->selVcoCache    = selVco;
        pLpwrCg->gpcSelVcoCache = gpcSelVco;
        pLpwrCg->bCacheValid    = LW_TRUE;
    }

    // Check if the clocks are on alt path or not
    if ((selVco != LPWR_CG_SEL_VCO_XBAR_CLK_OUT_BYPASS) ||
        (gpcSelVco != LPWR_CG_SEL_VCO_GPC_CLK_OUT_BYPASS))
    {
        //
        // Some of the clocks are on VCO path. So we need to move
        // them to ALT Path.
        //
        // Step 1: Ungate the Bypass path.
        // setp 2: move the all supported clocks to BYPASS path.
        //

        // Step 1: Ungate the Alt path
        s_lpwrCgUngateAltPath_GP10X(clkMask);

        // Step 2: Switch Clock to Alt path
        s_lpwrCgSwitchClkPath_GP10X(clkMask, LPWR_CG_TARGET_MODE_ALT);
    }

    if (targetMode == LPWR_CG_TARGET_MODE_GATED)
    {
        // Gate the clocks frm ALT path
        s_lpwrCgGateAltPath_GP10X(clkMask);
    }
}

/*!
 * @Brief : LPWR Clock Gating Powerup API
 *
 * @Description : API handles clock Ungate operations. Powerup lwrrently
 *                includes 2 operations :
 *                      - GATED --> ALT switching
 *                      - ALT   --> VCO switching
 *
 * @param[in] - clkMask     - Mask of clocks which need to be acted upon
 *              targetMode - Target modes for all clocks in clkMask
 *                           targetMode can have the values:
 *                           LPWR_CG_TARGET_MODE_ALT -> Unagte the clocks only.
 *                                                    no clock source switch
 *                                                    needed.
 *                           LPWR_CG_TARGET_MODE_ORG -> Ungate the clocks and
 *                                                    move them to original
 *                                                    clock source.
 *
 * NOTE:
 * if targetMode == LPWR_CG_TARGET_MODE_ORG, then it means we need to move clock
 * to original source. This source can be VCO path or ALT path. This will be
 * determined based on the cached value of selVco(LW_PTRIM_SYS_STATUS_SEL_VCO)
 * stored on pLpwrCg->selVcoCache variable and pPgCg->gpcSelVcoCache for GPC Clock.
 */
void
lpwrCgPowerup_GP10X
(
    LwU32  clkMask,
    LwU8   targetMode
)
{
    LPWR_CG *pLpwrCg = LPWR_GET_CG();

    // Return early if LPWR CG is not supported
    if (pLpwrCg == NULL)
    {
        return;
    }

    if ((targetMode != LPWR_CG_TARGET_MODE_ALT) &&
        (targetMode != LPWR_CG_TARGET_MODE_ORG))
    {
        // Unsupported mode for this API.
        return;
    }

    // Mask out non supported clocks
    clkMask &= pLpwrCg->clkDomainSupportMask;

    // If no clocks are present return
    if (clkMask == 0)
    {
        return;
    }

    // Ungate clocks for Alt path
    s_lpwrCgUngateAltPath_GP10X(clkMask);

    //
    // After the above step, all the clocks present in clkMask
    // are at ALT Path. If target mode == LPWR_CG_TARGET_MODE_ALT,
    // then we are done, there is no other operation needed.
    //

    //
    // If target mode == LPWR_CG_TARGET_MODE_ORG, move the clock to
    // their original source. Here Original source can be:
    // a. ALT Path
    // b. VCO Path
    // This will be determined based on the pLpwrCg->selVcoCache &
    // pLpwrCg->gpcSelVcoCache variables. These variables has the original
    // value of clock's source. Therfore, the clocks which were on Alt
    // path originally, we need to remove them from the clkMask.
    // This is done using the function s_lpwrCgGetClkMaskAltPath_GP10X.
    //
    // Once we have identified those clocks and removed them from the
    // mask, we just need to move the remaining clocks to VCO Path.
    //
    if (targetMode == LPWR_CG_TARGET_MODE_ORG)
    {
        //
        // Remove the clocks from mask which were on Alt path
        // originally by checking the pLpwrCg->selVcoCache & pPgCg->gpcSelVcoCache
        // This function will return the mask of clocks which
        // were on Alt path originally by checking the cache.
        //
        clkMask &= ~(s_lpwrCgGetClkMaskAltPath_GP10X(clkMask));

        // Switch the remaining clocks back to VCO Path
        s_lpwrCgSwitchClkPath_GP10X(clkMask, LPWR_CG_TARGET_MODE_VCO);

        // Gate the Alt Path for the clocks which are on VCO path
        s_lpwrCgGateAltPath_GP10X(clkMask);

        //
        // Since we have moved the clocks to original source, mark the cache
        // as invalid.
        //
        pLpwrCg->bCacheValid = LW_FALSE;
    }
}


/*!
 * @Brief : Gate the clocks from alt path.
 *
 * @param[in] - clkMask - Mask of clocks which need to be acted upon
 */
static void
s_lpwrCgGateAltPath_GP10X
(
    LwU32 clkMask
)
{
    //
    // Gate the clocks in following order:
    // GPC Clock
    // XBAR Clock
    //
    if (IS_CLK_SUPPORTED(GPC, clkMask))
    {
        s_lpwrCgUpdateClkState_GP10X(LW_PTRIM_SYS_GPC2CLK_ALT_SWITCH, LW_TRUE);
    }

    if (IS_CLK_SUPPORTED(XBAR, clkMask))
    {
        s_lpwrCgUpdateClkState_GP10X(LW_PTRIM_SYS_XBAR2CLK_ALT_SWITCH, LW_TRUE);
    }
}

/*!
 * @Brief : Ungate the clocks from alt path.
 *
 * @param[in] - clkMask    - Mask of clocks which need to be acted upon
 */
static void
s_lpwrCgUngateAltPath_GP10X
(
    LwU32  clkMask
)
{

    //
    // Unate the clocks in following order:
    // XBAR Clock
    // GPC Clock
    //
    // We need to ungate the Xbar clock first, because we need to access
    // GPC and LTC clusters. If Xbar clock is gated, we will not be able to
    // access them.
    //
    if (IS_CLK_SUPPORTED(XBAR, clkMask))
    {
        s_lpwrCgUpdateClkState_GP10X(LW_PTRIM_SYS_XBAR2CLK_ALT_SWITCH, LW_FALSE);
    }

    if (IS_CLK_SUPPORTED(GPC, clkMask))
    {
        s_lpwrCgUpdateClkState_GP10X(LW_PTRIM_SYS_GPC2CLK_ALT_SWITCH, LW_FALSE);
    }
}

/*!
 * @Brief : Update the clock state.
 * This API write the register to gate/ugate the clock
 *
 * @param[in] - regAddr - Address of the register which need to be
 *                        updated
 *              bGate   - LW_TRUE  -> Gate the clocks
 *                      - LW_FALSE -> Ungate the clocks
 */
static void
s_lpwrCgUpdateClkState_GP10X
(
    LwU32  regAddr,
    LwBool bGate
)
{
    LwU32 regVal    = 0;
    LwU32 regValNew = 0;

    regVal = REG_RD32(FECS, regAddr);
    if (bGate)
    {
        regValNew = FLD_SET_DRF(_PTRIM_SYS, _GPC2CLK_ALT_SWITCH,
                                _STOPCLK, _YES, regVal);
    }
    else
    {
        regValNew = FLD_SET_DRF(_PTRIM_SYS, _GPC2CLK_ALT_SWITCH,
                                _STOPCLK, _NO, regVal);
    }

    if (regVal != regValNew)
    {
        REG_WR32(FECS, regAddr, regValNew);

        // Read the register again to flush the register
        regVal = REG_RD32(FECS, regAddr);
    }
}


/*!
 * @Brief : switch the clocks between VCO and BYPASS path and vice versa.
 *
 * @param[in] - clkMask - Mask of clocks which need to be acted upon
 *              mode    - LPWR_CG_TARGET_MODE_ALT -> switch to bypass path
 *                        LPWR_CG_TARGET_MODE_VCO -> switch to vco path
 */
static void
s_lpwrCgSwitchClkPath_GP10X
(
    LwU32 clkMask,
    LwU8  mode
)
{

    LwU32 selVco       = REG_RD32(FECS, LW_PTRIM_SYS_SEL_VCO);
    LwU32 gpcSelVco    = REG_RD32(FECS, LW_PTRIM_GPC_BCAST_SEL_VCO);
    LwU32 selVcoNew    = selVco;
    LwU32 gpcSelVcoNew = gpcSelVco;

    PMU_HALT_COND((mode == LPWR_CG_TARGET_MODE_ALT) ||
               (mode == LPWR_CG_TARGET_MODE_VCO));

    if (IS_CLK_SUPPORTED(GPC, clkMask))
    {
        gpcSelVcoNew = (mode == LPWR_CG_TARGET_MODE_ALT) ?
                        FLD_SET_DRF(_PTRIM_GPC_BCAST, _SEL_VCO, _GPC2CLK_OUT,
                                    _BYPASS, gpcSelVcoNew) :
                        FLD_SET_DRF(_PTRIM_GPC_BCAST, _SEL_VCO, _GPC2CLK_OUT,
                                    _VCO, gpcSelVcoNew);
    }

    if (IS_CLK_SUPPORTED(XBAR, clkMask))
    {
        selVcoNew = (mode == LPWR_CG_TARGET_MODE_ALT) ?
                     FLD_SET_DRF(_PTRIM_SYS, _SEL_VCO, _XBAR2CLK_OUT,
                                 _BYPASS, selVcoNew) :
                     FLD_SET_DRF(_PTRIM_SYS, _SEL_VCO, _XBAR2CLK_OUT,
                                 _VCO, selVcoNew);
    }

    if (selVco != selVcoNew)
    {
        REG_WR32(FECS, LW_PTRIM_SYS_SEL_VCO, selVcoNew);

        //
        // Linear divider takes ~70 PLL clock cycles to switch clock source.
        // Bug 532489 Comment #204 OR CL4455814 for the details.
        // Poll LW_PTRIM_SYS_STATUS_SEL_VCO to confirm that value got reflected in LDIV
        //
        if (!PMU_REG32_POLL_NS(
                USE_FECS(LW_PTRIM_SYS_STATUS_SEL_VCO),                      // Address
                DRF_SHIFTMASK(LW_PTRIM_SYS_STATUS_SEL_VCO_XBAR2CLK_OUT),    // Mask
                selVcoNew,                                                  // Value
                LPWR_CG_CLK_SWITCH_TIMEOUT_NS,                              // Timeout
                PMU_REG_POLL_MODE_BAR0_EQ))                                 // Mode
        {
            PMU_HALT();
        }
    }

    if (gpcSelVco != gpcSelVcoNew)
    {
        REG_WR32(FECS, LW_PTRIM_GPC_BCAST_SEL_VCO, gpcSelVcoNew);

        //
        // Linear divider takes ~70 PLL clock cycles to switch clock source.
        // Bug 532489 Comment #204 OR CL4455814 for the details.
        // Poll LW_PTRIM_GPC_BCAST_STATUS_SEL_VCO to confirm that value got reflected in LDIV
        //
        if (!PMU_REG32_POLL_NS(
                USE_FECS(LW_PTRIM_GPC_BCAST_STATUS_SEL_VCO),                  // Address
                DRF_SHIFTMASK(LW_PTRIM_GPC_BCAST_STATUS_SEL_VCO_GPC2CLK_OUT), // Mask
                gpcSelVcoNew,                                                 // Value
                LPWR_CG_CLK_SWITCH_TIMEOUT_NS,                                // Timeout
                PMU_REG_POLL_MODE_BAR0_EQ))                                   // Mode
        {
            PMU_HALT();
        }
    }
}

/*!
 * @Brief : get the mask of clocks which are on alt path
 *
 * @Description : API returns the mask of clocks which are on
 *                Alt path from the clock cache.
 */
static LwU32
s_lpwrCgGetClkMaskAltPath_GP10X
(
    LwU32 orgClkMask
)
{
    LPWR_CG *pLpwrCg = LPWR_GET_CG();
    LwU32    clkMask = 0;

    if (IS_CLK_SUPPORTED(GPC, orgClkMask) &&
        FLD_TEST_DRF(_PTRIM_GPC_BCAST, _STATUS_SEL_VCO, _GPC2CLK_OUT,
                     _BYPASS, pLpwrCg->gpcSelVcoCache))
    {
        clkMask |= CLK_DOMAIN_MASK_GPC;
    }

    if (IS_CLK_SUPPORTED(XBAR, orgClkMask) &&
        FLD_TEST_DRF(_PTRIM_SYS, _STATUS_SEL_VCO, _XBAR2CLK_OUT,
                     _BYPASS, pLpwrCg->selVcoCache))
    {
        clkMask |= CLK_DOMAIN_MASK_XBAR;
    }

    return clkMask;
}

