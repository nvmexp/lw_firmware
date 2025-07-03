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
 * @file   objpsi.c
 * @brief  PMU PG PSI related functions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostask.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objhal.h"
#include "pmu_objpsi.h"
#include "pmu_objdi.h"
#include "pmu_objperf.h"
#include "pmu_objpg.h"
#include "pmu_objms.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OBJPSI Psi
    GCC_ATTRIB_SECTION("dmem_lpwr", "Psi");

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED)
OS_TMR_CALLBACK OsCBPsi;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED)

/* ------------------------- Prototypes ------------------------------------- */
static OsTmrCallbackFunc (s_psiHandleOsCallbackMultiRail)
                GCC_ATTRIB_SECTION("imem_libPsiCallback", "s_psiHandleOsCallbackMultiRail")
                GCC_ATTRIB_USED();
static OsTimerCallback (s_psiHandleCallbackMultiRail)
                GCC_ATTRIB_SECTION("imem_libPsiCallback", "s_psiHandleCallbackMultiRail")
                GCC_ATTRIB_USED();
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Construct the PSI object.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructPsi(void)
{
    // Add actions here to be performed prior to pg task is scheduled
    return FLCN_OK;
}

/*!
 * @brief PSI initialization post init
 *
 * Initialization of PSI related structures immediately after scheduling
 * the pg task in scheduler.
 */
FLCN_STATUS
psiPostInit(void)
{
    memset(&Psi, 0, sizeof(Psi));

    return FLCN_OK;
}

/*!
 * @brief Callwlate estimated Sleep current for Power saving features
 *
 * This interface is designed for Current Aware PSI.
 * pwrEquationLeakageEvaluatemA will call it to callwlate sleep current
 * for MSCG and DI.
 *
 * PSI requires the most recent value of estimated sleep current.
 * PSI will be engaged iff isleep < iCrossmA (obtained from vbios)
 *
 * Formula to callwlate iSleepmA for MSCG
 *    iSleep [mA] = Leakage [mA] + mscgDynamicLwrrentCo-eff * voltage [mV]
 *
 * Formula to callwlate iSleepmA for DI
 *    iSleep [mA] = Leakage ( gcx_voltage)
 *
 * Caller has to ensure that this interface will get called only if
 * PMU_PSI_FLAVOUR_LWRRENT_AWARE feature is enabled.
 *
 * Note: This interface is only for Monorail LWVDD. Lwrrenlty this
 * interface is being used till MAXWELL only.
 * So we have hardcoded the railId = RM_PMU_PSI_RAIL_ID_LWVDD
 *
 * @param[in] pLeakage      Pointer to PWR_EQUATION_LEAKAGE object
 * @param[in] leakagemA     Leakage current in mA
 * @param[in] voltageuV     Voltage in uV for equation callwlation
 */
void
psiSleepLwrrentCalcMonoRail
(
    PWR_EQUATION_LEAKAGE   *pLeakage
)
{
    FLCN_STATUS  status    = FLCN_OK;
    PSI_CTRL    *pPsiCtrl  = NULL;
    LwU32        voltageuV = perfVoltageuVGet();
    LwU32        railId    = RM_PMU_PSI_RAIL_ID_LWVDD;
    LwU32        leakagemA = 0;
    LwU32        iSleepmA;
    LwU32        estDynLwrrentuA;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwr)
    };

    // Since function is exelwted as a pmgr task we need to attach lpwr DMEM overlay
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)      &&
            PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS) &&
            perfPstatesAreSupported())
        {
            //
            // MSCG does not lower LWVDD voltage, hence current voltage is used
            // to callwlate estimated sleep current.
            //

            //
            // iSleep[mA] = iLeakage[mA] + (Estimated Dynamic LWVDD Current
            //                              when GPU is in MSCG)
            //
            // iSleep[mA] = iLeakage[mA] + (MscgLwrrentCoeff[mV/A] * voltage[uV])/1000000;
            //
            // iSleep[mA] = iLeakage[mA] + (MscgLwrrentCoeff[mA/V] * voltage[mV])/1000;
            //
            // iSleep[mA] = iLeakage[mA] + (Estimated Dynamic Current[uA])/1000;
            //
            // Multiplication of MscgLwrrentCoeff[mV/A] and voltage[uV] overflows
            // 64 bits. Thus, added intermediate step of callwlating "Estimated
            // Dynamic Lwrrentin uA".
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrEquationLeakageEvaluatemA_CORE(pLeakage, voltageuV,
                    NULL /* PGres = 0.0 */, &leakagemA),
                psiSleepLwrrentCalcMonoRail_exit);

            estDynLwrrentuA = perfMsDynLwrrentCoeffGet(
                              perfDomGrpGetValue(RM_PMU_PERF_DOMAIN_GROUP_PSTATE)) *
                              LW_UNSIGNED_ROUNDED_DIV(voltageuV, 1000);

            iSleepmA = leakagemA + LW_UNSIGNED_ROUNDED_DIV(estDynLwrrentuA, 1000);

            // Store the value of sleep current for MSCG for current railId
            pPsiCtrl = PSI_GET_CTRL(RM_PMU_PSI_CTRL_ID_MS);
            pPsiCtrl->pRailStat[railId]->iSleepmA = iSleepmA;

            //
            // Dis-engage MSCG coupled PSI if iSleepMscg is greater than
            // iOptimalOnePhase. Waking MSCG will automatically dis-engage PSI.
            //
            if ((iSleepmA >= PSI_GET_RAIL(railId)->iCrossmA) &&
                (psiIsCtrlEngaged(RM_PMU_PSI_CTRL_ID_MS, railId)))
            {
                OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
                pgCtrlWakeExt(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
                OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libLpwr));
            }
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI) &&
            PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_DI))
        {
            //
            // iSleep[mA] = leakage(gcx_voltage)
            //
            // Estimated iSleep is equal to leakage current callwlated
            // as per gcx voltage, since dynamic current is expected to
            // be negligent and can be ignored
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrEquationLeakageEvaluatemA_CORE(pLeakage,
                    PG_VOLT_GET_SLEEP_VOLTAGE_UV(PSI_GET_RAIL(railId)->voltRailIdx),
                    NULL /* PGres = 0.0 */, &iSleepmA),
                psiSleepLwrrentCalcMonoRail_exit);

            // Store the value of sleep current for DI
            pPsiCtrl = PSI_GET_CTRL(RM_PMU_PSI_CTRL_ID_DI);
            pPsiCtrl->pRailStat[railId]->iSleepmA = iSleepmA;
        }

psiSleepLwrrentCalcMonoRail_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @brief Callwlate estimated Sleep current for Power saving features
 *
 * This interface is designed for Current Aware PSI.
 * pwrEquationLeakageEvaluatemA will call it to callwlate sleep current
 * for MSCG and DI.
 *
 * PSI requires the most recent value of estimated sleep current.
 * PSI will be engaged iff isleep < iCrossmA (obtained from vbios)
 *
 * Formula to callwlate iSleepmA for MSCG
 *    iSleep [mA] = Leakage [mA] + mscgDynamicLwrrentCo-eff * voltage [mV]
 *
 * Formula to callwlate iSleepmA for DI
 *    iSleep [mA] = Leakage ( gcx_voltage)
 *
 * Caller has to ensure that this interface will get called only if
 * PMU_PSI_PMGR_SLEEP_CALLBACK feature is enabled.
 */
void
psiSleepLwrrentCalcMultiRail()
{
    PSI_CTRL   *pPsiCtrl = NULL;
    FLCN_STATUS status;
    VOLT_RAIL  *pVoltRail;
    LwU8        voltRailIdx;
    LwU32       railId;
    LwU32       iSleepmA;
    LwU32       leakagemA;
    LwU32       estDynLwrrentuA;
    LwU32       voltageuV;

    // Attach overlays required for getLeakage() API
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwr)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        FOR_EACH_INDEX_IN_MASK(32, railId, Psi.railSupportMask)
        {
            if (PSI_FLAVOUR_IS_ENABLED(LWRRENT_AWARE, railId))
            {
                voltRailIdx = PSI_GET_RAIL(railId)->voltRailIdx;

                // On some platforms (e.g. ga104), an invalid index is not
                // unexpected, so just exit as normal if we encounter one.
                if (!VOLT_RAIL_INDEX_IS_VALID(voltRailIdx))
                {
                    goto psiSleepLwrrentCalcMultiRail_exit;
                }

                // Get a valid VOLT_RAIL object
                pVoltRail = VOLT_RAIL_GET(voltRailIdx);
                if (pVoltRail == NULL)
                {
                    PMU_BREAKPOINT();
                    goto psiSleepLwrrentCalcMultiRail_exit;
                }

                if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
                    PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS))
                {
                    //
                    // MSCG does not lower LWVDD voltage, hence current voltage is used
                    // to callwlate estimated sleep current.
                    //

                    //
                    // iSleep[mA] = iLeakage[mA] + (Estimated Dynamic LWVDD Current
                    //                              when GPU is in MSCG)
                    //
                    // iSleep[mA] = iLeakage[mA] + (MscgLwrrentCoeff[mV/A] *
                    // voltage[uV])/1000000;
                    //
                    // iSleep[mA] = iLeakage[mA] + (MscgLwrrentCoeff[mA/V] *
                    //  voltage[mV])/1000;
                    //
                    // iSleep[mA] = iLeakage[mA] + (Estimated Dynamic Current[uA])/1000;
                    //
                    // Multiplication of MscgLwrrentCoeff[mV/A] and voltage[uV] overflows
                    // 64 bits. Thus, added intermediate step of callwlating "Estimated
                    // Dynamic Lwrrentin uA".
                    //

                    // Get current voltage of the specified rail
                    status = voltRailGetVoltage(pVoltRail, &voltageuV);
                    if (status != FLCN_OK)
                    {
                        PMU_HALT();
                        goto psiSleepLwrrentCalcMultiRail_exit;
                    }

                    // Call into DTCS1.2 equation for the specified rail
                    status = voltRailGetLeakage(pVoltRail, voltageuV, LW_TRUE,
                                NULL /* PGres = 0.0 */, &leakagemA);
                    if (status != FLCN_OK)
                    {
                        PMU_HALT();
                        goto psiSleepLwrrentCalcMultiRail_exit;
                    }

                    estDynLwrrentuA = msDynamicLwrrentCoeffGet(railId) *
                                        LW_UNSIGNED_ROUNDED_DIV(voltageuV, 1000);

                    iSleepmA = leakagemA +
                               LW_UNSIGNED_ROUNDED_DIV(estDynLwrrentuA, 1000);

                    // Store the value of sleep current for MSCG for current railId
                    pPsiCtrl = PSI_GET_CTRL(RM_PMU_PSI_CTRL_ID_MS);
                    pPsiCtrl->pRailStat[railId]->iSleepmA = iSleepmA;
                }

                if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI) &&
                    PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_DI))
                {
                    //
                    // iSleep[mA] = leakage(gcx_voltage)
                    //
                    // Estimated iSleep is equal to leakage current callwlated
                    // as per gcx voltage, since dynamic current is expected to
                    // be negligent and can be ignored
                    //
                    status = voltRailGetLeakage(pVoltRail,
                                                PG_VOLT_GET_SLEEP_VOLTAGE_UV(voltRailIdx),
                                                LW_TRUE,
                                                NULL, /* PGres = 0.0 */
                                                &leakagemA);
                    if (status != FLCN_OK)
                    {
                        PMU_HALT();
                        goto psiSleepLwrrentCalcMultiRail_exit;
                    }

                    // Store the value of sleep current for DI for current railId
                    pPsiCtrl = PSI_GET_CTRL(RM_PMU_PSI_CTRL_ID_DI);
                    pPsiCtrl->pRailStat[railId]->iSleepmA = leakagemA;
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

psiSleepLwrrentCalcMultiRail_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @brief Schedules callback to callwlate sleep current for PSI
 *
 * @param[in] bSchedule   Indicates schedule/deschedule callback
 *
 *  @return   FLCN_OK     Callback scheduled successfully
 */
void
psiCallbackTriggerMultiRail
(
    LwBool  bSchedule
)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    if (bSchedule)
    {
        // Create callback only if it was not created in past
        if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBPsi))
        {
            osTmrCallbackCreate(&OsCBPsi,                               // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                OVL_INDEX_IMEM(libPsiCallback),                         // ovlImem
                s_psiHandleOsCallbackMultiRail,                         // pTmrCallbackFunc
                LWOS_QUEUE(PMU, PMGR),                                  // queueHandle
                psiLwrrentPollingPeriodNormalGet(),                     // periodNormalus
                psiLwrrentPollingPeriodSleepGet(),                      // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                RM_PMU_TASK_ID_PMGR);                                   // taskId
        }

        osTmrCallbackSchedule(&OsCBPsi);
    }
    else
    {
        osTmrCallbackCancel(&OsCBPsi);
    }
#else
    if (bSchedule)
    {
        osTimerScheduleCallback(
            &PmgrOsTimer,                                             // pOsTimer
            PMGR_OS_TIMER_ENTRY_PG_PSI_CALLBACK,                      // index
            lwrtosTaskGetTickCount32(),                               // ticksPrev
            psiLwrrentPollingPeriodNormalGet(),                       // usDelay
            DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_SKIP), // flags
            s_psiHandleCallbackMultiRail,                             // pCallback
            NULL,                                                     // pParam
            OVL_INDEX_IMEM(libPsiCallback));                          // ovlIdx
    }
    else
    {
        osTimerDeScheduleCallback(&PmgrOsTimer,                         // pOsTimer
                                  PMGR_OS_TIMER_ENTRY_PG_PSI_CALLBACK); // index
    }
#endif
}

/*!
 * @brief : Sends callback trigger event to PMGR
 *
 * @param[in]  bSchedule  Bool to indicate schedule/deshcedule callback
 */
void
psiSchedulePmgrCallbackMultiRail
(
    LwBool bSchedule
)
{
    DISPATCH_PMGR   disp2Pmgr;

    disp2Pmgr.hdr.eventType     = PMGR_EVENT_ID_PG_CALLBACK_TRIGGER;
    disp2Pmgr.trigger.bSchedule = bSchedule;

    LWOS_WATCHDOG_ADD_ITEM(RM_PMU_TASK_ID_PMGR);
    lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PMGR),
                              &disp2Pmgr, sizeof(disp2Pmgr));
}

/*!
 * @brief : Init the PSI Rail I2C Command values
 *
 * @param[in]  railId   RailId for which we need to init the commands
 */
void
psiRailI2cCommandInit
(
    LwU8    railId
)
{
    PSI_RAIL *pPsiRail = PSI_GET_RAIL(railId);

    // Init the command settings for MP2891
    if (pPsiRail->pI2cInfo->i2cDevType == RM_PMU_I2C_DEVICE_TYPE_POWER_CONTRL_MP28XX)
    {
        // For MP2891 VR, PSI Engage/disengage command consist of two stesp:
        //
        // 1. Command 1 - Select the Rail.
        // 2. Command 2 - Set the Rail Phase
        //
        if (railId == RM_PMU_PSI_RAIL_ID_LWVDD)
        {
            // PSI Engage Command
            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_SELECT].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_SELECT].val         = LPWR_PSI_I2C_MP2891_RAIL_SELECT_LWVDD;

            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_PHASE_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].val         = LPWR_PSI_I2C_MP2891_LWVDD_PHASE_PSI;

            // PSI Disengage Command
            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_SELECT].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_SELECT].val         = LPWR_PSI_I2C_MP2891_RAIL_SELECT_LWVDD;

            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_PHASE_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].val = LPWR_PSI_I2C_MP2891_LWVDD_PHASE_FULL;
        }
        else if (railId == RM_PMU_PSI_RAIL_ID_FBVDD)
        {
            // PSI Engage Command
            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_SELECT].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_SELECT].val         = LPWR_PSI_I2C_MP2891_RAIL_SELECT_FBVDD;

            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_PHASE_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiEngageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].val         = LPWR_PSI_I2C_MP2891_FBVDD_PHASE_PSI;

            // PSI Disengage Command
            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_SELECT].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_SELECT].val         = LPWR_PSI_I2C_MP2891_RAIL_SELECT_FBVDD;

            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].controlAddr = LPWR_PSI_I2C_MP2891_RAIL_PHASE_SELECT_CTRL_REG;
            pPsiRail->pI2cInfo->psiDisengageCommand[PSI_I2C_CMD_RAIL_PHASE_UPDATE].val         = LPWR_PSI_I2C_MP2891_FBVDD_PHASE_FULL;
        }
        // We do not suuport I2C PSI on any other Rails
        else
        {
            PMU_BREAKPOINT();
        }
    }
}

/*!
 * @brief : Disallow/Allow the Pstate coupled PSI engagement/disengagement
 *
 * @param[in]  bDisallow : LW_TRUE -> PSI Can not be enagged by Psate.
 *                         LW_FALSE -> PSI can be engaged by Pstate
 */
FLCN_STATUS
psiPstateDisallow
(
    LwBool bDisallow
)
{
    FLCN_STATUS status = FLCN_OK;

    // If its a disable Call, send the request to disengage PSI on all supported rails.
    if (bDisallow)
    {
        // Set the variable so that future PSI enagement doest not happen.
        Psi.bPstateCoupledPsiDisallowed = LW_TRUE;

        // Issue the PSI disengage call. If PSI is enagged, it will disengage them.
        status = psiDisengagePsi_HAL(&Psi, RM_PMU_PSI_CTRL_ID_PSTATE, Psi.railSupportMask,
                                     PSI_I2C_ENTRY_EXIT_STEP_MASK_ALL);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }
    else
    {
        Psi.bPstateCoupledPsiDisallowed = LW_FALSE;
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @ref     OsTmrCallback
 *
 * @brief   Handles PSI callback
 *
 * Callback function callwlates sleep current for DI and MS.
 */
static FLCN_STATUS
s_psiHandleOsCallbackMultiRail
(
    OS_TMR_CALLBACK *pCallback
)
{
    PSI_CTRL *pPsiCtrl      = NULL;
    LwBool    bMsDisengaged = LW_FALSE;
    LwU32     railId;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, lpwr)
    };

    // This function is a pmgr task we need to attach lpwr DMEM overlay
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Callwlate sleep current
        psiSleepLwrrentCalcMultiRail();

        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS) &&
            PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_MS))
        {
            pPsiCtrl = PSI_GET_CTRL(RM_PMU_PSI_CTRL_ID_MS);

            //
            // Dis-engage MSCG coupled PSI if iSleepMscg is greater than
            // iCrossmA. Waking MSCG will automatically disengage PSI.
            //
            FOR_EACH_INDEX_IN_MASK(32, railId, Psi.railSupportMask)
            {
                if (PSI_FLAVOUR_IS_ENABLED(LWRRENT_AWARE, railId))
                {
                    if ((pPsiCtrl->pRailStat[railId]->iSleepmA >=
                        PSI_GET_RAIL(railId)->iCrossmA))
                    {
                        //
                        // We should only send single MS wakeup event for
                        // all the PSI current rails. We have to add check
                        // for Pascal spilt rails.
                        //
                        // TODO: we will have a clean implementation
                        // in follow up CL.
                        //
                        bMsDisengaged = LW_TRUE;
                    }
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;

            // wakeup the MSCG
            if (bMsDisengaged)
            {
                OSTASK_OVL_DESC ovlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
                {
                    pgCtrlWakeExt(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
                }
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * @ref     OsTimerCallback
 *
 * @brief   Handles PSI callback
 *
 * Callback function callwlates sleep current for DI and MS.
 */
static void
s_psiHandleCallbackMultiRail
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_psiHandleOsCallbackMultiRail(NULL);
}
