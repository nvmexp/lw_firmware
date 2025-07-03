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
 * @file    vfe.c
 * @brief   VFE class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "lwostimer.h"
#include "perf/3x/vfe.h"
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "pmu_objpmu.h"
#include "pmu_objlpwr.h"
#include "lib_lw64.h"
#include "cmdmgmt/cmdmgmt.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static OsTimerCallback                          (s_vfeTimerCallback)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "s_vfeTimerCallback")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc                        (s_vfeTimerOsCallback)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "s_vfeTimerOsCallback")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc                        (s_vfeTimerOsCallbackPri)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "s_vfeTimerOsCallbackPri");

static void             s_vfeRmNotify(void);
static void             s_voltProcessDynamilwpdate(void);
static LwS32            s_vfeCalcAdcCode(LwF32 val);

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Restrict VFE ppolling period to 200+ ms in sleep aware low power mode.
 * TODO : The low sampling perod should come from VBIOS instead of hard coding.
 */
#define VFE_CALLBACK_SLEEP_SAMPLE_PERIOD_US(_pollingPeriodms)                  \
    (((_pollingPeriodms <= 200U) ? 200U : _pollingPeriodms) * 1000U)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Structure holding all VFE related data.
 */
static VFE                                   Vfe
    GCC_ATTRIB_SECTION("dmem_perfVfe", "Vfe") = {0};

/*!
 * @brief Primary VFE_VARS group.
 */
static VFE_VARS                           VfeVars
    GCC_ATTRIB_SECTION("dmem_perfVfe", "VfeVars");

/*!
 * @brief Primary VFE_EQUS group.
 */
static VFE_EQUS                           VfeEqus
    GCC_ATTRIB_SECTION("dmem_perfVfe", "VfeEqus");

/*!
 * @brief VFE_EQU_MONITOR.
 */
static VFE_EQU_MONITOR                    VfeEquMonitor
    GCC_ATTRIB_SECTION("dmem_perfVfe", "VfeEquMonitor");

/*!
 * @brief VFE derived callback structure from base class OS_TMR_CALLBACK.
 */
typedef struct
{
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    /*!
     * @brief OS_TMR_CALLBACK super class.
     *
     * @protected
     *
     * This should always be the first member! only enabled when OS callbacks
     * are centralized.
     */
    OS_TMR_CALLBACK super;
#endif

    /*!
     * @brief Specifies if the PMU needs to notify the RM VFE clients of any
     * changes.
     */
    LwBool          bTriggerRmNotification;
} OS_TMR_CALLBACK_ENTRY_VFE;

/*!
 * @brief VFE callback instance.
 */
static OS_TMR_CALLBACK_ENTRY_VFE OsCBVfeTimer;

/* ------------------------ Global Variables -------------------------------- */

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the VFE component.
 *
 * @pre The PERF task must have been initialized successfully.
 */
void
perfVfePostInit(void)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(BASIC)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PERF_GET_VFE(&Perf) = &Vfe;

        Vfe.pVars = &VfeVars;
        Vfe.pEqus = &VfeEqus;

        Vfe.pRmEquMonitor = &VfeEquMonitor;

        vfeEquMonitorInit(Vfe.pRmEquMonitor);

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_PMU_EQU_MONITOR))
        {
            vfeEquMonitorInit(PERF_PMU_VFE_EQU_MONITOR_GET());
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @copydoc VfeVarEvaluate()
 */
FLCN_STATUS
vfeVarEvaluate
(
    LwU8    vfeVarIdx,
    LwF32  *pResult
)
{
    VFE_CONTEXT    *pContext = vfeContextGet();
    FLCN_STATUS     status   = FLCN_ERR_ILWALID_STATE;

    if (pContext != NULL)
    {
        status = vfeVarEvalByIdx(pContext, vfeVarIdx, pResult);
    }

    return status;
}

/*!
 * @copydoc VfeEquEvaluate()
 */
FLCN_STATUS
vfeEquEvaluate_IMPL
(
    LwVfeEquIdx                 vfeEquIdx,
    RM_PMU_PERF_VFE_VAR_VALUE  *pValues,
    LwU8                        valCount,
    LwU8                        outputType,
    RM_PMU_PERF_VFE_EQU_RESULT *pResult
)
{
    VFE_CONTEXT    *pContext = vfeContextGet();
    FLCN_STATUS     status   = FLCN_ERR_ILWALID_INDEX;
    VFE_EQU        *pEqu;
    LwF32           result;

    if (pContext == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto vfeEquEvaluate_IMPL_exit;
    }

    pEqu = BOARDOBJGRP_OBJ_GET(VFE_EQU, vfeEquIdx);
    if (pEqu == NULL)
    {
        goto vfeEquEvaluate_IMPL_exit;
    }

    //
    // Assure that caller receives exactly what he asked for. The "_UNITLESS"
    // check is needed for older VBIOSes that does not have this information.
    // Also ensure that caller doesn't request for "_UNITLESS" output type.
    //
    if (((pEqu->outputType != LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UNITLESS) &&
         (pEqu->outputType != outputType)) ||
        (outputType == LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UNITLESS))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto vfeEquEvaluate_IMPL_exit;
    }

    status = vfeVarClientValuesLink(pContext, pValues, valCount, LW_TRUE);
    if (status != FLCN_OK)
    {
        goto vfeEquEvaluate_IMPL_exit;
    }

    // Evaluate the equation.
    status = vfeEquEvalList(pContext, vfeEquIdx, &result);
    if (status != FLCN_OK)
    {
        goto vfeEquEvaluate_IMPL_exit;
    }

    // Copy out computed value based on the output type.
    switch (outputType)
    {
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_FREQ_MHZ:
        {
            pResult->freqMHz = lwF32ColwertToU32(result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV:
        {
            pResult->voltuV = lwF32ColwertToU32(result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VF_GAIN:
        {
            pResult->vfGain = lwF32ColwertToU32(result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_DELTA_UV:
        {
            pResult->voltDeltauV = lwF32ColwertToS32(result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_WORK_TYPE:
        {
            pResult->workType = LW_TYPES_F32_TO_UFXP_X_Y_ROUND(20, 12, result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_UTIL_RATIO:
        {
            pResult->utilRatio = LW_TYPES_F32_TO_UFXP_X_Y_ROUND(20, 12, result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_WORK_FB_NORM:
        {
            pResult->workFbNorm = LW_TYPES_F32_TO_UFXP_X_Y_ROUND(20, 12, result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_POWER_MW:
        {
            pResult->powermW = lwF32ColwertToU32(result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_PWR_OVER_UTIL_SLOPE:
        {
            pResult->pwrOverUtilSlope = LW_TYPES_F32_TO_UFXP_X_Y_ROUND(20, 12, result);
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_ADC_CODE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_TEMPERATURE_VARIATION_WAR_BUG_200423771))
            {
                pResult->adcCode = s_vfeCalcAdcCode(result);
            }
            else
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                PMU_BREAKPOINT();
            }
            break;
        }
        case LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_THRESH_PERCENT:
        {
            pResult->threshPercent = LW_TYPES_F32_TO_UFXP_X_Y_ROUND(20, 12, result);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_STATE;
            break;
        }
    }

    if (status == FLCN_OK)
    {
        status = vfeVarClientValuesLink(pContext, pValues, valCount, LW_FALSE);
        if (status != FLCN_OK)
        {
            goto vfeEquEvaluate_IMPL_exit;
        }
    }

vfeEquEvaluate_IMPL_exit:
    return status;
}

/*!
 * @brief Loads/unloads the VFE component.
 *
 * When loading the VFE component, the equations and variables are evaluated.
 * Additionally, the timer callback used by the component to monitor the
 * variables and equations is started.
 *
 * @param[in]  bLoad  Specifies whether the unit is to be loaded (LW_TRUE) or
 *                    unloaded (LW_FALSE).
 *
 * @return FLCN_OK The VFE unit was successfully loaded/unloaded.
 */
FLCN_STATUS
perfVfeLoad
(
    LwBool bLoad
)
{
    if (bLoad)
    {
        //
        // Set the bObjectsUpdated to TRUE each time VFE is loaded so that the
        // VFE evaluation will be forced even though the independent variables
        // do not change.
        //
        PERF_GET_VFE(&Perf)->bObjectsUpdated = LW_TRUE;

        //
        // Force callback at time ZERO to initialize dependent code. NULL value
        // of bTriggerRmNotification prevents callback to issue message to the RM.
        //
        OsCBVfeTimer.bTriggerRmNotification = LW_FALSE;
        s_vfeTimerOsCallback((OS_TMR_CALLBACK *)&OsCBVfeTimer);

        OsCBVfeTimer.bTriggerRmNotification = LW_TRUE;

        // Now schedule the periodic callback only for the first time
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
        if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBVfeTimer.super))
        {
            osTmrCallbackCreate(&OsCBVfeTimer.super,                                                // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,                             // type
                OVL_INDEX_IMEM(perfIlwalidation),                                                   // ovlImem
                (OsTmrCallbackFuncPtr)OS_LABEL(s_vfeTimerOsCallback),                               // pTmrCallbackFunc
                LWOS_QUEUE(PMU, PERF),                                                              // queueHandle
                (LwU32)(PERF_GET_VFE(&Perf)->pollingPeriodms) * (LwU32)1000U,                       // periodNormalus
                VFE_CALLBACK_SLEEP_SAMPLE_PERIOD_US((LwU32)(PERF_GET_VFE(&Perf)->pollingPeriodms)), // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                                                   // bRelaxedUseSleep
                RM_PMU_TASK_ID_PERF);                                                               // taskId
            osTmrCallbackSchedule(&OsCBVfeTimer.super);
        }
        else
        {
            //Update callback period
            osTmrCallbackUpdate(&OsCBVfeTimer.super,
                (LwU32)(PERF_GET_VFE(&Perf)->pollingPeriodms) * (LwU32)1000U,
                VFE_CALLBACK_SLEEP_SAMPLE_PERIOD_US((LwU32)(PERF_GET_VFE(&Perf)->pollingPeriodms)),
                OS_TIMER_RELAXED_MODE_USE_NORMAL,
                OS_TIMER_UPDATE_USE_BASE_LWRRENT);
        }
#else
        osTimerScheduleCallback(
            &PerfOsTimer,                                                 // pOsTimer
            PERF_OS_TIMER_ENTRY_VFE_CALLBACK,                             // index
            lwrtosTaskGetTickCount32(),                                   // ticksPrev
            (LwU32)(PERF_GET_VFE(&Perf)->pollingPeriodms) * (LwU32)1000U, // usDelay
            DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_SKIP),     // flags
            s_vfeTimerCallback,                                           // pCallback
            NULL,                                                         // pParam
            OVL_INDEX_IMEM(perfIlwalidation));                            // ovlIdx
#endif
    }

    return FLCN_OK;
}

/*!
 * @brief Helper function to ilwalidate VFE from VFE BOARDOBJGRP SET functions.
 *
 * Placed within @ref .imem_libPerfBoardObj, will load in @ref
 * OSTASK_OVL_DESC_DEFINE_VFE(FULL) and then call @ref vfeIlwalidate().
 *
 * @return FLCN_STATUS returned by @ref vfeIlwalidate().
 */
FLCN_STATUS
vfeBoardObjIlwalidate(void)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // If successfully update the objects, ilwalidate VFE and all dependent
        // object groups.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            vfeIlwalidate(),
            vfeBoardObjIlwalidate_exit);

vfeBoardObjIlwalidate_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Exelwtes generic VFE_ILWALIDATE RPC.
 *
 * @param[in,out]  pParams  Pointer to RM_PMU_RPC_STRUCT_PERF_VFE_ILWALIDATE.
 *
 * @return The return value from @ref vfeIlwalidate().
 */
FLCN_STATUS
pmuRpcPerfVfeIlwalidate
(
    RM_PMU_RPC_STRUCT_PERF_VFE_ILWALIDATE *pParams
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = vfeIlwalidate();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Helper to look-up the VFE Context for the current task.
 *
 * @return A pointer the @ref VFE_CONTEXT for the current task.
 */
VFE_CONTEXT *
vfeContextGet(void)
{
    VFE_CONTEXT *pVfeContext = NULL;

    // Pre-PStates35 all code shares same context.
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35))
    {
        pVfeContext = VFE_CONTEXT_GET(&Perf, VFE_CONTEXT_ID_TASK_PERF);
    }
    else
    {
        //
        // For PStates35 each task has its own VFE Context, PMGR being exception
        // due to a single init time use-case that was a pre-PStates35 bug
        // carried over. This must be revisited if PMGR adds more VFE evaluation
        // use-cases.
        //
        switch (osTaskIdGet())
        {
            case RM_PMU_TASK_ID_PERF:
            case RM_PMU_TASK_ID_PMGR:
                pVfeContext = VFE_CONTEXT_GET(&Perf, VFE_CONTEXT_ID_TASK_PERF);
                break;
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35))
            case RM_PMU_TASK_ID_PERF_DAEMON:
                pVfeContext = VFE_CONTEXT_GET(&Perf, VFE_CONTEXT_ID_TASK_PERF_DAEMON);
                break;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35)
            default:
                PMU_BREAKPOINT();
                pVfeContext = NULL;
                break;
        }
    }

    return pVfeContext;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief VFE timer callback periodically sampling SINGLE_SENSED Variables.
 *
 * @copydoc OsTimerOsCallback
 */
static FLCN_STATUS
s_vfeTimerOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status;

    status = s_vfeTimerOsCallbackPri(pCallback);

    return status;
}

/*!
 * @brief VFE timer callback periodically sampling SINGLE_SENSED Variables
 * for primary group.
 *
 * @copydoc OsTimerOsCallback
 */
static FLCN_STATUS
s_vfeTimerOsCallbackPri
(
    OS_TMR_CALLBACK *pCallback
)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        LwBool bChanged = LW_FALSE;

        // Use the PERF DMEM write semaphore.
        perfWriteSemaphoreTake();
        {
            // Re-cache all sensed variables and see if they changed.
            if (FLCN_OK != vfeVarSampleAll(PERF_GET_VFE_VARS(&Perf), &bChanged))
            {
                PMU_BREAKPOINT();
            }

            bChanged = bChanged || PERF_GET_VFE(&Perf)->bObjectsUpdated;

            // If changed, broadcast to all VFE clients (part 1).
            if (bChanged)
            {
                //
                // @note - Due to the required overlay combinations and MUTEX,
                // VFE code is directly caling PRE and POST clk ilwalidation
                // interfaces instead of calling @ref clkIlwalidate
                //

                // Ilwalidate the clocks and request for VF lwrve re-evaluation.
                (void)preClkIlwalidate(LW_TRUE);
            }
        }
        perfWriteSemaphoreGive();

        // If changed, broadcast to all VFE clients (part 2).
        if (bChanged)
        {
            // Use the PERF DMEM read semaphore.
            perfReadSemaphoreTake();
            {
                //
                // Re-evaluate the VFE_EQU_MONITOR structures, caching the
                // latest values so they can be referenced by other code without
                // having to inline IMEM/DMEM attach and evaluation.
                //
                (void)vfeEquMonitorUpdate(PERF_GET_VFE(&Perf)->pRmEquMonitor, LW_TRUE);
                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_PMU_EQU_MONITOR))
                {
                    (void)vfeEquMonitorUpdate(PERF_PMU_VFE_EQU_MONITOR_GET(), LW_TRUE);
                }

                // Ilwalidate the objects dependent on VF lwrve.
                (void)postClkIlwalidate();

                s_voltProcessDynamilwpdate();

                if (PMUCFG_FEATURE_ENABLED(PMU_DI_SEQUENCE_COMMON))
                {
                    lpwrProcessVfeDynamilwpdate();
                }

                // ... add new stuff here ...

                // Non-NULL value prevents callback to issue message to the RM.
                if (((OS_TMR_CALLBACK_ENTRY_VFE *)pCallback)->bTriggerRmNotification)
                {
                    s_vfeRmNotify();
                }

                PERF_GET_VFE(&Perf)->bObjectsUpdated = LW_FALSE;
            }
            perfReadSemaphoreGive();
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK; // NJ-TODO-CB
}

/*!
 * @brief VFE timer callback periodically sampling SINGLE_SENSED Variables.
 *
 * @copydoc OsTimerCallback
 */
static void
s_vfeTimerCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    (void)s_vfeTimerOsCallback((OS_TMR_CALLBACK *)&OsCBVfeTimer);
}

/*!
 * @brief Notifies the RM that the VFE independent variables has changed and
 * that RM needs to query into PMU to retrieve updated state for all relevant
 * features.
 */
static void
s_vfeRmNotify(void)
{
    PMU_RM_RPC_STRUCT_PERF_VFE_CALLBACK rpc;
    FLCN_STATUS                         status;

    // RC-TODO, properly handle status here.
    PMU_RM_RPC_EXELWTE_BLOCKING(status, PERF, VFE_CALLBACK, &rpc);
}

/*!
 * @brief Update VOLT dynamic parameters that depend on VFE.
 */
static void
s_voltProcessDynamilwpdate(void)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        (void)voltProcessDynamilwpdate();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @brief Helper function to callwlate the ADC code based on the equation's
 * result.
 *
 * @param[in]  val  the floating point value to callwlate the ADC code from.
 *
 * @return the ADC code.
 */
static LwS32
s_vfeCalcAdcCode(LwF32 val)
{
    LwS32   adcCode;
    LwS32   exponent;
    LwU32   input;

    // Represent LwF32 -> LwU32 for binary math.
    input = lwF32MapToU32(&val);

    //
    // Requirements.
    // ADC Code is expected to be negative number. The expected output is
    // rounded down 2's complement number.
    // Output of VFE Evalation -> Expected output ADC Code
    // -12.375 -> -13 represented in 2's complement format.
    //

    // Pull out the mantissa as fixed-point, including adding 1.
    adcCode = (LwS32)LW_TYPES_SINGLE_MANTISSA_TO_UFXP9_23(input);

    // If mantissa is returned as zero, the output is zero.
    if (adcCode == LW_TYPES_U32_TO_UFXP_X_Y(9, 23, 0))
    {
        adcCode = 0;
        goto s_vfeCalcAdcCode_done;
    }

    // Pull out the exponent value.
    exponent = LW_TYPES_SINGLE_EXPONENT_BIASED(input);

    //
    // If exponent is negative number then the number is in the range of
    // (-1 , 1), thus set -1 if the sign bit it set otherwise set 0.
    //
    if (exponent < 0)
    {
        //
        // If the sign bit is negative, the output is -1 otherwise
        // the output is 0.
        //
        if (FLD_TEST_DRF(_TYPES, _SINGLE, _SIGN, _NEGATIVE, input))
        {
            adcCode = (-1);
        }
        else
        {
            adcCode = 0;
        }
        goto s_vfeCalcAdcCode_done;
    }
    else // (exponent >= 0)
    {
        // Ensure that there is no overflow.
        if (exponent > 23)
        {

            //
            // If signed bit is set, return the negative max value else
            // return positive max value.
            //
            if (FLD_TEST_DRF(_TYPES, _SINGLE, _SIGN, _NEGATIVE, input))
            {
                adcCode = LW_S32_MIN;
            }
            else
            {
                adcCode = LW_S32_MAX;
            }

            PMU_BREAKPOINT();
            goto s_vfeCalcAdcCode_done;
        }

        //
        // If signed bit is set, colwert to 2's complement number by
        // multiplication with (-1)
        //
        if (FLD_TEST_DRF(_TYPES, _SINGLE, _SIGN, _NEGATIVE, input))
        {
            adcCode *= (-1);
        }

        // Truncate the ADC CODE to get the 2's complement rounded number.
        adcCode >>= (23 - exponent);
    }

s_vfeCalcAdcCode_done:
    return adcCode;
}
