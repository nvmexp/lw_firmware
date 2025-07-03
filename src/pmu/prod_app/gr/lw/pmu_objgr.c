/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objgr.c
 * @brief  Graphics object providing gr related methods
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objgr.h"
#include "pmu_objperf.h"
#include "dbgprintf.h"
#include "main.h"
#include "main_init_api.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#ifdef DMEM_VA_SUPPORTED
OBJGR Gr
    GCC_ATTRIB_SECTION("dmem_lpwr", "Gr");
#else
    OBJGR Gr;
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG)
LwrtosSemaphoreHandle GrRgSemaphore;
#endif

/* ------------------------ Prototypes --------------------------------------*/
static FLCN_STATUS s_grPgEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_grPgExit(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_grPgReset(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_grRgEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_grRgExit(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_grPassiveEntry(void)
                   GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_grPassiveExit(void)
                   GCC_ATTRIB_NOINLINE();
/* ------------------------- Private Variables ------------------------------ */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct the GR object.  This sets up the HAL interface used by the
 * GR module.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructGr(void)
{
   // Add actions here to be performed prior to pg task is scheduled
   return FLCN_OK;
}

/*!
 * @brief GR initialization post init
 *
 * Initialization of GR related structures immediately after scheduling
 * the pg task in scheduler.
 */
FLCN_STATUS
grPostInit(void)
{
    // All members are by default initialize to Zero.
    memset(&Gr, 0, sizeof(Gr));

    // Initialize Rail Gating specific parameters
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG))
    {
        FLCN_STATUS     status;
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libInit)
        };

        // Create GR-RG Sequence binary semaphore.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = lwrtosSemaphoreCreateBinaryGiven(&GrRgSemaphore,
                                                      OVL_INDEX_DMEM(lpwr));
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NO_FREE_MEM;
        }
    }

    // Initialize common parameters
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_GR))
    {
        // Initialize priv masks
        grPgPrivErrHandlingInit_HAL(&Gr);

        // Allocate and assign GR sequence cache.
        Gr.pSeqCache = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1, GR_SEQ_CACHE);
        if (Gr.pSeqCache == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NO_FREE_MEM;
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Initialization of GR
 *
 * RM based initialization of GR. This function disables power gating delays
 * for GR-RPPG.
 */
void
grInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams
)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);

    // Initialize PG interfaces for GR
    pGrState->pgIf.lpwrEntry = s_grPgEntry;
    pGrState->pgIf.lpwrExit  = s_grPgExit;
    pGrState->pgIf.lpwrReset = s_grPgReset;

    // Setup Idle and Holdoff masks
    grInitSetIdleMasks_HAL(&Gr);
    grInitSetHoldoffMask_HAL(&Gr);

    // GR-RPPG don't need "power gating" delays
    if (PMUCFG_FEATURE_ENABLED(PMU_GR_RPPG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pGrState, GR, RPPG))
    {
        pgCtrlPgDelaysDisable_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_GR_PG);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_GR_PERFMON_RESET_WAR))
    {
        // Initialize the data structure to save PERFMON state
        grPerfmonWarStateInit_HAL(&Gr);
    }

    // Initialize histogram counters for CPU based adaptive algorithm
    if (!PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR))
    {
        pgApConfigIdleMask_HAL(&Pg, RM_PMU_PGCTRL_IDLE_MASK_HIST_IDX_GR,
                                    pGrState->idleMask);
        pgApConfigCntrMode_HAL(&Pg, RM_PMU_PGCTRL_IDLE_MASK_HIST_IDX_GR,
                                    PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE);
    }

    // Override the GR PRIV_RING Support based on the PLM Settings
    grPgPrivErrHandlingSupportOverride_HAL(&Gr);
}

/*!
 * @brief Initialization of GR-RG
 *
 * RM based initialization of GR-RG. This function sets Idle and
 * Holdoff masks for GPC-RG.
 */
void
grRgInit
(
    RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams
)
{
    OBJPGSTATE *pGrRgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);

    // Initialize PG interfaces for GR-RG
    pGrRgState->pgIf.lpwrEntry = s_grRgEntry;
    pGrRgState->pgIf.lpwrExit  = s_grRgExit;

    // Setup Idle and Holdoff masks
    grRgInitSetIdleMasks_HAL(&Gr);
    grRgInitSetHoldoffMask_HAL(&Gr);

    // Override entry conditions of GR-RG.
    grRgEntryConditionConfig_HAL();

    // Override the GR PRIV_RING Support based on the PLM Settings
    grRgPrivErrHandlingSupportOverride_HAL(&Gr);
}

/*!
 * @brief Initialization of GR-Passive
 *
 * RM based initialization of GR-Passive. This function sets idle masks
 * for GR-Passive.
 */
void
grPassiveInit(void)
{
    OBJPGSTATE *pGrPassiveState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PASSIVE);

    // Initialize PG interfaces for GR-Passive
    pGrPassiveState->pgIf.lpwrEntry = s_grPassiveEntry;
    pGrPassiveState->pgIf.lpwrExit  = s_grPassiveExit;

    grPassiveIdleMasksInit_HAL(&Gr);
}

/*!
 * @brief Post Init settings of Graphics features
 */
void
grLpwrCtrlPostInit(void)
{
    LwU32 holdoffMask = 0;

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_PG))
    {
        OBJPGSTATE *pGrPgState  = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
        holdoffMask = (holdoffMask | pGrPgState->holdoffMask);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG))
    {
        OBJPGSTATE *pGrRgState  = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_RG);
        holdoffMask = (holdoffMask | pGrRgState->holdoffMask);
    }

    // Enable the holdoff interrupts based on enabled mask
    pgEnableHoldoffIntr_HAL(&Pg, holdoffMask, LW_TRUE);

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG) &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_GR_RG))
    {
        //
        // GR-RG is by default disabled until EI is engaged. Initialize
        // mutual exclusion policy settings to keep GP-RG by default
        // disabled on boot.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_GRP_MUTUAL_EXCLUSION))
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI)        &&
                pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_EI)  &&
                LPWR_ARCH_IS_SF_SUPPORTED(EI_COUPLED_GR_RG))
            {
                lpwrGrpCtrlMutualExclusion(RM_PMU_LPWR_GRP_CTRL_ID_GR,
                                           LPWR_GRP_CTRL_CLIENT_ID_EI,
                                           BIT(RM_PMU_LPWR_CTRL_ID_GR_RG));
            }
        }

        // Initialize the buffer to Save/Restore global GR state via PMU
        grGlobalStateInitPmu_HAL(&Gr);
    }

    // Initialize the data structure to Save BA state.
    grBaStateInit_HAL(&Gr);

    // Cache the BA State
    grBaStateSave_HAL(&Gr);
}

/*!
 * @brief Check if a message is pending in the LWOS_QUEUE(PMU, LPWR)
 */
LwBool
grWakeupPending(void)
{
    FLCN_STATUS   status;
    LwU32         numItems;

    status = lwrtosQueueGetLwrrentNumItems(LWOS_QUEUE(PMU, LPWR), &numItems);

    if (status != FLCN_OK)
    {
        PMU_HALT();
    }

    return numItems > 0;
}

/*!
 * @brief Assert/De-assert GR engine reset
 *
 * This includes the following -
 *  GPC reset
 *  FECS reset
 *  BECS reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grPgAssertEngineReset(LwBool bAssert)
{
    if (bAssert)
    {
        grAssertBECSReset_HAL(&Gr, LW_TRUE);
        grAssertFECSReset_HAL(&Gr, LW_TRUE);
        grAssertGPCReset_HAL(&Gr, LW_TRUE);
    }
    else
    {
        grAssertFECSReset_HAL(&Gr, LW_FALSE);
        grAssertGPCReset_HAL(&Gr, LW_FALSE);
        grAssertBECSReset_HAL(&Gr, LW_FALSE);
    }
}

/*!
 * @brief Perform GR Subfeature specific actions.
 *
 * Note: This function must be called from the
 * pgCtrlSubfeatureMaskUpdate() API only.
 */
void
grSfmHandler
(
    LwU8 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
    //
    // If Perf Change Sequencer is supported, update change sequencer step exclusion
    // mask based on low power requested mask.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pPgState, GR, RG_PERF_CHANGE_SEQ))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            CHANGE_SEQ_OVERLAYS_LPWR_SCRIPTS_DMEM
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            CHANGE_SEQ *pChangeSeq = PERF_CHANGE_SEQ_GET();

            //
            // @note - Change Sequencer is keeping track of it's sequence's step via
            //         step id exclusion mask bits. So here, we are just updating the
            //         step id exclusion mask bit based on GPC-RG requested mask bit.
            //         Given that OR or AND of two mask bit is very simple operation
            //         we are directly performing the OR / AND operation independent
            //         of checking if the requested mask != enabled mask.
            //
            if (pChangeSeq != NULL)
            {
                if (!LPWR_IS_SF_MASK_SET(GR, HW_SEQUENCE, pPgState->requestedMask))
                {
                    //
                    // Skip LWVDD Rail Gating and Ungating step if HW sequence
                    // is disabled
                    //
                    perfChangeSeqChangeStepIdExclusionMaskSet(
                        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_GATE);
                    perfChangeSeqChangeStepIdExclusionMaskSet(
                        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_UNGATE);

                    //
                    // Skip clock init/deinit and perf restore steps if reset
                    // is disabled
                    //
                    if (!LPWR_IS_SF_MASK_SET(GR, RESET, pPgState->requestedMask))
                    {
                        perfChangeSeqChangeStepIdExclusionMaskSet(
                            LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_DEINIT);
                        perfChangeSeqChangeStepIdExclusionMaskSet(
                            LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_INIT);
                        perfChangeSeqChangeStepIdExclusionMaskSet(
                            LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_RESTORE);

                        perfChangeSeqLpwrPerfRestoreDisableSet(pChangeSeq, LW_TRUE);
                    }
                }

                if (LPWR_IS_SF_MASK_SET(GR, RESET, pPgState->requestedMask))
                {
                    //
                    // Re-enable clock init/deinit and perf restore steps if reset
                    // is enabled
                    //
                    perfChangeSeqChangeStepIdExclusionMaskUnset(
                        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_DEINIT);
                    perfChangeSeqChangeStepIdExclusionMaskUnset(
                        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_INIT);
                    perfChangeSeqChangeStepIdExclusionMaskUnset(
                        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_RESTORE);

                    perfChangeSeqLpwrPerfRestoreDisableSet(pChangeSeq, LW_FALSE);

                    //
                    // Enable LWVDD Rail Gating and Ungating step if HW sequence
                    // is enabled
                    //
                    if (LPWR_IS_SF_MASK_SET(GR, HW_SEQUENCE, pPgState->requestedMask))
                    {
                        perfChangeSeqChangeStepIdExclusionMaskUnset(
                            LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_GATE);
                        perfChangeSeqChangeStepIdExclusionMaskUnset(
                            LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_UNGATE);
                    }
                }
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
#endif

    // Update the Priv Blocker EnabledMask
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_UNIFIED_PRIV_BLOCKER_CTRL))
    {
        //
        // Resetting the priv blocker enabled mask and setting it
        // based on each SFM bit.
        //
        // Note: We can optimize this code here, but to avoid more IMEM
        // usages, i am following this appraoch. Later, we will revisit this
        // again.
        //
        pPgState->privBlockerEnabledMask = 0;

        if (LPWR_IS_SF_MASK_SET(GR, SEC2, pPgState->requestedMask))
        {
            pPgState->privBlockerEnabledMask |= BIT(RM_PMU_PRIV_BLOCKER_ID_SEC2);
        }

        if (LPWR_IS_SF_MASK_SET(GR, GSP, pPgState->requestedMask))
        {
            pPgState->privBlockerEnabledMask |= BIT(RM_PMU_PRIV_BLOCKER_ID_GSP);
        }

        if (LPWR_IS_SF_MASK_SET(GR, XVE, pPgState->requestedMask))
        {
            pPgState->privBlockerEnabledMask |= BIT(RM_PMU_PRIV_BLOCKER_ID_XVE);
        }
    }
}

/*!
 * @brief API to disable/enable all Lpwr Ctrls (except EI) as part of
 *        critical and non-critical part of GPC-RG exit
 *
 * @param[in] bDisallow   Whether to disallow or allow the features
 */
void
grRgPerfSeqDisallowAll
(
    LwBool bDisallow
)
{
    LwU32 pgCtrlMask = 0;
    LwU32 ctrlId     = 0;

    // Skip enablement/disablement for EI
    pgCtrlMask = Pg.supportedMask & (~BIT(RM_PMU_LPWR_CTRL_ID_EI));

    FOR_EACH_INDEX_IN_MASK(32, ctrlId, pgCtrlMask)
    {
        if (bDisallow)
        {
            // Disallow with reason GR_RG
            pgCtrlHwDisallow(ctrlId, PG_CTRL_ALLOW_REASON_MASK(GR_RG));
        }
        else
        {
            // Allow with reason GR_RG
            pgCtrlHwAllow(ctrlId, PG_CTRL_ALLOW_REASON_MASK(GR_RG));
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Context save interface for PG_ENG(GR)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_grPgEntry(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    FLCN_STATUS status   = FLCN_ERROR;

    if (PMUCFG_FEATURE_ENABLED(PMU_GR_POWER_GATING) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG))
    {
        status = grPgSave_HAL(&Gr);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_GR_RPPG) &&
             LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RPPG))
    {
        status = grRppgEntry_HAL(&Gr);
    }

    return status;
}

/*!
 * @brief Context restore interface for PG_ENG(GR)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_grPgExit(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    FLCN_STATUS status   = FLCN_ERROR;

    if (PMUCFG_FEATURE_ENABLED(PMU_GR_POWER_GATING) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG))
    {
        status = grPgRestore_HAL(&Gr);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_GR_RPPG) &&
             LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, RPPG))
    {
        status = grRppgExit_HAL(&Gr);
    }

    return status;
}

/*!
 * @brief Engine reset interface for PG_ENG(GR)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_grPgReset(void)
{
    OBJPGSTATE *pGrState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    FLCN_STATUS status   = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_GR_POWER_GATING) &&
        LPWR_CTRL_IS_SF_ENABLED(pGrState, GR, ELPG))
    {
        status = grPgReset_HAL(&Gr);
    }

    return status;
}

/*!
 * @brief Context save interface for LPWR_ENG(GR_RG)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_grRgEntry(void)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG))
    {
        status = grGrRgEntry_HAL(&Gr);
    }

    return status;
}

/*!
 * @brief Context restore interface for LPWR_ENG(GR_RG)
 *
 * @return FLCN_OK      Success.
 * @return FLCN_ERROR   Otherwise
 */
static FLCN_STATUS
s_grRgExit(void)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_RG))
    {
        status = grGrRgExit_HAL(&Gr);
    }

    return status;
}

/*!
 * @brief Context save interface for LPWR_ENG(GR_PASSIVE)
 *
 * @return FLCN_OK      Success.
 */
static FLCN_STATUS
s_grPassiveEntry(void)
{
    return FLCN_OK;
}

/*!
 * @brief Context restore interface for LPWR_ENG(GR_PASSIVE)
 *
 * @return FLCN_OK      Success.
 */
static FLCN_STATUS
s_grPassiveExit(void)
{
    return FLCN_OK;
}
