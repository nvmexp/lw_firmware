/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @brief   Manages handling of MclkFreqDomain
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Chandrabhanu Mahapatra
 */

/* ------------------------- System Includes -------------------------------- */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#endif

/* ------------------------- Application Includes --------------------------- */

// The manuals
#include "clk3/generic_dev_trim.h"

#include "clk3/dom/clkfreqdomain_mclk.h"

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmu_objclk.h"
#include "clk/pmu_clkfbflcn.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"
#endif                      // CLK_SD_CHECK


/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

//! The virtual table
CLK_DEFINE_VTABLE_CLK3__DOMAIN(MclkFreqDomain);

//! Mux value map used for main multiplexer
const ClkFieldValue clkMuxValueMap_MclkFreqDomain[CLK_INPUT_COUNT__MCLK]
    GCC_ATTRIB_SECTION("dmem_clk3x", "_clkMuxValueMap_MclkFreqDomain") =
{
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_DDR_SUPPORTED)
#if                     defined(LW_PFB_FBPA_FBIO_CTRL_SER_PRIV)     // GA10x and later
    [CLK_INPUT_BYPASS__MCLK]  = CLK_DRF__FIELDVALUE(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _ONESOURCE),  //  0  dramclk_mode=0   ONESOURCE
    [CLK_INPUT_REFMPLL__MCLK] = CLK_DRF__FIELDVALUE(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _REFMPLL),    //  1  dramclk_mode=1   REFMPLL
    [CLK_INPUT_DRAMPLL__MCLK] = CLK_DRF__FIELDVALUE(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _DRAMPLL),    //  2  dramclk_mode=2   DRAMPLL
#elif                   defined(LW_PTRIM_SYS_FBIO_MODE_SWITCH)      // TU10x
    [CLK_INPUT_BYPASS__MCLK]  = CLK_DRF__FIELDVALUE(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _ONESOURCE),  //  0  dramclk_mode=0   ONESOURCE
    [CLK_INPUT_REFMPLL__MCLK] = CLK_DRF__FIELDVALUE(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL),    //  1  dramclk_mode=1   REFMPLL
    [CLK_INPUT_DRAMPLL__MCLK] = CLK_DRF__FIELDVALUE(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _DRAMPLL),    //  2  dramclk_mode=2   DRAMPLL
#else
#error Required: At least one of the MODE_SWITCH registers should be supported on DDR chips
#endif
#elif PMUCFG_FEATURE_ENABLED(PMU_CLK_MCLK_HBM_SUPPORTED)
    [CLK_INPUT_BYPASS__MCLK]  = CLK_DRF__FIELDVALUE(_PFB, _FBPA_HBMPLL_CFG, _BYPASSPLL, _BYPASSCLK),            //  0  bypasspll=1    BYPASSCLK
    [CLK_INPUT_HBMPLL__MCLK]  = CLK_DRF__FIELDVALUE(_PFB, _FBPA_HBMPLL_CFG, _BYPASSPLL, _VCO),                  //  1  bypasspll=0    VCO
#else
#error Required: At least one PMUCFG feature to specify the supported memory type(s)
#endif
};


/* ------------------------- Macros and Defines ----------------------------- */

//TODO: review step size with Stefan Scotzniovsky
#define CLK_STEPSIZE__MCLK              4000    // 4MHz

#define CLK_TARGET_MARGIN__MCLK          500    // 500 KHz

#define MCLK_WITHIN_MARGIN(freqTarget, freqLwrrent, margin)     \
    ((freqTarget < (freqLwrrent + margin)) && (freqTarget >= (freqLwrrent - margin)))

#define WITHIN_MARGIN_MCLK_FORCE                                        \
    (FLD_TEST_DRF(_RM_PMU, _CLK_INIT_FLAGS, _SWITCH_WITHIN_MARGIN_MCLK, _FORCE, Clk.freqDomainGrp.initFlags))


/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Virtual Implemenations ------------------------- */

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED)

/*!
 * @brief   Configure the clkmclkfreqdomain to produce the target frequency
 */
FLCN_STATUS
clkConfig_MclkFreqDomain
(
    ClkMclkFreqDomain       *this,
    const ClkTargetSignal   *pTarget
)
{
    //
    // Switch only if we're already not producing the required signal unless
    // forced MCLK switching is enabled by the RMClkSwitchWithinMargin regkey.
    //
    // Phase 0: the current hardware state
    // Phase 1: the target hardware state
    //
    // this->targetFreqKHz reflects the previous configured frequency until
    // updated with current target frequency below.
    //
    // Due to dependency on fbflcn, the output frequency cannot be configured
    // and as such is known only after programming. As such, comparison of
    // configured output with current output cannot be done unlike other clock
    // domains. Thus, applied tighter margins below for better results.
    //
    if (WITHIN_MARGIN_MCLK_FORCE                                ||
       (!MCLK_WITHIN_MARGIN(pTarget->super.freqKHz,
                            CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz,
                            CLK_STEPSIZE__MCLK / 2)             &&
        !MCLK_WITHIN_MARGIN(pTarget->super.freqKHz,
                            this->targetFreqKHz,
                            CLK_TARGET_MARGIN__MCLK)))
    {
        this->super.phaseCount = 2;
        this->targetFreqKHz = pTarget->super.freqKHz;
    }
    else
    {
        // Do nothing
        this->super.phaseCount = 1;
    }

    // Done
    return FLCN_OK;
}

/*!
 * @brief   Program the clkmclkfreqdomain to produce the target frequency
 */
FLCN_STATUS
clkProgram_MclkFreqDomain
(
    ClkMclkFreqDomain *this
)
{
    RM_PMU_STRUCT_CLK_MCLK_SWITCH           mclkSwitchRequest;
    PMU_RM_RPC_STRUCT_CLK_MCLK_SWITCH_ACK   rpc;
    FLCN_STATUS                             status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkReadPerfDaemon)
    };

    //
    // If one of these assertions fail, it may mean that clkRead and clkConfig
    // were not successfully called since these function set 'phaseCount'.
    //
    PMU_HALT_COND(this->super.phaseCount);
    PMU_HALT_COND(this->super.phaseCount <= CLK_MAX_PHASE_COUNT);

    // clkConfig sets 'phaseCount = 1' to indicate that there is nothing to do.
    if (this->super.phaseCount == 1)
    {
        return FLCN_OK;
    }

    //
    // Ilwalidate the cache frequency since we are unaware of the exact
    // frequency that FB falcon will program.
    //
    CLK_OUTPUT_WRITE_BUFFER__FREQDOMAIN(&this->super).freqKHz = 0;

    //
    // TODO: Generic preparations required for an MCLK switch to execute i.e.
    // pre MClk switch settings (bug 200332117)
    //

    // call fbflcn to switch mclk
    memset(&mclkSwitchRequest, 0x00, sizeof(mclkSwitchRequest));
    mclkSwitchRequest.targetFreqKHz = this->targetFreqKHz;

    memset(&rpc, 0x00, sizeof(rpc));
    status = clkTriggerMClkSwitchOnFbflcn(&mclkSwitchRequest,
                                          &rpc.mClkSwitchAck.fbStopTimeUs,
                                          &rpc.mClkSwitchAck.status);
    this->fbStopTimeUs = rpc.mClkSwitchAck.fbStopTimeUs;

    if (status != FLCN_OK)
    {
        return status;
    }

    // Check if we need to report fb stop time back to RM or not
    if (FLD_TEST_DRF(_RM_PMU, _CLK_INIT_FLAGS, _REPORT_FB_STOP_TIME, _YES, Clk.freqDomainGrp.initFlags))
    {
        // Report fbStopTime to rm
        PMU_RM_RPC_EXELWTE_NON_BLOCKING(status, CLK, MCLK_SWITCH_ACK, &rpc);
    }

    // Initialize output freq by reading directly from hardware
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkRead_ProgrammableFreqDomain(&this->super);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        return status;
    }

    //
    // TODO: Restore the generic settings i.e. post MClk switch settings.
    // ( http://lwbugs/200332117 )
    //
    return status;
}

#endif                      // PMU_CLK_PROGRAMMABLE_MCLK_SUPPORTED
#endif                      // CLK_SD_CHECK

