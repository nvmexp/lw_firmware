/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objdi.h"
#include "pmu_objgcx.h"

// Wrapper for the generated GCX RPC source file.
#include "rpcgen/g_pmurpc_gcx.c"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Handles RM_PMU_RPC_ID_GCX_INIT_OS
 *
 * The INIT command is sent to pass initialization data to the
 * deepidle task that is necessary for entering deepidle.  No
 * state change is required.
 *
 * @param[in]   RM_PMU_RPC_STRUCT_GCX_INIT_OS
 *
 *              bIsGpuDisplayless   Stores whether current GPU is displayless
 *
 *              voltageSettleTimeUs DIs voltage settle time Us
 *
 *              diSupportMask       Support mask for DI
 *
 * @param[out]  pLwstom             Void pointer used to send back data to caller.
 *                                  Parameter not used in this RPC.
 */
static FLCN_STATUS
s_pmuRpcGcxInitOs
(
    RM_PMU_RPC_STRUCT_GCX_INIT_OS *pParams,
    void                          *pLwstom
)
{
    //
    // The INIT command is sent to pass initialization data to the
    // deepidle task that is necessary for entering deepidle.  No
    // state change is required.
    //

    if (!PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
    {
         gcxDidleAttachCoreSequence_HAL(&Gcx);
    }

    Gcx.bIsGpuDisplayless = pParams->bIsGpuDisplayless;

    if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
    {
        Di.voltageSettleTimeUs = pParams->voltageSettleTimeUs;
        Di.supportMask         = pParams->diSupportMask;
    }

    // Initialize the DIOS statistics structures
    didleResetStatistics(&DidleStatisticsNH);

    //
    // Turn ON (RG_)VBLANK interrupt on requested headIndex.
    // DispHal.dispEnableVblank(headIndex);
    //
    // Keeping this commented out since we will need to HALify this
    // in order to keep size of DIDLE task under control.
    //

    pParams->statsDmemOffset = (LwU32)(&DidleStatisticsNH);
    Gcx.bInitialized = LW_TRUE;

    return FLCN_OK;
}

/*!
 * @brief Handles RM_PMU_RPC_ID_GCX_ARM_OS which instructs the GCX to arm itself
 * for Deep Idle No Heads.
 *
 * @param[in]   RM_PMU_RPC_STRUCT_GCX_ARM_OS
 *
 *              featureSet      Supported features for DIOS engagement
 *
 *              maxSleepTimeUs  Maximum sleep time for DI
 *
 *              bWakeupTimer    Specifies if wakeup timer is enabled or not
 *
 *              bModeAuto       Mode of wakeup timer (AUTO / FORCE)
 *
 * @param[out]  pLwstom         Void pointer used to send back data to caller.
 *                              Parameter used to return nextState.
 */
static FLCN_STATUS
s_pmuRpcGcxArmOs
(
    RM_PMU_RPC_STRUCT_GCX_ARM_OS *pParams,
    void                         *pLwstom
)
{
    OBJGC4 *pGc4 = GCX_GET_GC4(Gcx);

    //
    // The ARM_OS command is sent to allow us to go into Deepidle
    // No Heads autonomously based on Deep L1 signals.
    //

    *((LwU8 *)pLwstom) = DidleLwrState;

    //
    // If the ARM_OS command is received while in the INACTIVE state,
    // the state needs to transition to ARMED_OS
    //
    if (DidleLwrState == DIDLE_STATE_INACTIVE)
    {
        // nextState is communicated using pLwstom
        *((LwU8 *)pLwstom) = DIDLE_STATE_ARMED_OS;

        //
        // Enable the rising and falling edge DeepL1 interrupt when
        // RM sends a ARM-NH command to PMU.
        //
        gcxDidleDeepL1SetInterrupts_HAL(&Gcx, LW_TRUE);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
    {
        Di.bWakeupTimer     = pParams->bWakeupTimer;
        Di.bModeAuto        = pParams->bModeAuto;
        Di.maxSleepTimeUs   = pParams->maxSleepTimeUs;

        // Update the enabledMask for Legacy DI
        Di.enabledMask = (Di.supportMask & pParams->featureSet);

        if (PG_IS_SF_SUPPORTED(Di, DI, CORE_NAFLLS))
        {
            diSeqPllListInitCoreNafll_HAL(&Di, &Di.pPllsCore,
                                               &Di.pllCountCore,
                                               &Di.pNafllCtrlReg);
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
    {
        //
        // Attaching required overlay to pg. So next time PG gets scheduled
        // it will load all the required overlays.
        //
        gcxAttachGC45toPG();
        pGc4->bArmed = LW_TRUE;
    }

    //
    // The ARM_OS command is legal but does not require a state
    // change while in the following states:
    // ENTERING_OS, IN_OS
    //

    return FLCN_OK;
}

/*!
 * @brief Handles RM_PMU_RPC_ID_GCX_DISARM_OS which instructs the GCX to disarm
 * itself for Deep Idle No Heads.
 *
 * @param[in]   RM_PMU_RPC_STRUCT_GCX_DISARM_OS
 *
 * @param[out]  pLwstom         Void pointer used to send back data to caller.
 *                              Parameter used to return nextState.
 */
static FLCN_STATUS
s_pmuRpcGcxDisarmOs
(
    RM_PMU_RPC_STRUCT_GCX_DISARM_OS *pParams,
    void                            *pLwstom
)
{
    OBJGC4 *pGc4 = GCX_GET_GC4(Gcx);

    //
    // The DISARM_OS command is sent to prevent us from going into
    // Deepidle No Heads autonomously based on Deep L1 signals.
    //

    *((LwU8 *)pLwstom) = DidleLwrState;

    //
    // If the DISARM_OS command is received while in the states below,
    // the state needs to transition to INACTIVE
    //
    // Note:  nextState is communicated using pLwstom
    //
    if ((DidleLwrState == DIDLE_STATE_ARMED_OS)    ||
        (DidleLwrState == DIDLE_STATE_ENTERING_OS) ||
        (DidleLwrState == DIDLE_STATE_IN_OS))
    {
        *((LwU8 *)pLwstom) = DIDLE_STATE_INACTIVE;

        //
        // Disable the rising and falling edge DeepL1 interrupt when
        // RM sends a DISARM-NH command to PMU. This will save us from
        // context switching to as well as loading this task from FB
        // each time this interrupt is raised
        //
        gcxDidleDeepL1SetInterrupts_HAL(&Gcx, LW_FALSE);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
    {
        pGc4->bArmed = LW_FALSE;
        gcxDetachGC45fromPG();
    }

    pGc4->cmdStatus = RM_PMU_GCX_CMD_STATUS_OK;
    pGc4->bDisarmResponsePending = LW_TRUE;

    return FLCN_OK;
}

/*!
 * @brief Handles RM_PMU_RPC_ID_GCX_STAT_RESET which instructs the PMU to
 * RESET the specified Deep Idle statistics.
 *
 * @param[in]   RM_PMU_RPC_STRUCT_GCX_STAT_RESET
 *
 * @param[out]  pLwstom         Void pointer used to send back data to caller.
 *                              Parameter not used in this RPC.
 */
static FLCN_STATUS
s_pmuRpcGcxStatReset
(
    RM_PMU_RPC_STRUCT_GCX_STAT_RESET   *pParams,
    void                               *pLwstom
)
{
    didleResetStatistics(&DidleStatisticsNH);
    return FLCN_OK;
}

/* ------------------------- Public Functions ------------------------------- */
