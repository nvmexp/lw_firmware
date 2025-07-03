/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   thrmmon.c
 * @brief  Thermal monitor task
 *
 * The following is a thermal control module that aims to monitor the
 * thermal interrupts on PMU.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

#include "main.h"

/* ------------------------ Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "task_therm.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "main.h"

/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBThermMonitor;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTimerCallback   (s_thermMonitorTimerCallbackExelwte)
    GCC_ATTRIB_SECTION("imem_thermLibMonitor", "s_thermMonitorTimerCallbackExelwte")
    GCC_ATTRIB_USED();

static OsTmrCallbackFunc (s_thermMonitorTimerOsCallbackExelwte)
    GCC_ATTRIB_SECTION("imem_thermLibMonitor", "s_thermMonitorTimerOsCallbackExelwte")
    GCC_ATTRIB_USED();

/*!
 * @brief   Update THRM_MON counter
 *
 * @Note    Caller is responsible for attaching libLw64 and libLw64Umul overlays
 *
 * @param[in]   pThrmMon          THERM monitor object
 * @param[in]   pCounter          Buffer to store the value of updated SW counter
 * @param[in]   pEngagedTimens    Buffer to store the value of updated engaged time (ns)
 *
 * @return  FLCN_OK                 Valid counter value updated
 * @return  FLCN_ERR_ILWALID_STATE  Counter has overflowed
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_thermMonCountAndEngagedTimeUpdate
(
    THRM_MON          *pThrmMon,
    LwU64             *pCounter,
    FLCN_TIMESTAMP    *pEngagedTimens
)
{
    FLCN_STATUS status;

    appTaskCriticalEnter();
    {
        LwU32 count;
        LwU64 temp = 0;
        LwU64 multiplier = 1000000;
        LwU64 utilsClkFreqKhz = Therm.monData.utilsClkFreqKhz;

        // Get the HW count value.
        status = thermMonCountGet_HAL(&Therm, pThrmMon, &count);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
        else
        {
            // Update the 64-bit SW counter.
            lw64Add32(&pThrmMon->counter, count);

            if (pCounter != NULL)
            {
                *pCounter = pThrmMon->counter;
            }

            //
            // Compute engaged time in nanoseconds from HW count
            //
            // Time (ns) = count / utilclkkHz * 10^6
            //           = ((count * 10^6) / utilsClkFreqKhz)
            //
            // Update running engagedTimens timer
            //
            // engagedTime (ns) += Time (ns)
            //
            // Note: No overflow while computing engagedTimens, 
            //       64-bit engagedTimens overflows normally once it reaches LwU64_MAX
            //

            // 64.0 = 32.0
            temp = count;

            // 52.0 = 32.0 * 20.0
            lwU64Mul(&temp, &temp, &multiplier);

            // 35.0 = 52.0 / 17.0
            lwU64Div(&temp, &temp, &utilsClkFreqKhz);

            // 64.0 = 64.0 + 35.0
            lw64Add(&pThrmMon->engagedTimens, &pThrmMon->engagedTimens, &temp);

            if (pEngagedTimens != NULL)
            {
                pEngagedTimens->data = pThrmMon->engagedTimens;
            }
        }
    }
    appTaskCriticalExit();

    return status;
}
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * THRM_MON handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
thrmMonitorBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        THRM_MON,                                   // _class
        pBuffer,                                    // _pBuffer
        thrmThrmMonIfaceModel10SetHeader,                 // _hdrFunc
        thrmThrmMonIfaceModel10SetEntry,                  // _entryFunc
        all.therm.thermMonitorGrpSet,               // _ssElement
        OVL_INDEX_DMEM(therm),                      // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}


/*!
 * Queries all THERM_MONITORS.
 */
FLCN_STATUS
thrmMonitorBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32, // _grpType
        THRM_MON,                               // _class
        pBuffer,                                // _pBuffer
        thrmThrmMonIfaceModel10GetStatusHeader,             // _hdrFunc
        thrmThrmMonIfaceModel10GetStatus,                   // _entryFunc
        all.therm.thermMonitorGrpGetStatus);    // _ssElement
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
thrmThrmMonIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10          *pModel10,
    RM_PMU_BOARDOBJGRP   *pBuf,
    BOARDOBJGRPMASK      *pMask
)
{
    s_thermMonitorTimerOsCallbackExelwte(NULL);

    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
thrmThrmMonIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_THERM_THERM_MONITOR_BOARDOBJ_GET_STATUS *pGetStatus;
    THRM_MON   *pMon;
    FLCN_STATUS status;

    status = boardObjIfaceModel10GetStatus(pModel10, pBuf);
    if (status != FLCN_OK)
    {
        goto thrmQueryThrmMonEntry_exit;
    }
    pMon       = (THRM_MON *)pBoardObj;
    pGetStatus = (RM_PMU_THERM_THERM_MONITOR_BOARDOBJ_GET_STATUS *)pBuf;

    // Return counter value.
    LW64_FLCN_U64_PACK(&pGetStatus->counter, &pMon->counter);
    LW64_FLCN_U64_PACK(&pGetStatus->engagedTimens, &pMon->counter);

thrmQueryThrmMonEntry_exit:
    return status;
}

/*!
 * @Copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
thrmThrmMonIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_THERM_THERM_MONITOR_BOARDOBJGRP_SET_HEADER  *pSetHdr;
    FLCN_STATUS                                         status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto thrmThrmMonIfaceModel10SetHeader_exit;
    }

    pSetHdr = (RM_PMU_THERM_THERM_MONITOR_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    Therm.monData.utilsClkFreqKhz = pSetHdr->utilsClkFreqKhz;

    THERM_MONITOR_PATCH_ID_MASK_SET(&Therm.monData, pSetHdr->patchIdMask);

thrmThrmMonIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
thrmThrmMonIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    RM_PMU_THERM_THERM_MONITOR_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_MONITOR_BOARDOBJ_SET *)pBuf;
    THRM_MON   *pThrmMon;
    FLCN_STATUS status;

    // Sanity check for phyInstIdx.
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pSet->phyInstIdx >= thermMonNumInstanceGet_HAL(&Therm))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto thrmMonConstruct_exit;
        }
    }

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, sizeof(THRM_MON), pBuf);
    if (status != FLCN_OK)
    {
        goto thrmMonConstruct_exit;
    }
    pThrmMon = (THRM_MON *)*ppBoardObj;

    // Set member variables.
    pThrmMon->phyInstIdx = pSet->phyInstIdx;

thrmMonConstruct_exit:
    return status;
}

/*!
 * @brief   Returns 64-bit counter for requested physical instance of monitor.
 *
 * @param[in]   pThrmMon    THERM monitor object
 * @param[out]  pCount      Buffer to store current counter value
 *
 * @return  FLCN_OK                 Valid counter value.
 * @return  FLCN_ERR_NOT_SUPPORTED  If the monitor type is configured as _LEVEL.
 * @return  FLCN_ERR_ILWALID_STATE  Counter has overflown.
 */
FLCN_STATUS
thrmMonCounter64Get
(
    THRM_MON   *pThrmMon,
    LwU64      *pCount
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);    

    // Ensure that the monitor counts edges.
    if (thermMonTypeGet_HAL(&Therm, pThrmMon->phyInstIdx) !=
        LW2080_CTRL_THERMAL_THERM_MONITOR_TYPE_EDGE)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto thrmMonCounter64Get_exit;
    }

    PMU_HALT_OK_OR_GOTO(status,
        s_thermMonCountAndEngagedTimeUpdate(pThrmMon, pCount, NULL),
        thrmMonCounter64Get_exit);

thrmMonCounter64Get_exit:

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @brief   Returns the 64-bit timer for requested physical instance of monitor.
 *
 * @param[in]   pThrmMon        THERM monitor object
 * @param[out]  pTimeTimer      Buffer to store current PTimer's time value [ns]
 * @param[out]  pTimeEngagedns  Buffer to store current engaged time value [ns]
 *
 * @return  FLCN_OK                 Valid counter value.
 * @retunr  FLCN_ERR_NOT_SUPPORTED  If the monitor type is configured as _EDGE.
 * @return  FLCN_ERR_ILWALID_STATE  Counter has overflown.
 */
FLCN_STATUS
thrmMonTimer64Get
(
    THRM_MON       *pThrmMon,
    FLCN_TIMESTAMP *pTimeTimer,
    FLCN_TIMESTAMP *pTimeEngagedns
)
{
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
    };

    // Ensure that the monitor type is _LEVEL.
    if (thermMonTypeGet_HAL(&Therm, pThrmMon->phyInstIdx) !=
        LW2080_CTRL_THERMAL_THERM_MONITOR_TYPE_LEVEL)
    {
         PMU_BREAKPOINT();
         status = FLCN_ERR_NOT_SUPPORTED;
         goto thrmMonTimer64Get_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    appTaskCriticalEnter();
    {
        // sample current ptimer
        osPTimerTimeNsLwrrentGet(pTimeTimer);

        // sample current engaged time (ns)
        status = s_thermMonCountAndEngagedTimeUpdate(pThrmMon, NULL, pTimeEngagedns);
    }
    appTaskCriticalExit();
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto thrmMonTimer64Get_exit;
    }

thrmMonTimer64Get_exit:
    return status;
}

/*!
 * @brief  Loads all THERM_MONITOR SW state and enables
 *         THERM_MONITOR HW state
 *
 * @return FLCN_OK
 *         All THERM_MONITOR SW state loaded successfully.
 */
FLCN_STATUS
thermMonitorsLoad(void)
{
    THRM_MON     *pThrmMon;
    LwU32         callbackDelayus;
    LwBoardObjIdx i;

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR_WRAP_AROUND))
    LwU32         monMaxCount;
#endif
    // If there are no valid therm monitors, do nothing
    if (boardObjGrpMaskIsZero(&(Therm.monData.monitors.objMask)))
    {
        goto thermMonitorsLoad_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR_PATCH))
    {
        thermMonPatch_HAL(&Therm, THERM_MONITOR_PATCH_ID_MASK_GET(&Therm.monData));
    }

    // Enable THERM_MONITORS to start counting their respective signals
    BOARDOBJGRP_ITERATOR_BEGIN(THRM_MON, pThrmMon, i, NULL)
    {
        thermMonEnablementSet_HAL(&Therm, pThrmMon->phyInstIdx, LW_TRUE);
    }
    BOARDOBJGRP_ITERATOR_END;

    //
    //  If UTILSCLK is running at 108 MHz (i.e. XTAL 27MHz * 4),
    //  the time to overflow is ~19.9s.
    //
    // The counter is 31 bits wide, so counter will overflow every
    // (2 ^ 31)/108000000  = 19.88 seconds. To ensure overflow does not
    // happen, divide this value by 2.
    //
    // Note: Since this counter is bound to overflow every ~19 seconds, we
    // conservatively update and clear it every ~9 seconds. This period cannot
    // be different for sleep and normal conditions. Since this period is high
    // enough, LPWR task won't be affected.
    //
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR_WRAP_AROUND))
    monMaxCount = thermMonMaxCountGet_HAL();
    callbackDelayus = ((monMaxCount / Therm.monData.utilsClkFreqKhz) /
         THRM_MON_OVERFLOW_PERIOD_DIVISOR) * 1000;
#else
    callbackDelayus = ((BIT(31) / Therm.monData.utilsClkFreqKhz) /
         THRM_MON_OVERFLOW_PERIOD_DIVISOR) * 1000;
#endif


#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCreate(&OsCBThermMonitor,                        // pCallback
        OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED ,  // type
        OVL_INDEX_IMEM(thermLibMonitor),                          // ovlImem
        s_thermMonitorTimerOsCallbackExelwte,                     // pTmrCallbackFunc
        LWOS_QUEUE(PMU, THERM),                                   // queueHandle
        callbackDelayus,                                          // periodNormalus
        callbackDelayus,                                          // periodSleepus
        OS_TIMER_RELAXED_MODE_USE_NORMAL,                         // bRelaxedUseSleep
        RM_PMU_TASK_ID_THERM);                                    // taskId
    osTmrCallbackSchedule(&OsCBThermMonitor);
#else
    osTimerScheduleCallback(
        &ThermOsTimer,                                            // pOsTimer
        THERM_OS_TIMER_ENTRY_THERM_MONITOR_CALLBACKS,             // index
        lwrtosTaskGetTickCount32(),                               // ticksPrev
        callbackDelayus,                                          // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_SKIP), // flags
        s_thermMonitorTimerCallbackExelwte,                       // pCallback
        NULL,                                                     // pParam
        OVL_INDEX_IMEM(thermLibMonitor));                         // ovlIdx
#endif

thermMonitorsLoad_exit:
    return FLCN_OK;
}

/*!
 * @brief    Cancels the thermal monitors' timer callback and disables all
 *           thermal monitors.
 */
void
thermMonitorsUnload(void)
{

    THRM_MON     *pThrmMon;
    LwBoardObjIdx i;

    // If there are no valid therm monitors, do nothing
    if (boardObjGrpMaskIsZero(&(Therm.monData.monitors.objMask)))
    {
        return;
    }

    // Disable therm monitors so that count stops for their respective signals
    BOARDOBJGRP_ITERATOR_BEGIN(THRM_MON, pThrmMon, i, NULL)
    {
        thermMonEnablementSet_HAL(&Therm, pThrmMon->phyInstIdx, LW_FALSE);
    }
    BOARDOBJGRP_ITERATOR_END;

    // Cancel all timer callbacks.
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCancel(&OsCBThermMonitor);
#else
    osTimerDeScheduleCallback(&ThermOsTimer,
        THERM_OS_TIMER_ENTRY_THERM_MONITOR_CALLBACKS);
#endif
}

/*!
 * @ref    OsTimerCallback
 */
static FLCN_STATUS
s_thermMonitorTimerOsCallbackExelwte
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS     status = FLCN_OK;
    THRM_MON       *pThrmMon;
    LwBoardObjIdx   i;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_LW64
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    // Get each THRM_MON object and cache the HW count value
    BOARDOBJGRP_ITERATOR_BEGIN(THRM_MON, pThrmMon, i, NULL)
    {
        PMU_HALT_OK_OR_GOTO(status,
            s_thermMonCountAndEngagedTimeUpdate(pThrmMon, NULL, NULL),
            s_thermMonitorTimerOsCallbackExelwte_exit);
    }
    BOARDOBJGRP_ITERATOR_END;

s_thermMonitorTimerOsCallbackExelwte_exit:

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @ref    OsTimerCallback
 */
static void
s_thermMonitorTimerCallbackExelwte
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_thermMonitorTimerOsCallbackExelwte(NULL);
}
