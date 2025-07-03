/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq_35_daemon.c
 * @copydoc changeseq_35_daemon.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "perf/perf_daemon.h"
#include "task_perf_daemon.h"
#include "clk3/clk3.h"
#include "clk/clk_clockmon.h"
#include "task_perf.h"
#include "dev_pwr_csb.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS s_perfDaemonChangeSeqBuildActiveClkDomainsMask(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqBuildActiveClkDomainsMask");
static FLCN_STATUS s_perfDaemonChangeSeqIsXbarBoostReq(CHANGE_SEQ *pChangeSeq, CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeScratch, LwBool *pBIsXbarBoostReq, LwBool *pBIsXbarBoostPre)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "s_perfDaemonChangeSeqIsXbarBoostReq");

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc PerfDaemonChangeSeqScriptInit()
 * @memberof CHANGE_SEQ_35
 * @public
 */
FLCN_STATUS
perfDaemonChangeSeqScriptInit_35
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    CHANGE_SEQ_35                      *pChangeSeq35      = (CHANGE_SEQ_35 *)pChangeSeq;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeScratch    = pChangeSeq->pChangeScratch;
    LwBool                              bIsPstateChange   = (pChangeLwrr->pstateIndex != pChangeLast->pstateIndex);
    LwBool                              bIsXbarBoostReq   = LW_FALSE;
    LwBool                              bIsXbarBoostPre   = LW_FALSE;
    LwBool                              bIsMemTuneEnabled = LW_FALSE;
    FLCN_STATUS                         status            = FLCN_OK;
    LwU8                                i;

    // Take the clock VF points semaphore
    perfReadSemaphoreTake();

    //
    // XBAR boost only required for MCLK switch which is assumed to be 1-1 mapped
    // with PState.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
    {
        // Determine whether XBAR Boost required.
        status = s_perfDaemonChangeSeqIsXbarBoostReq(pChangeSeq,
                                                     pScript,
                                                     pChangeLwrr,
                                                     pChangeLast,
                                                     pChangeScratch,
                                                     &bIsXbarBoostReq,
                                                     &bIsXbarBoostPre);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptInit_35_exit;
        }
    }

    // Enable MEM_TUNE if memory clock is programmable.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
    {
        CLK_DOMAIN *pDomain;

        pDomain = clkDomainsGetByApiDomain(LW2080_CTRL_CLK_DOMAIN_MCLK);
        if (pDomain == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptInit_35_exit;
        }

        bIsMemTuneEnabled = (!clkDomainIsFixed(pDomain));
    }
    (void)bIsMemTuneEnabled; // To avoid unused variable error.

    //
    // The order of steps built/exelwted in P-states 3.5. Steps may be excluded
    // if the feature is disabled or is not necessary (e.g. P-state steps are
    // omitted when a P-state change is not happening).
    //
    struct
    {
        const LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID stepId;
        const LwBool bInclude;
        const LwBool bIntermediateStep;
        const LwBool bIntermediateStepPre;
    } steps[] =
    {
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_RM,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM),
            LW_FALSE,
            LW_FALSE
        },
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_PMU,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU),
            LW_FALSE,
            LW_FALSE
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_RM,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_RM) && bIsPstateChange),
            LW_FALSE,
            LW_FALSE
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_PMU,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_PMU) && bIsPstateChange),
            LW_FALSE,
            LW_FALSE
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_LPWR,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_LPWR) && bIsPstateChange),
            LW_FALSE,
            LW_FALSE
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_BIF,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_BIF) && bIsPstateChange),
            LW_FALSE,
            LW_FALSE
        },
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_PRE_VOLT_CLKS,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS) && bIsXbarBoostReq && bIsXbarBoostPre),
            bIsXbarBoostReq,
            bIsXbarBoostPre
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_XBAR_BOOST_VOLT,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_VOLT) && bIsXbarBoostReq && bIsXbarBoostPre),
            bIsXbarBoostReq,
            bIsXbarBoostPre
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_POST_VOLT_CLKS,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS) && bIsXbarBoostReq && bIsXbarBoostPre),
            bIsXbarBoostReq,
            bIsXbarBoostPre
        },
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_NAFLL_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS),
            LW_FALSE,
            LW_FALSE
        },
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS),
            bIsXbarBoostReq,
            (!bIsXbarBoostPre)
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_VOLT,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_VOLT),
            bIsXbarBoostReq,
            (!bIsXbarBoostPre)
        },
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_NAFLL_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS),
            LW_FALSE,
            LW_FALSE
        },
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_CLKS,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS),
            bIsXbarBoostReq,
            (!bIsXbarBoostPre)
        },
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_PRE_VOLT_CLKS,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS) && bIsXbarBoostReq && (!bIsXbarBoostPre)),
            bIsXbarBoostReq,
            bIsXbarBoostPre
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_XBAR_BOOST_VOLT,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_VOLT) && bIsXbarBoostReq && (!bIsXbarBoostPre)),
            bIsXbarBoostReq,
            bIsXbarBoostPre
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_POST_VOLT_CLKS,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS) && bIsXbarBoostReq && (!bIsXbarBoostPre)),
            bIsXbarBoostReq,
            bIsXbarBoostPre
        },
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_CLK_MON,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON) && clkDomainsIsClkMonEnabled(),
            LW_FALSE,
            LW_FALSE
        },
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_MEM_TUNE,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE) && bIsMemTuneEnabled,
            LW_FALSE,
            LW_FALSE
        },
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_PMU,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_PMU) && bIsPstateChange),
            LW_FALSE,
            LW_FALSE
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_RM,
            (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_PSTATE_RM) && bIsPstateChange),
            LW_FALSE,
            LW_FALSE
        },
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_PMU,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_PMU),
            LW_FALSE,
            LW_FALSE
        },
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM)
        {
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_RM,
            PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM),
            LW_FALSE,
            LW_FALSE
        }
#endif  // #if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_CHANGE_RM)
    };

    //
    // Sanity check the VF lwrve change counter to determine whether VF data
    // is stale. If so, discard the change as there will be new change. If
    // there is no change in VF lwrve, hold the lock for complete perf change
    // sequence script initialization.
    //
    if ((pChangeLwrr->vfPointsCacheCounter !=
            clkVfPointsVfPointsCacheCounterGet()) &&
        (pChangeLwrr->vfPointsCacheCounter !=
            LW2080_CTRL_CLK_CLK_VF_POINT_CACHE_COUNTER_TOOLS))
    {
        status = FLCN_WARN_NOTHING_TO_DO;

        // Discard the change and release the semaphore.
        goto perfDaemonChangeSeqScriptInit_35_exit;
    }

    // Build active mask
    status = s_perfDaemonChangeSeqBuildActiveClkDomainsMask(pChangeSeq, pScript);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptInit_35_exit;
    }

    // Validate the input change request
    status = perfDaemonChangeSeqValidateChange(pChangeSeq, pChangeLwrr);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptInit_35_exit;
    }

    // Build the script
    for (i = 0; i < LW_ARRAY_ELEMENTS(steps); ++i)
    {
        if (steps[i].bInclude)
        {
            if (steps[i].bIntermediateStep)
            {
                // In pre intermediate step, switch from last -> intermediate
                if (steps[i].bIntermediateStepPre)
                {
                    status = perfDaemonChangeSeqScriptBuildAndInsert(
                        &pChangeSeq35->super, pScript, pChangeScratch, pChangeLast, steps[i].stepId);
                }
                // In post intermediate step, switch from intermediate -> current
                else
                {
                    status = perfDaemonChangeSeqScriptBuildAndInsert(
                        &pChangeSeq35->super, pScript, pChangeLwrr, pChangeScratch, steps[i].stepId);
                }
            }
            else
            {
                status = perfDaemonChangeSeqScriptBuildAndInsert(
                    &pChangeSeq35->super, pScript, pChangeLwrr, pChangeLast, steps[i].stepId);
            }
            if (status != FLCN_OK)
            {
                goto perfDaemonChangeSeqScriptInit_35_exit;
            }
        }
    }

perfDaemonChangeSeqScriptInit_35_exit:
    perfReadSemaphoreGive();
    return status;
}

/*!
 * @copydoc PerfDaemonChangeSeqScriptExelwteStep()
 * @memberof CHANGE_SEQ_35
 * @public
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_35_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    FLCN_STATUS status = FLCN_OK;

    // Call the super class implementation.
    status = perfDaemonChangeSeqScriptExelwteStep_SUPER(
                                                    pChangeSeq,
                                                    pStepSuper);
    // Execute class specific version steps.
    if (status == FLCN_ERR_ILWALID_VERSION)
    {
        switch (pStepSuper->stepId)
        {
            case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_CLKS:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS))
                {
                    status = perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS(
                                                                    pChangeSeq,
                                                                    pStepSuper);
                }

                break;
            }
            case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_CLKS:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_CLKS))
                {
                    status = perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS(
                                                                    pChangeSeq,
                                                                    pStepSuper);
                }
                break;
            }
            case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_PRE_VOLT_CLKS:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
                {
                    status = perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS(
                                                                    pChangeSeq,
                                                                    pStepSuper);
                }

                break;
            }
            case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_XBAR_BOOST_POST_VOLT_CLKS:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK))
                {
                    status = perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS(
                                                                    pChangeSeq,
                                                                    pStepSuper);
                }
                break;
            }
            case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_PRE_VOLT_NAFLL_CLKS:
            case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_POST_VOLT_NAFLL_CLKS:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS))
                {
                    status = perfDaemonChangeSeq35ScriptExelwteStep_NAFLL_CLKS(
                                                                    pChangeSeq,
                                                                    pStepSuper);
                }
                break;
            }
            case LW2080_CTRL_PERF_CHANGE_SEQ_35_STEP_ID_CLK_MON:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_CLK_MON))
                {
                    status = perfDaemonChangeSeq35ScriptExelwteStep_CLK_MON(
                                                                    pChangeSeq,
                                                                    pStepSuper);
                }
                break;
            }
            default:
            {
                // Invalid step.
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                break;
            }
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptExelwteStep_35_exit;
    }

perfDaemonChangeSeqScriptExelwteStep_35_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */

/*!
 * @brief Builds the active clock domain mask for the change sequencer script
 * exelwtion.
 * @memberof CHANGE_SEQ_35
 * @private
 *
 * @param[in]   pChangeSeq      CHANGE_SEQ pointer
 * @param[in]   pScript         CHANGE_SEQ_SCRIPT pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
static FLCN_STATUS
s_perfDaemonChangeSeqBuildActiveClkDomainsMask
(
    CHANGE_SEQ         *pChangeSeq,
    CHANGE_SEQ_SCRIPT  *pScript
)
{
    FLCN_STATUS status = FLCN_OK;

    // Set active mask equal to exclusion clock domain mask.
    status = boardObjGrpMaskCopy(&pScript->clkDomainsActiveMask,
                                 &pChangeSeq->clkDomainsExclusionMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqBuildActiveClkDomainsMask_exit;
    }

    // Ilwert the active mask to clear the excluded clock domains.
    status = boardObjGrpMaskIlw(&pScript->clkDomainsActiveMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqBuildActiveClkDomainsMask_exit;
    }

    // Clear the non-programmable clock domains.
    status = boardObjGrpMaskAnd(&pScript->clkDomainsActiveMask,
                                &pScript->clkDomainsActiveMask,
                                &Clk.domains.progDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqBuildActiveClkDomainsMask_exit;
    }

    // Include fixed clock domains on client's request.
    status = boardObjGrpMaskOr(&pScript->clkDomainsActiveMask,
                               &pScript->clkDomainsActiveMask,
                               &pChangeSeq->clkDomainsInclusionMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqBuildActiveClkDomainsMask_exit;
    }

s_perfDaemonChangeSeqBuildActiveClkDomainsMask_exit:
    return status;
}

/*!
 * @brief Helper interface to determine whether XBAR boost required
 *        around MCLK switch.
 *
 * @memberof CHANGE_SEQ_35
 * @pprivate
 *
 * @param[in]  pChangeSeq       CHANGE_SEQ pointer.
 * @param[in]  pScript          Pointer to CHANGE_SEQ_SCRIPT buffer.
 * @param[in]  pChangeLwrr      Pointer to current LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
 * @param[in]  pChangeLast      Pointer to last completed LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
 * @param[out] pChangeScratch   Pointer to intermediate LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE.
 * @param[out] pBIsXbarBoostReq Pointer to boolean tracking whether intermediate boost required.
 * @param[out] pBIsXbarBoostPre Pointer to boolean tracking whether XBAR boost steps before VF switch steps.
 *
 * More documentation on the algorithm
 * CASE 1 : MCLK increasing
 *     Build
 *         Update the intermediate change with last completed change
 *             *(pChangeScratch) = *(pChangeLwrr);
 *         Callwlated the XBARb = function(MCLKLwrr);
 *         Update the XBAR in pChangeScratch to XBARb
 *         Update voltage  in pChangeScratch if needs to be increased based on XBARb
 *     Execute
 *         In this execute sequence MCLK switch will be done as part of the XBAR boost
 *         VF switch sequence. Also note that all other clocks may go into FR from VR
 *         during the XBAR_BOOST VF switch sequence as we are increasing the voltage
 *         but keeping the clocks as it is.
 *
 *     PRE_VOLT (pChangeLast -> pChangeScratch)
 *         If VR / FR -> Relax XBAR -> XBARb
 *         If FFR -> NOP
 *     VOLT (pChangeLast -> pChangeScratch)
 *         Increase or keep constant voltage
 *     POST_VOLT (pChangeLast -> pChangeScratch)
 *         If VR / FR -> Target XBAR = XBARb
 *         If FFR -> Increase XBAR -> XBARb
 *         Increase MCLK
 *     XBAR_BOOST_PRE_VOLT (pChangeScratch -> pChangeLwrr)
 *         If VR / FR -> Relax XBAR -> XBARb
 *         If FFR -> if decreasing, XBAR = Target XBAR else NOP
 *     XBAR_BOOST_VOLT (pChangeScratch -> pChangeLwrr)
 *         Decrease or keep constant voltage
 *     XBAR_BOOST_POST_VOLT (pChangeScratch -> pChangeLwrr)
 *         If VR / FR -> Target XBAR
 *         If FFR -> if increasing, XBAR = Target XBAR else NOP
 *
 *
 * CASE 2 : MCLK decreasing
 *     Build
 *         Update the intermediate change with last completed change
 *             *(pChangeScratch) = *(pChangeLast);
 *         Callwlated the XBARb = function(MCLKLwrr);
 *         Update the XBAR in pChangeScratch to XBARb
 *         Update voltage in pChangeScratch if needs to be increased based on XBARb
 *     Execute
 *         In this execute sequence MCLK switch will be done as part of the regular VF
 *         switch sequence. Also note that all other clocks may go into FR from VR during
 *         the XBAR_BOOST VF switch sequence as we are increasing the voltage but keeping
 *         the clocks as it is.
 *
 *     XBAR_BOOST_PRE_VOLT (pChangeLast -> pChangeScratch)
 *         If VR / FR -> Relax XBAR -> XBARb
 *         If FFR -> NOP
 *     XBAR_BOOST_VOLT (pChangeLast -> pChangeScratch)
 *         Increase or keep constant voltage
 *     XBAR_BOOST_POST_VOLT (pChangeLast -> pChangeScratch)
 *         If VR / FR -> Target XBAR = XBARb
 *         If FFR -> Increase XBAR -> XBARb
 *     PRE_VOLT (pChangeScratch -> pChangeLwrr)
 *         Decrease MCLK
 *         If VR / FR -> Relax XBAR -> XBARb
 *         If FFR -> if decreasing, XBAR = Target XBAR else NOP
 *     VOLT (pChangeScratch -> pChangeLwrr)
 *         Decrease or keep constant voltage
 *     POST_VOLT (pChangeScratch -> pChangeLwrr)
 *         If VR / FR -> Target XBAR
 *         If FFR -> if increasing, XBAR = Target XBAR else NOP
 *
 * @return LW_TRUE  If XBAR boost is required
 * @return LW_FALSE If XBAR boost is not required
 */
static LwBool
s_perfDaemonChangeSeqIsXbarBoostReq
(
    CHANGE_SEQ                          *pChangeSeq,
    CHANGE_SEQ_SCRIPT                   *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE  *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE  *pChangeLast,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE  *pChangeScratch,
    LwBool                              *pBIsXbarBoostReq,
    LwBool                              *pBIsXbarBoostPre
)
{
    LwU16       lwrrFreqMHzMclk;
    LwU16       lwrrFreqMHzXclk;
    LwU16       minFreqMHzXclk;
    LwU32       xclkDomainIdx;
    LwU32       mclkDomainIdx;
    FLCN_STATUS status;

    RM_PMU_PERF_VFE_VAR_VALUE   vfeVarVal;
    RM_PMU_PERF_VFE_EQU_RESULT  result;

    // Init
    (*pBIsXbarBoostReq) = LW_FALSE;
    (*pBIsXbarBoostPre) = LW_FALSE;

    // If VFE is not specied, feature is disabled.
    if (clkDomainsXbarBoostVfeIdxGet() == LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        status = FLCN_OK;
        goto s_perfDaemonChangeSeqIsXbarBoostReq_exit;
    }

    // Get current memory clock value.
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_MCLK, &mclkDomainIdx),
        s_perfDaemonChangeSeqIsXbarBoostReq_exit);

    // Validate the domain
    PMU_ASSERT_OK_OR_GOTO(status,
        (LW2080_CTRL_CLK_DOMAIN_MCLK != pChangeLwrr->clkList.clkDomains[mclkDomainIdx].clkDomain) ?
            FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfDaemonChangeSeqIsXbarBoostReq_exit);

    // Confirm MCLK is changing. If no change, bail out.
    if (pChangeLwrr->clkList.clkDomains[mclkDomainIdx].clkFreqKHz ==
        pChangeLast->clkList.clkDomains[mclkDomainIdx].clkFreqKHz)
    {
        status = FLCN_OK;
        goto s_perfDaemonChangeSeqIsXbarBoostReq_exit;
    }

    // Always use the target MCLK frequency as XBAR requirement is dependent only on it.
    lwrrFreqMHzMclk = (pChangeLwrr->clkList.clkDomains[mclkDomainIdx].clkFreqKHz / 1000U);

    // Get current XBAR clock value.
    PMU_ASSERT_OK_OR_GOTO(status,
        clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_XBARCLK, (&xclkDomainIdx)),
        s_perfDaemonChangeSeqIsXbarBoostReq_exit);

    // Validate the domain and do NOT clobber the @ref xclkDomainIdx
    PMU_ASSERT_OK_OR_GOTO(status,
        (LW2080_CTRL_CLK_DOMAIN_XBARCLK != pChangeLwrr->clkList.clkDomains[xclkDomainIdx].clkDomain) ?
            FLCN_ERR_ILWALID_STATE : FLCN_OK,
        s_perfDaemonChangeSeqIsXbarBoostReq_exit);

    lwrrFreqMHzXclk = (pChangeLwrr->clkList.clkDomains[xclkDomainIdx].clkFreqKHz / 1000U);

    // Get min XBAR required for current memory clock value.
    vfeVarVal.frequency.varType      = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY;
    vfeVarVal.frequency.varValue     = lwrrFreqMHzMclk;
    vfeVarVal.frequency.clkDomainIdx = mclkDomainIdx;

    status = vfeEquEvaluate(
        VFE_GET_PMU_IDX_FROM_BOARDOBJIDX(clkDomainsXbarBoostVfeIdxGet()),   // vfeEquIdx
        &vfeVarVal,                                                         // pValues
        1,                                                                  // valCount
        LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ,                      // outputType
        &result);                                                           // pResult
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfDaemonChangeSeqIsXbarBoostReq_exit;
    }
    minFreqMHzXclk = (LwU16)result.freqMHz;

    // XBAR boost is required if current XBAR is less than min XBAR.
    (*pBIsXbarBoostReq) = (lwrrFreqMHzXclk < minFreqMHzXclk);

    // If Xbar Boost required, build intermediate perf change struct.
    if (*pBIsXbarBoostReq)
    {
        LwU8 railIdx;

        // Validate the buffer for intermediate change struct.
        PMU_ASSERT_OK_OR_GOTO(status,
            (pChangeScratch == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
            s_perfDaemonChangeSeqIsXbarBoostReq_exit);

        // If increasing, use current else last change data
        if (pChangeLwrr->clkList.clkDomains[mclkDomainIdx].clkFreqKHz >
            pChangeLast->clkList.clkDomains[mclkDomainIdx].clkFreqKHz)
        {
            // Update the intermediate change with current change
            *(pChangeScratch)   = *(pChangeLwrr);

            // Update the pre vs post sequence flag.
            (*pBIsXbarBoostPre) = LW_FALSE;
        }
        else
        {
            // Update the intermediate change with last change.
            *(pChangeScratch)   = *(pChangeLast);

            // Update the pre vs post sequence flag.
            (*pBIsXbarBoostPre) = LW_TRUE;
        }

        // Update the intermediate boost XBAR frequency
        pChangeScratch->clkList.clkDomains[xclkDomainIdx].clkFreqKHz = (minFreqMHzXclk * 1000U);

        // Update the intermediate voltage to meet boost XBAR frequency
        for (railIdx = 0; railIdx < pChangeScratch->voltList.numRails; railIdx++)
        {
            CLK_DOMAIN_PROG *pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(CLK_DOMAIN_GET(xclkDomainIdx), PROG);
            PERF_LIMITS_VF   vfDomain;
            PERF_LIMITS_VF   vfRail;

            // Skip the voltage rail if there is no dependency of XBAR.
            if (!boardObjGrpMaskBitGet(CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK_GET(pDomainProg), railIdx))
            {
                continue;
            }
            ((void)(pDomainProg));  // To skip unused variable error when feature is not supported.

            vfDomain.idx   = xclkDomainIdx;
            vfDomain.value = pChangeScratch->clkList.clkDomains[xclkDomainIdx].clkFreqKHz;
            vfRail.idx     = railIdx;

            PMU_ASSERT_OK_OR_GOTO(status,
                perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
                s_perfDaemonChangeSeqIsXbarBoostReq_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                (vfRail.value == 0U) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
                s_perfDaemonChangeSeqIsXbarBoostReq_exit);

            pChangeScratch->voltList.rails[railIdx].voltageuV                =
                LW_MAX(pChangeScratch->voltList.rails[railIdx].voltageuV, vfRail.value);

            // Must also raise the noise unaware Vmin to ensure the HW features do not drop it.
            pChangeScratch->voltList.rails[railIdx].voltageMinNoiseUnawareuV =
                LW_MAX(pChangeScratch->voltList.rails[railIdx].voltageMinNoiseUnawareuV, vfRail.value);
        }
    }

s_perfDaemonChangeSeqIsXbarBoostReq_exit:
    return status;
}
