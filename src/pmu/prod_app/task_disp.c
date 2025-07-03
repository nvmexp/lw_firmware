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
 * @file  task_disp.c
 * @brief Display Task which contains brightness control and Self-Refresh Panel.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"
#include "dev_ext_devices.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "main_init_api.h"
#include "pmu_i2capi.h"
#include "cmdmgmt/cmdmgmt.h"
#include "os/pmu_lwos_task.h"
#include "pmu_disp.h"
#include "pmu_objic.h"
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "pmu_oslayer.h"
#include "dbgprintf.h"
#include "lwos_dma.h"
#include "lwostimer.h"
#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define DMA_MIN_READ_ALIGN_MASK   (DMA_MIN_READ_ALIGNMENT - 1)

/* ------------------------- External Definitions --------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_MODE_SET) || PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_GC6_EXIT))
/*! The IPC queue used between psr task and i2c, when i2c tx is ilwoked. */
LwrtosQueueHandle DpAuxAckQueue;
#endif

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TIMER   DispOsTimer;
#else
OS_TMR_CALLBACK OsCBDisp;
#endif

/* ------------------------- Static Variables ------------------------------- */
DISP_CONTEXT  DispContext;

/*!
 * @brief Union of all possible RPC subsurface data that can be handled by the DISP task.
 */
typedef union
{
    /*!
     * @brief Default size, as defined by configuration
     */
    LwU8 defaultSize[OSTASK_ATTRIBUTE_DISP_CMD_SCRATCH_SIZE];
} TASK_SURFACE_UNION_DISP;

/*!
 * @brief   Defines the command buffer for the DISP task.
 */
PMU_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(DISP, "dmem_dispCmdBuffer");

/* ------------------------- Private Functions ------------------------------ */
static void   s_dispInit(DISP_CONTEXT *pContext);
static void   s_dispSetBrightness(DISP_CONTEXT *pContext, LwU16 brightness);
// Making it NONINLINE to save stack size.
static void   s_dispProcessVBlank(DISP_CONTEXT *pContext)
    GCC_ATTRIB_NOINLINE();
static FLCN_STATUS  s_dispEventHandle(DISPATCH_DISP *pRequest);

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static OsTimerCallback  (s_disp1HzCallback)
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc  (s_disp1HzOsCallback)
    GCC_ATTRIB_USED();

lwrtosTASK_FUNCTION(task_disp, pvParameters);

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief      Initialize the DISP Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
dispPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 2;

    OSTASK_CREATE(status, PMU, DISP);
    if (status != FLCN_OK)
    {
        goto dispPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    {
        // Assure that we have queue slot for each callback.
        queueSize++;    // OsCBDisp
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, DISP), queueSize,
                                     sizeof(DISPATCH_DISP));
    if (status != FLCN_OK)
    {
        goto dispPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_MODE_SET) ||
        PMUCFG_FEATURE_ENABLED(PMU_DISP_LWSR_GC6_EXIT))
    {
        // DP access ack queue for disp task
        status = lwrtosQueueCreateOvlRes(&DpAuxAckQueue, 1, sizeof(I2C_INT_MESSAGE));
        if (status != FLCN_OK)
        {
            goto dispPreInitTask_exit;
        }
    }

#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTimerInitTracked(OSTASK_TCB(DISP), &DispOsTimer,
                       DISP_OS_TIMER_ENTRY_NUM_ENTRIES);
#endif

dispPreInitTask_exit:
    return status;
}

/*!
 * Task entry-point and event-loop for the Disp Task. Will run in an
 * infinite-loop waiting for Disp commands to arrive in its command-
 * queue (at which point it will execute the command and go back to sleep).
 */
lwrtosTASK_FUNCTION(task_disp, pvParameters)
{
    DISPATCH_DISP disp2disp;

    //
    // Permanently attach the I2C library to this task so that it can freely
    // call into the i2c-device interfaces.
    //
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libi2c));

    DBG_PRINTF_DISP(("Task Disp\n", 0, 0, 0, 0));

    s_dispInit(&DispContext);

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, DISP), &disp2disp, status, PMU_TASK_CMD_BUFFER_PTRS_GET(DISP))
    {
        status = s_dispEventHandle(&disp2disp);
    }
    LWOS_TASK_LOOP_END(status);
#else
    lwosVarNOP(PMU_TASK_CMD_BUFFER_PTRS_GET(DISP));

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&DispOsTimer, LWOS_QUEUE(PMU, DISP),
                                                   &disp2disp, lwrtosMAX_DELAY);
        {
            if (FLCN_OK != s_dispEventHandle(&disp2disp))
            {
                PMU_HALT();
            }
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&DispOsTimer, lwrtosMAX_DELAY);
    }
#endif
}

/*!
* @brief   Exelwtes generic DISP_GC6_EXIT_MODESET RPC.
*
* @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_GC6_EXIT_MODESET
*/

FLCN_STATUS
pmuRpcDispGc6ExitModeset
(
    RM_PMU_RPC_STRUCT_DISP_GC6_EXIT_MODESET *pParams
)
{
    FLCN_STATUS status;

    if (Disp.pPmsCtx == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto pmuRpcDispGc6ExitModeset_exit;
    }

    status = dispPmsModeSet_HAL(&Disp, pParams->modeset, pParams->bTriggerSrExit);

pmuRpcDispGc6ExitModeset_exit:
    return status;
}

/*!
* @brief   DISP Mscg RG line War RPC
*
* @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_GC6_EXIT_MODESET
*/
FLCN_STATUS
pmuRpcDispMsRgLineWar
(
    RM_PMU_RPC_STRUCT_DISP_MS_RG_LINE_WAR *pParams
)
{
    return dispMsRgLineWarActivate_HAL(&Disp, pParams->rgLineNum,
                                       pParams->headIdx, pParams->bActivate);
}

/*!
 * @brief   Exelwtes generic DISP_BC_SET_BRIGHTNESS RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_BC_SET_BRIGHTNESS
 */
FLCN_STATUS
pmuRpcDispBcSetBrightness
(
    RM_PMU_RPC_STRUCT_DISP_BC_SET_BRIGHTNESS *pParams
)
{
    s_dispSetBrightness(&DispContext, pParams->brightnessLevel);
    return FLCN_OK;
}

/*!
 * @brief   Exelwtes generic DISP_BC_CTRL RPC (BC_LOCK & BC_Unlock).
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_BC_CTRL
 */
FLCN_STATUS
pmuRpcDispBcCtrl
(
    RM_PMU_RPC_STRUCT_DISP_BC_CTRL *pParams
)
{
    DISP_CONTEXT *pContext = &DispContext;

    pContext->bLockBrightness = pParams->bLock;

    return FLCN_OK;
}

/*!
 *  @brief Handler called by ISR to notify Disp task of RG_VBLANK intr.
 *
 *  Called only within ISR.  Do not use critical sections!!!
 *
 *  @param[in]  headIndex   RG_VBLANK head index
 */
void
dispReceiveRGVblankISR
(
    LwU8 headIndex
)
{
    DISPATCH_DISP disp2disp;

    if (DispContext.bInitialized &&
        DispContext.bUpdateBrightness &&
        (!DispContext.bVblankEventQueued) &&
        (headIndex == DispContext.pFpEntry->fpEntry.headIndex))
    {
        disp2disp.hdr.eventType     = DISP_SIGNAL_EVTID;
        disp2disp.signal.signalType = DISP_SIGNAL_RG_VBLANK;
        lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, DISP), &disp2disp,
                                 sizeof(disp2disp));
        DispContext.bVblankEventQueued = LW_TRUE;
    }
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper call handling events sent to DISP task.
 */
static FLCN_STATUS
s_dispEventHandle
(
    DISPATCH_DISP *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE;

    switch (pRequest->hdr.eventType)
    {
        case DISP2UNIT_EVT_RM_RPC:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_UNIT_ID;
            switch (pRequest->hdr.unitId)
            {
                case RM_PMU_UNIT_DISP:
                {
                    status = pmuRpcProcessUnitDisp(&(pRequest->rpc));
                }
                break;
            }
        }
        break;

        case DISP_SIGNAL_EVTID:
            switch (pRequest->signal.signalType)
            {
                case DISP_SIGNAL_RG_VBLANK:
                {
                    status = FLCN_OK;
                    s_dispProcessVBlank(&DispContext);
                    break;
                }
#if PMUCFG_FEATURE_ENABLED(PMU_DISP_MSCG_LOW_FPS_WAR_200637376)
                case DISP_SIGNAL_LAST_DATA:
                {
                    dispProcessLastData_HAL(&Disp);
                    status = FLCN_OK;
                    break;
                }
#endif
            }
            break;

        // Handle PBI events here.
        case DISP_PBI_MODESET_EVTID:
        case DISP_PBI_SR_EXIT_EVTID:
            if (PMUCFG_FEATURE_ENABLED(PMU_PBI_MODE_SET))
            {
                // Ensure we get modeset scripts set from RM.
                if ((Disp.pPmsCtx != NULL) || (pRequest->hdr.eventType == DISP_PBI_SR_EXIT_EVTID))
                {
                    // Do modeset now
                    status = dispPmsModeSet_HAL(&Disp,
                        ((pRequest->hdr.eventType == DISP_PBI_SR_EXIT_EVTID) ?
                        DISP_MODESET_SR_EXIT : DISP_MODESET_BSOD), LW_FALSE);
                }

                //
                // No matter modeset is done or not, we should update
                // the PBI status and clear GPU interrupt bit for client.
                //
                pmuUpdatePbiStatus_HAL(&Pmu, (status == FLCN_OK));
                status = FLCN_OK;
            }
            break;
    }

    return status;
}

/*!
 * Initialize Disp context default setting.
 *
 * @param[in/out]   pContext    Current Disp context
 */
static void
s_dispInit
(
    DISP_CONTEXT  *pContext
)
{
    LwU32 data;

    // Read reference clock
    data = REG_RD32(BAR0, LW_PEXTDEV_BOOT_0);

    switch (DRF_VAL(_PEXTDEV, _BOOT_0, _STRAP_CRYSTAL, data))
    {
        case LW_PEXTDEV_BOOT_0_STRAP_CRYSTAL_27000K:
        {
            pContext->crystalClkFreqHz = 27000000;
            break;
        }
        case LW_PEXTDEV_BOOT_0_STRAP_CRYSTAL_25000K:
        {
            pContext->crystalClkFreqHz = 25000000;
            break;
        }
    }

    pContext->bInitialized = LW_FALSE;
    pContext->lastBrightnessLevel = RM_PMU_DISP_BACKLIGHT_PWM_DUTY_CYCLE_UNITS;
    pContext->bUpdateBrightness = LW_FALSE;
    pContext->bLockBrightness = LW_FALSE;
    pContext->bVblankEventQueued = LW_FALSE;
    pContext->lastDataLineCnt = 0U;
    pContext->frameTimeVblankMscgDisableUs = 0U;
    pContext->bMclkSwitchOwnsFrameCounter = LW_FALSE;

    //
    // Schedule relwrring 1Hz callback.  We can just use the current tick count
    // here because we really don't relative from when we start the scheduling
    // the callbacks - only that they're 1s apart.
    //
#if PMUCFG_FEATURE_ENABLED(PMU_DISP_CHECK_VRR_HACK_BUG_1604175)
    //
    // Check if the GPU is in the approved list for Notebook Gsync
    // If not schedule the callback
    //
    if (!dispIsNotebookGsyncSupported_HAL())
    {
        //
        // Not in the list of approved GPU and not enabled by HULK
        // Schedule the callback to verify that are not running Notebook
        // GSync on an unapproved system.
        //
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
        osTmrCallbackCreate(&OsCBDisp,                                 // pcallback
            OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,    // type
            OVL_INDEX_ILWALID,                                         // ovlImem
            s_disp1HzOsCallback,                                       // pTmrCallbackFunc
            LWOS_QUEUE(PMU, DISP),                                     // queueHandle
            OS_TIMER_DELAY_1_S,                                        // periodNormalus
            OS_TIMER_DELAY_1_S,                                        // periodSleepus
            OS_TIMER_RELAXED_MODE_USE_NORMAL,                          // bRelaxedUseSleep
            RM_PMU_TASK_ID_DISP);                                      // taskId
        osTmrCallbackSchedule(&OsCBDisp);
# else
        osTimerScheduleCallback(
            &DispOsTimer,                                               // pOsTimer
            DISP_OS_TIMER_ENTRY_1HZ_CALLBACK,                           // index
            lwrtosTaskGetTickCount32(),                                 // ticksPrev
            OS_TIMER_DELAY_1_S,                                         // usDelay
            DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC),   // flags
            s_disp1HzCallback,                                          // pCallback
            NULL,                                                       // pParam
            OVL_INDEX_ILWALID);                                         // ovlIdx
#endif
    }
#endif
}

/*!
 * @brief Takes the command issued from RM and callwlates the SOR PWM values
 *        to be programmed during the next vblank.
 *
 * @param[in/out]   pContext        Disp context.
 * @param[in]       brightness      Target brightness level.
 */
static void
s_dispSetBrightness
(
    DISP_CONTEXT   *pContext,
    LwU16           brightness
)
{
    DISP_BRIGHTC_BRIGHTNESS_FXP brightnessFxp0 = LW_TYPES_FXP_ZERO;
    DISP_BRIGHTC_BRIGHTNESS_FXP brightnessFxp1 = LW_TYPES_FXP_ZERO;
    DISP_BRIGHTC_DUTY_CYCLE_FXP dutyCycleFxp0  = LW_TYPES_FXP_ZERO;
    DISP_BRIGHTC_DUTY_CYCLE_FXP dutyCycleFxp1  = LW_TYPES_FXP_ZERO;
    LwU32 newPwmDiv = 0;
    LwU32 newPwmCtl = 0;
    LwU16 minDutyCycle;
    LwU16 maxDutyCycle;
    LwU8  i;

    if (!pContext->bInitialized || pContext->pFpEntry->fpEntry.pwmInfoEntries == 0)
    {
        DBG_PRINTF_DISP(("DISP : not initialized or no PWM freq provided!\n",
                         0, 0, 0, 0));
        return;
    }

    pContext->lastBrightnessLevel = brightness;

    //
    // Colwert to duty cycle by referring luminance map
    //
    // Note that "brightness" variable represents duty cycle in the rest of
    // this function.
    //
    if (pContext->pFpEntry->fpEntry.luminanceEntries >=
        RM_PMU_DISP_NUMBER_OF_BACKLIGHT_PWM_LWRVE_GAMMA_ENTRIES)
    {
        for (i = 0; i < (pContext->pFpEntry->fpEntry.luminanceEntries - 1); i++)
        {
            if (brightness <=
                pContext->pFpEntry->pLuminanceMap[i+1].luminanceValue)
            {
                break;
            }
        }

        if (i == (pContext->pFpEntry->fpEntry.luminanceEntries - 1))
        {
            // Reached end of the table, use last entry
            brightness =
                pContext->pFpEntry->pLuminanceMap[i].dutyCycleValue;
        }
        else if (brightness <=
                 pContext->pFpEntry->pLuminanceMap[i].luminanceValue)
        {
            // Equal to first point
            brightness =
                pContext->pFpEntry->pLuminanceMap[i].dutyCycleValue;
        }
        else if (brightness ==
                 pContext->pFpEntry->pLuminanceMap[i+1].luminanceValue)
        {
            // Equal to second point
            brightness =
                pContext->pFpEntry->pLuminanceMap[i+1].dutyCycleValue;
        }
        else
        {
            brightnessFxp0 = DISP_BRIGHTC_TO_FXP(
                pContext->pFpEntry->pLuminanceMap[i].luminanceValue);
            brightnessFxp1 = DISP_BRIGHTC_TO_FXP(
                pContext->pFpEntry->pLuminanceMap[i+1].luminanceValue);

            // brightnessFxp1 is the delta between 1st and 2nd points
            brightnessFxp1 = brightnessFxp1 - brightnessFxp0;

            // brightnessFxp0 is the delta between target and first point
            brightnessFxp0 = DISP_BRIGHTC_TO_FXP(brightness) -
                             brightnessFxp0;

            // dutyCycleFxp1 is the delta between 1st and 2nd points in the lwrve
            dutyCycleFxp1 = DISP_BRIGHTC_TO_FXP(
                 pContext->pFpEntry->pLuminanceMap[i+1].dutyCycleValue -
                 pContext->pFpEntry->pLuminanceMap[i].dutyCycleValue);

            //
            // callwlate brightnessFxp0 * dutyCycleFxp1
            // brightnessFxp0 can be <1 while dutyCycleFxp1 is always a whole value
            // (fxp10.22 >> 10) * (fxp10.22 >> 22) = fxp20.12
            //
            dutyCycleFxp0 = (brightnessFxp0 >> 10) * (dutyCycleFxp1 >> 22);

            // now divide by brightnessFxp1, result is fxp20.12
            dutyCycleFxp0 = dutyCycleFxp0 / (brightnessFxp1 >> 22);

            // colwert back to fxp10.22 and add 1st duty cycle point
            dutyCycleFxp0 = dutyCycleFxp0 << 10;
            dutyCycleFxp0 += DISP_BRIGHTC_TO_FXP(
                pContext->pFpEntry->pLuminanceMap[i].dutyCycleValue);

            brightness = DISP_BRIGHTC_FROM_FXP(dutyCycleFxp0);
        }
    }

    minDutyCycle =
        pContext->pFpEntry->pBacklightPwmInfo[0].minDutyCyclePer1000;
    maxDutyCycle =
        pContext->pFpEntry->pBacklightPwmInfo[pContext->pFpEntry->fpEntry.pwmInfoEntries - 1].maxDutyCyclePer1000;

    // Clip duty cycle
    if (brightness < minDutyCycle)
    {
        brightness = minDutyCycle;
    }
    else if (brightness > maxDutyCycle)
    {
        brightness = maxDutyCycle;
    }

    // Get the PWM frequency, based on the duty cycle
    for (i = 0; i < (pContext->pFpEntry->fpEntry.pwmInfoEntries - 1); i++)
    {
        if ((brightness >=
             pContext->pFpEntry->pBacklightPwmInfo[i].minDutyCyclePer1000) &&
            (brightness <
             pContext->pFpEntry->pBacklightPwmInfo[i+1].minDutyCyclePer1000))
        {
            break;
        }
    }

    // Callwlate PWM registers
    newPwmDiv = pContext->crystalClkFreqHz /
                pContext->pFpEntry->pBacklightPwmInfo[i].frequencyHz;

    newPwmCtl = (newPwmDiv * brightness) /
                RM_PMU_DISP_BACKLIGHT_PWM_DUTY_CYCLE_UNITS;
    newPwmCtl = FLD_SET_DRF(_PDISP, _SOR_PWM_CTL, _CLKSEL, _XTAL, newPwmCtl);
    newPwmCtl = FLD_SET_DRF(_PDISP, _SOR_PWM_CTL, _SETTING_NEW,
                            _TRIGGER, newPwmCtl);

    // Store in brightc context and update them in next vblank
    pContext->lastPwmDiv = newPwmDiv;
    pContext->lastPwmCtl = newPwmCtl;

    if (!pContext->bUpdateBrightness)
    {
        // Enable RG_VBLANK interrupt on requested headIndex.
        dispSetEnableVblankIntr_HAL(&Disp, pContext->pFpEntry->fpEntry.headIndex,
                                    LW_TRUE);

        pContext->bUpdateBrightness = LW_TRUE;
    }
}

/*!
 * @brief   Exelwtes generic DISP_BC_UPDATE_FPINFO RPC.
 *          Update flat panel information for backlight control.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_FPINFO
 */
FLCN_STATUS
pmuRpcDispBlwpdateFpinfo
(
    RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_FPINFO *pParams
)
{
    DISP_CONTEXT *pContext = &DispContext;
    LwU32         byteOffset;
    DISP_BACKLIGHT_FP_ENTRY *pFpEntry;
    FLCN_STATUS   status   =  FLCN_OK;
    LwU32 backlightPwmSize;

    if (pContext->bInitialized)
    {
        status = FLCN_OK;
        goto pmuRpcDispBlwpdateFpinfo_exit;
    }

    // Initialize the FP entry pointer and data
    pFpEntry = lwosCallocResident(sizeof(*pFpEntry));
    if (pFpEntry == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        PMU_BREAKPOINT();
        goto pmuRpcDispBlwpdateFpinfo_exit;
    }

    ct_assert(sizeof(pParams->dispBrightcBuffer) >= sizeof(pFpEntry->fpEntry));
    (void)memcpy(
        &pFpEntry->fpEntry,
        pParams->dispBrightcBuffer,
        sizeof(pFpEntry->fpEntry));

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Sanity check for number of PMW info Entries
        if ((pFpEntry->fpEntry.pwmInfoEntries == 0) ||
            (pFpEntry->fpEntry.pwmInfoEntries >
                 RM_PMU_DISP_MAX_BACKLIGHT_PWM_INFO_ENTRIES))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcDispBlwpdateFpinfo_exit;
        }

        // Sanity check for number of Luminance Entries
        if (pFpEntry->fpEntry.luminanceEntries >
            RM_PMU_DISP_MAX_BACKLIGHT_PWM_LWRVE_LARGE_GAMMA_ENTRIES)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcDispBlwpdateFpinfo_exit;
        }

        // Validate pFpEntry->headIndex right after dmaRead
        if (pFpEntry->fpEntry.headIndex >=
            Disp.numHeads)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcDispBlwpdateFpinfo_exit;
        }

        // Validate pFpEntry->sorIndex right after dmaRead
        if (pFpEntry->fpEntry.sorIndex >=
            Disp.numSors)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcDispBlwpdateFpinfo_exit;
        }
    }

    // Initialize the backlight PWM info pointer and data
    backlightPwmSize =
        sizeof(*pFpEntry->pBacklightPwmInfo) * pFpEntry->fpEntry.pwmInfoEntries;
    byteOffset = RM_PMU_DISP_FP_ENTRY_PWM_INFO_BYTE_OFFSET;
    PMU_HALT_OK_OR_GOTO(status,
        (pFpEntry->pBacklightPwmInfo = lwosCallocResident(
            backlightPwmSize)) != NULL ?
            FLCN_OK : FLCN_ERR_NO_FREE_MEM,
        pmuRpcDispBlwpdateFpinfo_exit);
    (void)memcpy(
        pFpEntry->pBacklightPwmInfo,
        pParams->dispBrightcBuffer + byteOffset,
        backlightPwmSize);

    // Initialize the luminance map pointer and data
    if (pFpEntry->fpEntry.luminanceEntries > 0U)
    {
        const LwU32 luminanceMapSize =
            sizeof(*pFpEntry->pLuminanceMap) * pFpEntry->fpEntry.luminanceEntries;
        byteOffset += backlightPwmSize;
        PMU_HALT_OK_OR_GOTO(status,
            (pFpEntry->pLuminanceMap = lwosCallocResident(
                luminanceMapSize)) != NULL ?
                FLCN_OK : FLCN_ERR_NO_FREE_MEM,
            pmuRpcDispBlwpdateFpinfo_exit);
        (void)memcpy(
            pFpEntry->pLuminanceMap,
            pParams->dispBrightcBuffer + byteOffset,
            luminanceMapSize);
    }

    dispInitVblankIntr_HAL(&Disp, pFpEntry->fpEntry.headIndex);
    
    pContext->pFpEntry = pFpEntry;
    pContext->bInitialized = LW_TRUE;

pmuRpcDispBlwpdateFpinfo_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic DISP_BC_UPDATE_HEADINFO RPC.
 *          Update head index of internal panel.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_HEADINFO
 */
FLCN_STATUS
pmuRpcDispBlwpdateHeadinfo
(
    RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_HEADINFO *pParams
)
{
    DISP_CONTEXT *pContext     = &DispContext;
    LwU8          newHeadIndex = pParams->pwmIndexHead;
    FLCN_STATUS   status;

    if (!pContext->bInitialized)
    {
        DBG_PRINTF_DISP(("DISP : not initialized yet!\n",
                         0, 0, 0, 0));
        status = FLCN_ERR_ILWALID_STATE;
        goto pmuRpcDispBlwpdateHeadinfo_exit;
    }

    // Sanity check
    if (pContext->pFpEntry->fpEntry.headIndex == newHeadIndex)
    {
        status = FLCN_OK;
        goto pmuRpcDispBlwpdateHeadinfo_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        // Validate newHeadIndex before it set to pFpEntry->fpEntry.headIndex
        if (newHeadIndex >= Disp.numHeads)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto pmuRpcDispBlwpdateHeadinfo_exit;
        }
    }

    dispInitVblankIntr_HAL(&Disp, newHeadIndex);

    if (pContext->bUpdateBrightness)
    {
        pContext->bUpdateBrightness = LW_FALSE;
        dispSetEnableVblankIntr_HAL(&Disp, pContext->pFpEntry->fpEntry.headIndex,
                                    LW_FALSE);

        pContext->pFpEntry->fpEntry.headIndex = newHeadIndex;

        dispSetEnableVblankIntr_HAL(&Disp, pContext->pFpEntry->fpEntry.headIndex,
                                    LW_TRUE);
        pContext->bUpdateBrightness = LW_TRUE;
    }
    else
    {
        pContext->pFpEntry->fpEntry.headIndex = newHeadIndex;
    }

    status = FLCN_OK;

pmuRpcDispBlwpdateHeadinfo_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic ONE_SHOT_START_DELAY_UPDATE RPC.
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_DISP_ONE_SHOT_START_DELAY_UPDATE
 */
FLCN_STATUS
pmuRpcDispOneShotStartDelayUpdate
(
    RM_PMU_RPC_STRUCT_DISP_ONE_SHOT_START_DELAY_UPDATE *pParams
)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_DISP_PROG_ONE_SHOT_START_DELAY))
    {
        Disp.DispImpOsmMclkSwitchSettings = pParams->impOsmMclkSwitchSettings;
    }
#endif
    return FLCN_OK;
}

/*!
 * @brief Process vblank signal when rg vblank signal come-in.
 *
 * @param[in]   pContext    Current Disp context
 */
static void
s_dispProcessVBlank
(
    DISP_CONTEXT    *pContext
)
{
    FLCN_STATUS status;

    //
    // Function dispReceiveRGVblankISR() already checked that Disp task has
    // a work to do.  We raise its priority so it will not get preempted.
    //
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY_VBLANK);

    if (pContext->bUpdateBrightness)
    {
        PMU_RM_RPC_STRUCT_DISP_BC_UPDATE_BRIGHTNESS bcRpc;

        if (!pContext->bLockBrightness)
        {
            REG_WR32(BAR0, LW_PDISP_SOR_PWM_DIV(pContext->pFpEntry->fpEntry.sorIndex),
                              pContext->lastPwmDiv);
            REG_WR32(BAR0, LW_PDISP_SOR_PWM_CTL(pContext->pFpEntry->fpEntry.sorIndex),
                              pContext->lastPwmCtl);
        }
        pContext->bUpdateBrightness = LW_FALSE;

        // Disable RG_VBLANK interrupt on requested headIndex.
        dispSetEnableVblankIntr_HAL(&Disp, pContext->pFpEntry->fpEntry.headIndex,
                                    LW_FALSE);

        // Send update brightness msg to notify RM
        bcRpc.brightnessLevel = pContext->lastBrightnessLevel;

        // Send the Event as an RPC.  RC-TODO Propery handle status here.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, DISP, BC_UPDATE_BRIGHTNESS, &bcRpc);
    }

    pContext->bVblankEventQueued = LW_FALSE;

    // Now we can restore original task priority.
    lwrtosLwrrentTaskPrioritySet(OSTASK_PRIORITY(DISP));
}

/*!
 * @ref     OsTimerCallback
 *
 * Calls 1Hz callback of DISP task
 *
 */
static void
s_disp1HzCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_disp1HzOsCallback(NULL);
}

/*!
 * @ref     OsTmrCallback
 *
 * Calls 1Hz callback of DISP task
 *
 */
static FLCN_STATUS
s_disp1HzOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    dispCheckForVrrHackBug1604175_HAL();

    return FLCN_OK; // NJ-TODO-CB
}
