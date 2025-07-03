/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file       lwostask.c
 * @details    This file is responsible for providing additional functionality
 *             and adapting task creation, context switching, and other core
 *             RTOS operations to work in LWPU elwironments.
 */

/* ------------------------ System Includes --------------------------------- */
#ifndef EXCLUDE_LWOSDEBUG
#include "lwosdebug.h"
#endif // EXCLUDE_LWOSDEBUG
#include "lwoslayer.h"
#ifdef HS_OVERLAYS_ENABLED
#include "lwosselwreovly.h"
#endif // HS_OVERLAYS_ENABLED
#include "dmemovl.h"
#include "lwos_dma.h"
#include "lib_lw64.h"
#include "lwrtos.h"
#include "lwos_ovl_priv.h"
#include "linkersymbols.h"
#include "lwrtosHooks.h"

#ifdef UPROC_RISCV
#include "riscv_manuals.h"
#include <lwriscv/print.h>
#endif // UPROC_RISCV

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ Macro definitions ------------------------------- */
#ifdef SCHEDULER_2X
#ifdef SCHEDULER_2X_AUTO
/*!
 * The amount to scale the number of timer ticks a task is allowed to run before
 * allowing another task of equal or lower priority to run.
 */
#define LWOS_LOAD_TIME_TICKS_AUTO   1U
#else // SCHEDULER_2X_AUTO
/*!
 * This macro defines the multipler that is used to multiply the time it took to
 * load the task and therefore allowing the new scheduled task run time in
 * proportion to their load time. Through experiments we found that 6 is a good
 * multipiler number to reduce thrashing among existing PMU tasks (mid-2018).
 */
#define LWOS_LOAD_TIME_TICKS_MULTIPLIER   6U
#endif // SCHEDULER_2X_AUTO
#endif // SCHEDULER_2X

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
#ifdef SCHEDULER_2X
/*!
 * Accumulate the task loading time that might cause missing timer ticks.
 */
static LwU32 tickAdjustTime = 0;
#endif // SCHEDULER_2X
/* ------------------------ Public variables -------------------------------- */
#ifndef UPROC_RISCV
/*!
 * Points to private TCB of lwrrently running task.
 */
PRM_RTOS_TCB_PVT pPrivTcbLwrrent = NULL;
#endif // UPROC_RISCV

/*!
 * Handle to the Idle task.
 */
LwrtosTaskHandle OsTaskIdle;

/* ------------------------ Static Function Prototypes  --------------------- */
/* ------------------------ Public Functions  ------------------------------- */
#ifdef SCHEDULER_2X
/*!
 * @brief      Estimates the missed timer ticks based on the aclwmulated load
 *             time.
 *
 * @details    When callwlating the number of timer ticks it took to perform a
 *             context switch, only integer division is used. The remainder time
 *             is aclwmulated in @ref tickAdjustTime so that when the remainder
 *             time sufficiently aclwmulates, it is accounted for.
 *
 * @return     The number of timer ticks unaccounted from task loading time.
 */
LwU32
lwrtosHookTickAdjustGet(void)
{
    LwU32 retVal;

    retVal = tickAdjustTime / LWRTOS_TICK_PERIOD_NS;

    tickAdjustTime = tickAdjustTime % LWRTOS_TICK_PERIOD_NS;

    return retVal;
}
#endif // SCHEDULER_2X

#ifdef IS_SSP_ENABLED
/*!
 * Sets up a new global stack canary and saves it to the current task's TCB. The
 * global stack canary will be restored the next time the task is loaded,
 * allowing each task to have independent canary values.
 */
GCC_ATTRIB_NO_STACK_PROTECT()
void
lwosTaskSetupCanary(void)
{
# ifdef UPROC_RISCV
    PRM_RTOS_TCB_PVT pPrivTcbLwrrent = lwrtosTaskPrivateRegister->pvObject;
# endif // UPROC_RISCV

    lwrtosENTER_CRITICAL();
    {
        LWOS_SETUP_STACK_CANARY_VAR(__stack_chk_guard);
        pPrivTcbLwrrent->stackCanary = LW_PTR_TO_LWUPTR(__stack_chk_guard);
    }
    lwrtosEXIT_CRITICAL();
}

# if defined(UPROC_RISCV) && defined(IS_SSP_ENABLED_PER_TASK)
GCC_ATTRIB_NO_STACK_PROTECT()
sysKERNEL_CODE void
lwrtosHookSetTaskStackCanary(void)
{
    PRM_RTOS_TCB_PVT pPrivTcbLwrrent;

    lwrtosTaskGetLwTcbExt(NULL, (void**)&pPrivTcbLwrrent);

    __stack_chk_guard = LW_LWUPTR_TO_PTR((LwUPtr)pPrivTcbLwrrent->stackCanary);
}
# endif // if defined(UPROC_RISCV) && defined(IS_SSP_ENABLED_PER_TASK)

#endif // IS_SSP_ENABLED

/*!
 * @brief      Load the named task into IMEM and DMEM so that it may be
 *             exelwted.
 *
 * @param[in]  pxTask           Handle of the task to load. Specifying NULL will
 *                              request that the current running task be loaded.
 * @param[out] pxLoadTimeTicks  The time in OS ticks it took to load the task.
 *
 * @return     A strictly-negative integer if the overlay could not be loaded.
 *             In this case, the scheduler must select a new task to run.
 * @return     Zero if all IMEM/DMEM required was already loaded.
 * @return     A strictly-postive integer if the task was successfully loaded.
 */
#ifndef UPROC_RISCV
LwS32
osTaskLoad
(
    LwrtosTaskHandle pxTask,
    LwU32           *pxLoadTimeTicks
)
{
    FLCN_STATUS             status          = FLCN_OK;
    PRM_RTOS_TCB_PVT        pTcb;
    static PRM_RTOS_TCB_PVT pPrivTcbLoaded  = NULL;
    LwS32                   blocksLoaded    = 0;
    LwS32                   blocksLoadedSum = 0;
    LwU32                   ovlCntImem;
    LwBool                  bReload         = LW_FALSE;
    LwBool                  bHsEncrypted    = LW_FALSE;
    LwBool                  bHsOvlAttached  = LW_FALSE;

#ifdef SCHEDULER_2X
    FLCN_TIMESTAMP          timeLoadStarted;
    LwU32                   elapsed;

    osPTimerTimeNsLwrrentGet(&timeLoadStarted);
#endif // SCHEDULER_2X

    status = lwrtosTaskGetLwTcbExt(pxTask, (void**)&pPrivTcbLwrrent);
    if (status != FLCN_OK)
    {
#ifdef SAFERTOS
        lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32)status);
#else //SAFERTOS
        OS_HALT();
#endif //SAFERTOS
    }

    pTcb    = pPrivTcbLwrrent;
    bReload = (pTcb == pPrivTcbLoaded);
#ifdef HS_UCODE_ENCRYPTION
    bHsEncrypted = LW_TRUE;
#endif // HS_UCODE_ENCRYPTION

#ifndef EXCLUDE_LWOSDEBUG
    OsDebugEntryPoint.taskID = pTcb->taskID;
#endif // EXCLUDE_LWOSDEBUG

#ifdef HS_OVERLAYS_ENABLED
    if (pTcb->ovlCntImemHS != 0)
    {
        LwU32 i;
        LwU8 *pHsOvlList = &(pTcb->ovlList[pTcb->ovlCntImemLS]);

        for (i = 0; i < pTcb->ovlCntImemHS; i++)
        {
            if (pHsOvlList[i] != OVL_INDEX_ILWALID)
            {
                bHsOvlAttached = LW_TRUE;
                break;
            }
        }
    }
#endif // HS_OVERLAYS_ENABLED

    //
    // It is not necessary to reload the same task that was loaded previously
    // unless the task's 'reload' flag is set. Doing so is incredibly wasteful
    // since the task's code is guaranteed to already reside in IMEM / DMEM.
    //
    if (bReload &&
        !(bHsEncrypted && bHsOvlAttached))
    {
        if (pTcb->reloadMask == RM_RTOS_TCB_PVT_RELOAD_NONE)
        {
            goto osTaskLoad_exit;
        }
    }
#ifdef DMEM_VA_SUPPORTED
#ifndef ON_DEMAND_PAGING_BLK
    else
    {
        LwU32 i;

        for (i = 0; i < 8U; i++)
        {
            LwosOvlImemLoaded[i] = 0;
        }
    }
#endif // ON_DEMAND_PAGING_BLK
#endif // DMEM_VA_SUPPORTED

    ovlCntImem  = pTcb->ovlCntImemLS;
#ifdef HS_OVERLAYS_ENABLED
    ovlCntImem += pTcb->ovlCntImemHS;
#endif // HS_OVERLAYS_ENABLED

    //
    // With HS_UCODE_ENCRYPTION, encrypted HS IMEM overlays that have been
    // exelwted by the task when we get here will be decrypted in place, and
    // hence cannot be re-exelwted. Hence, we have to force load the HS
    // overlays every time.
    //
#ifdef DMEM_VA_SUPPORTED
    if (!bReload ||
        bHsEncrypted ||
        ((pTcb->reloadMask & RM_RTOS_TCB_PVT_RELOAD_IMEM) != RM_RTOS_TCB_PVT_RELOAD_NONE))
#endif // DMEM_VA_SUPPORTED
    {
#ifdef ON_DEMAND_PAGING_BLK
        blocksLoaded = lwosOvlImemLoadPinnedOverlayList(pTcb->ovlList, pTcb->ovlCntImemLS);
        if (blocksLoaded < 0)
        {
            goto osTaskLoad_exit;
        }
        blocksLoadedSum += blocksLoaded;
#else // ON_DEMAND_PAGING_BLK
        if (ovlCntImem != 0U)
        {
            blocksLoaded = lwosOvlImemLoadList(&pTcb->ovlList[0], pTcb->ovlCntImemLS, LW_FALSE);
            if (blocksLoaded < 0)
            {
                goto osTaskLoad_exit;
            }
            blocksLoadedSum += blocksLoaded;

#ifdef HS_OVERLAYS_ENABLED
            if (pTcb->ovlCntImemHS != 0)
            {
                blocksLoaded = lwosOvlImemLoadList(&pTcb->ovlList[pTcb->ovlCntImemLS], pTcb->ovlCntImemHS, LW_TRUE);
                if (blocksLoaded < 0)
                {
                    goto osTaskLoad_exit;
                }
                blocksLoadedSum += blocksLoaded;
            }
#endif // HS_OVERLAYS_ENABLED
        }
#endif // ON_DEMAND_PAGING_BLK
    }

#ifdef DMEM_VA_SUPPORTED
#ifdef ON_DEMAND_PAGING_BLK
        blocksLoaded = lwosOvlDmemLoadPinnedOverlayList(&pTcb->ovlList[ovlCntImem], pTcb->ovlCntDmem);
        if (blocksLoaded < 0)
        {
            goto osTaskLoad_exit;
        }
        blocksLoadedSum += blocksLoaded;
#else // ON_DEMAND_PAGING_BLK
    if (!bReload ||
        ((pTcb->reloadMask & RM_RTOS_TCB_PVT_RELOAD_DMEM) != RM_RTOS_TCB_PVT_RELOAD_NONE))
    {
        if (pTcb->ovlCntDmem != 0U)
        {
            blocksLoaded = lwosOvlDmemLoadList(&pTcb->ovlList[ovlCntImem], pTcb->ovlCntDmem);
            if (blocksLoaded < 0)
            {
                goto osTaskLoad_exit;
            }
            blocksLoadedSum += blocksLoaded;
        }
   }
#endif // ON_DEMAND_PAGING_BLK
#endif // DMEM_VA_SUPPORTED

    // success
    pTcb->reloadMask  = RM_RTOS_TCB_PVT_RELOAD_NONE;
    pPrivTcbLoaded    = pTcb;
    blocksLoaded      = blocksLoadedSum;
#ifdef IS_SSP_ENABLED
    __stack_chk_guard = LW_LWUPTR_TO_PTR((LwUPtr)pTcb->stackCanary);
#endif // IS_SSP_ENABLED

osTaskLoad_exit:
    if (blocksLoaded < 0)
    {
        if (lwosDmaIsSuspended())
        {
            vApplicationDmaSuspensionNoticeISR();
        }
    }

#ifdef SCHEDULER_2X
    elapsed = osPTimerTimeNsElapsedGet(&timeLoadStarted);

    if (elapsed > LWRTOS_TICK_PERIOD_NS)
    {
        tickAdjustTime += (elapsed - LWRTOS_TICK_PERIOD_NS);
    }

    if (NULL != pxLoadTimeTicks)
    {
#ifdef SCHEDULER_2X_AUTO
        //
        // For automotive platforms, use a constant number of ticks for
        // more deterministic scheduling.
        //
        *pxLoadTimeTicks = LWOS_LOAD_TIME_TICKS_AUTO;
#else // SCHEDULER_2X_AUTO
        // Colwert time in [ns] to 30us timer ticks.
        *pxLoadTimeTicks =
            (LWOS_LOAD_TIME_TICKS_MULTIPLIER * elapsed) / LWRTOS_TICK_PERIOD_NS;
#endif //SCHEDULER_2X_AUTO
    }
#endif // SCHEDULER_2X
    return blocksLoaded;
}
#endif // UPROC_RISCV

/*!
 * @brief      Returns the ID of the lwrrently running task.
 *
 * @note       If this method is called before any task has been loaded, then
 *             the core will HALT.
 *
 * @return     The RM_PMU_TASK_ID_<xyz> ID of the lwrrently running task.
 */
LwU8
osTaskIdGet_IMPL(void)
{
#ifdef UPROC_RISCV
    PRM_RTOS_TCB_PVT pPrivTcbLwrrent = lwrtosTaskPrivateRegister->pvObject;
#endif // UPROC_RISCV

    if (NULL == pPrivTcbLwrrent)
    {
        OS_HALT();
        return 0;
    }

    return pPrivTcbLwrrent->taskID;
}

/*!
 * Create a new task and add it to the list of tasks that are ready to run. This
 * is a wrapper for @ref lwrtosTaskCreate that will also setup the private TCB
 * data and add it into linked list allowing RM to traverse private TCB
 * information.
 *
 * @param[out]     pxTaskHandle               Handle by which the created task
 *                                            can be referenced.
 * @param[in, out] pTcbPvt                    Pointer to the space allocated for
 *                                            the LW private TCB extension.
 * @param[in]      pvTaskCode                 Pointer to the task entry
 *                                            function. Tasks must be
 *                                            implemented to never return (i.e.
 *                                            continuous loop).
 * @param[in]      taskID                     A numeric identifier for each
 *                                            task.
 * @param[in]      usStackDepth               The size of the task stack
 *                                            specified as the number of
 *                                            variables the stack can hold - not
 *                                            the number of bytes. For example,
 *                                            if the stack is 16 bits wide and
 *                                            usStackDepth is defined as 100,
 *                                            200 bytes will be allocated for
 *                                            stack storage.
 * @param[in]      bUsingFPU                  Enable FPU hardware for selected
 *                                            task.
 * @param[in]      uxPriority                 The priority at which the task
 *                                            should run.
 * @param[in]      privLevelExt               Task EXT privilege level.
 * @param[in]      privLevelCsb               Task CSB privilege level.
 * @param[in]      ovlCntImemLS               The maximum number of LS IMEM
 *                                            overlays that the task can ever
 *                                            expect to have attached to it at
 *                                            one time. Care should be taken to
 *                                            set this number to its exact
 *                                            value. Overspecifying the value
 *                                            will waste memory. This value may
 *                                            be zero for resident tasks that do
 *                                            not require overlays.
 * @param[in]      ovlCntImemHS               The maximum number of HS IMEM
 *                                            overlays that the task can ever
 *                                            expect to have attached to it at
 *                                            one time. Care should be taken to
 *                                            set this number to its exact
 *                                            value. Overspecifying the value
 *                                            will waste memory. This value may
 *                                            be zero for tasks that do not
 *                                            require HS overlays.
 * @param[in]      ovlIdxImem                 Index of the task's primary IMEM
 *                                            overlay that is always attached.
 * @param[in]      ovlCntDmem                 The maximum number of DMEM
 *                                            overlays that the task can ever
 *                                            expect to have attached to it at
 *                                            one time. Care should be taken to
 *                                            set this number to its exact
 *                                            value. Overspecifying the value
 *                                            will waste memory. This value may
 *                                            be zero for resident tasks that do
 *                                            not require overlays.
 * @param[in]      ovlIdxStack                Index of the task's STACK overlay
 *                                            that is always attached.
 * @param[in]      ovlIdxDmem                 Index of the task's primary DMEM
 *                                            overlay that is always attached.
 * @param[in]      bEnableImemOnDemandPaging  Enable IMEM on-demand paging. If
 *                                            disabled, falcon will HALT on IMEM
 *                                            miss
 *
 * @return     FLCN_OK if successful
 * @return     Bubbled up error codes from sub-functions if any fail.
 */
FLCN_STATUS
lwosTaskCreate_IMPL
(
    LwrtosTaskHandle       *pxTaskHandle,
    PRM_RTOS_TCB_PVT        pTcbPvt,
    LwrtosTaskFunction      pvTaskCode,
    LwU8                    taskID,
    LwU16                   usStackDepth,
    LwBool                  bUsingFPU,
    LwU32                   uxPriority,
    LwU8                    privLevelExt,
    LwU8                    privLevelCsb,
    LwU8                    ovlCntImemLS,
    LwU8                    ovlCntImemHS,
    LwU8                    ovlIdxImem,
    LwU8                    ovlCntDmem,
    LwU8                    ovlIdxStack,
    LwU8                    ovlIdxDmem,
    LwBool                  bEnableImemOnDemandPaging
)
{
    FLCN_STATUS         status = FLCN_ERR_NO_FREE_MEM;
    LwUPtr             *pStack = NULL;
    LwrtosTaskHandle    pCreatedTask;

    //
    // An array of overlay indexes must be initialized to OVL_INDEX_ILWALID!
    // Lwrrently it's done at allocation time since OVL_INDEX_ILWALID == 0.
    //
#if (OVL_INDEX_ILWALID != 0x00)
    #error Someone has changed the define for OVL_INDEX_ILWALID. Please initialize pData->ovlList[i]
#endif // (OVL_INDEX_ILWALID != 0x00)

    if (pxTaskHandle == NULL || pTcbPvt == NULL) {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto lwosTaskCreate_exit;
    }

    // Ignore below codes for RISCV build since RM is not ready to use it
#if UPROC_FALCON
#ifndef EXCLUDE_LWOSDEBUG
    // Add newly created private TCB into linked list.
    pTcbPvt->pPrivTcbNext          = OsDebugEntryPoint.pPrivTcbHead;
    OsDebugEntryPoint.pPrivTcbHead = RM_FALC_PTR_TO_OFS(pTcbPvt);
#endif // EXCLUDE_LWOSDEBUG
#endif // UPROC_FALCON

#ifdef DYNAMIC_FLCN_PRIV_LEVEL
    // update the task privilege level
    pTcbPvt->privLevel =
        DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL, _EXT_DEFAULT, privLevelExt) |
        DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL, _CSB_DEFAULT, privLevelCsb) |
        DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL, _EXT_LWRRENT, privLevelExt) |
        DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL, _CSB_LWRRENT, privLevelCsb);
#endif // DYNAMIC_FLCN_PRIV_LEVEL

#if defined(DMEM_VA_SUPPORTED)
    if (ovlIdxStack == OVL_INDEX_ILWALID)
#endif // DMEM_VA_SUPPORTED
    {
        // Allocate task's stack (required with DMEM_VA for the IDLE task).
        pStack = lwosCallocResidentType(usStackDepth, LwUPtr);
        if (NULL == pStack)
        {
            // Return FLCN_ERR_NO_FREE_MEM
            goto lwosTaskCreate_exit;
        }
    }
#if defined(DMEM_VA_SUPPORTED)
    else
    {
#ifdef UPROC_RISCV
        status = mmTaskStackSetup(ovlIdxStack, &usStackDepth, &pStack);
#else // UPROC_RISCV
        status = lwosOvlDmemStackSetup(ovlIdxStack, &usStackDepth, &pStack);
#endif // UPROC_RISCV
        if (status != FLCN_OK)
        {
            goto lwosTaskCreate_exit;
        }
        pTcbPvt->ovlIdxStack = ovlIdxStack;
    }
#endif // defined(DMEM_VA_SUPPORTED) && !defined(FAKE_OVERLAY_SUPPORT)

    //
    // In SafeRTOS, we clear the stack in vPortInitialiseTask, called by xTaskCreate.
    // Do not clear the stack before calling xTaskCreate, because it checks if the
    // stack is in use and will error because of the fill value we use.
    //
#if !defined(SAFERTOS) && !defined(EXCLUDE_LWOSDEBUG)
    //
    // Prefill stack with a known pattern. We use the known pattern to detect
    // stack usages. Additionally, we use it to detect stack overflows on older
    // hardware where HW overflow detection was unavailable.
    //
    osDebugStackPreFill(pStack, usStackDepth);
#endif // !defined(SAFERTOS) && !defined(EXCLUDE_LWOSDEBUG)

    //
    // Since RISCV overlays are paged we can't reasonably share pStack with RM for now,
    // and it's not used by ucode at all so set it to LwU32.
    // MMINTS-TODO: find a way to share pStack with RM!
    //
    pTcbPvt->pStack     = (LwU32)(LwUPtr)pStack;
    pTcbPvt->stackSize  = (LwU16)usStackDepth;

    pTcbPvt->taskID       = taskID;
    pTcbPvt->ovlCntImemLS = ovlCntImemLS;

#ifdef HS_OVERLAYS_ENABLED
    pTcbPvt->ovlCntImemHS = ovlCntImemHS;
#else // HS_OVERLAYS_ENABLED
    pTcbPvt->ovlCntImemHS = 0;
#endif // HS_OVERLAYS_ENABLED

#ifdef DMEM_VA_SUPPORTED
    pTcbPvt->ovlCntDmem = ovlCntDmem;
#endif // DMEM_VA_SUPPORTED
    pTcbPvt->bPagingOnDemandImem = bEnableImemOnDemandPaging;

#ifdef IS_SSP_ENABLED
    pTcbPvt->stackCanary = LW_PTR_TO_LWUPTR(__stack_chk_guard);
#endif // IS_SSP_ENABLED

    const LwrtosTaskCreateParameters lwrtosTaskCreateParameters = {
        .pTaskCode = pvTaskCode,
        .taskID = taskID,
        .pTaskStack = pStack,
        .stackDepth = usStackDepth,
        .pParameters = lwrtosTASK_FUNC_ARG_COLD_START,
        .priority = uxPriority,
        .pTcbPvt = pTcbPvt,
#ifdef FPU_SUPPORTED
        .bUsingFPU = bUsingFPU,
#endif // FPU_SUPPORTED
    };

    status = lwrtosTaskCreate(&pCreatedTask, &lwrtosTaskCreateParameters);
    if (status != FLCN_OK)
    {
        goto lwosTaskCreate_exit;
    }

    // Attach task's primary IMEM overlay (unless task has none, i.e. resident).
    if (OVL_INDEX_ILWALID != ovlIdxImem)
    {
        OSTASK_ATTACH_PINNED_IMEM_OVERLAY_TO_TASK(pCreatedTask, ovlIdxImem);
    }

#ifdef DMEM_VA_SUPPORTED
    if (OVL_INDEX_ILWALID != ovlIdxStack)
    {
        OSTASK_ATTACH_PINNED_DMEM_OVERLAY_TO_TASK(pCreatedTask, ovlIdxStack);
    }

    // Attach task's primary DMEM overlay (unless task has none).
    if (OVL_INDEX_ILWALID != ovlIdxDmem)
    {
        OSTASK_ATTACH_PINNED_DMEM_OVERLAY_TO_TASK(pCreatedTask, ovlIdxDmem);
    }
#endif // DMEM_VA_SUPPORTED

    *pxTaskHandle = pCreatedTask;

    // Global tracking of the created tasks.
    status = lwosTaskCreated();
    if (status != FLCN_OK)
    {
        goto lwosTaskCreate_exit;
    }

lwosTaskCreate_exit:
    return status;
}


/*! GSP RISCV has its own implementation of Idle task creation, so compiling it out here */
#if !defined(GSPLITE_RTOS) || !defined(UPROC_RISCV)
/*!
 * @brief      Create the Idle task.
 *
 * @details    The Idle Task is special compared to other tasks because it must
 *             always be present in memory, the lowest priority task, and
 *             created by the RTOS.
 *
 * @param[in]  pvTaskCode  Function pointer for the Idle task.
 * @param[in]  uxPriority  The priority for the Idle task.
 */
void
vApplicationIdleTaskCreate
(
    LwrtosTaskFunction  pvTaskCode,
    LwU32               uxPriority
)
{
#define taskIDLE_TASK_ID            0U
#define taskIDLE_PRIV_LEVEL_EXT     ( ( LwU8 ) 0 )
#define taskIDLE_PRIV_LEVEL_CSB     ( ( LwU8 ) 0 )
    GCC_ATTRIB_SECTION("dmem_residentLwos", "LwosTcbPvt_IDLE")
    static RM_RTOS_TCB_PVT LwosTcbPvt_IDLE;

    if (FLCN_OK != lwosTaskCreate(&OsTaskIdle, &(LwosTcbPvt_IDLE),
                                  pvTaskCode, taskIDLE_TASK_ID,
                                  vApplicationIdleTaskStackSize(),
                                  LW_FALSE, uxPriority,
                                  taskIDLE_PRIV_LEVEL_EXT,
                                  taskIDLE_PRIV_LEVEL_CSB,
                                  0, 0, 0, 0, 0, 0, LW_FALSE))
    {
        OS_HALT();
    }
}
#endif // !defined(GSPLITE_RTOS) || !defined(UPROC_RISCV)

#if defined(DYNAMIC_FLCN_PRIV_LEVEL)
/*!
 * Sets the Falcon HW privilege level based on the current scheduled task privilege
 * level
 */
void
osFlcnPrivLevelSetFromLwrrTask(void)
{
#ifdef UPROC_RISCV
    PRM_RTOS_TCB_PVT pPrivTcbLwrrent;
    lwrtosTaskGetLwTcbExt(NULL, (void**)&pPrivTcbLwrrent);
#endif // UPROC_RISCV

    LwU8 privLevelExt = DRF_VAL(_RTOS_TCB_PVT, _PRIV_LEVEL, _EXT_LWRRENT,
                                pPrivTcbLwrrent->privLevel);
    LwU8 privLevelCsb = DRF_VAL(_RTOS_TCB_PVT, _PRIV_LEVEL, _CSB_LWRRENT,
                                pPrivTcbLwrrent->privLevel);

    vApplicationFlcnPrivLevelSet(privLevelExt, privLevelCsb);
}

#ifdef UPROC_RISCV
void
lwrtosHookPrivLevelSet(void)
{
    osFlcnPrivLevelSetFromLwrrTask();
}
#endif // UPROC_RISCV

/*!
 * Resets the Falcon HW privilege level to power on defaults.
 */
void
osFlcnPrivLevelReset(void)
{
    vApplicationFlcnPrivLevelReset();
}

/*!
 * @brief      Set the lwrrently running task's Falcon HW privilege level to new
 *             level
 *
 * @note       This method does nothing when DYNAMIC_FLCN_PRIV_LEVEL is not set.
 *
 * @param[in]  newPrivLevelExt  The new EXT priv level.
 * @param[in]  newPrivLevelCsb  The new CSB priv level.
 */
void
osFlcnPrivLevelLwrrTaskSet
(
    LwU8 newPrivLevelExt,
    LwU8 newPrivLevelCsb
)
{
#ifdef UPROC_RISCV
    PRM_RTOS_TCB_PVT pPrivTcbLwrrent = lwrtosTaskPrivateRegister->pvObject;
#endif // UPROC_RISCV

    pPrivTcbLwrrent->privLevel = FLD_SET_DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL,
        _EXT_LWRRENT, newPrivLevelExt, pPrivTcbLwrrent->privLevel);
    pPrivTcbLwrrent->privLevel = FLD_SET_DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL,
        _CSB_LWRRENT, newPrivLevelCsb, pPrivTcbLwrrent->privLevel);

    vApplicationFlcnPrivLevelSet(newPrivLevelExt, newPrivLevelCsb);
}

/*!
 * @brief      Get the lwrrently running task's Falcon HW privilege level.
 *
 * @note       This method does nothing when DYNAMIC_FLCN_PRIV_LEVEL is not set.
 *
 * @param[out] pPrivLevelExt  The lwrrently running tasks's EXT priv level.
 * @param[out] pPrivLevelCsb  The lwrrently running tasks's CSB priv level.
 */
void
osFlcnPrivLevelLwrrTaskGet
(
    LwU8 *pPrivLevelExt,
    LwU8 *pPrivLevelCsb
)
{
#ifdef UPROC_RISCV
    PRM_RTOS_TCB_PVT pPrivTcbLwrrent = lwrtosTaskPrivateRegister->pvObject;
#endif // UPROC_RISCV

    *pPrivLevelExt = DRF_VAL(_RTOS_TCB_PVT, _PRIV_LEVEL, _EXT_LWRRENT,
                             pPrivTcbLwrrent->privLevel);
    *pPrivLevelCsb = DRF_VAL(_RTOS_TCB_PVT, _PRIV_LEVEL, _CSB_LWRRENT,
                             pPrivTcbLwrrent->privLevel);
}

/*!
 * @brief      Reset the lwrrently running task's Falcon HW privilege level to
 *             the one specified at task creation.
 *
 * @note       This method does nothing when DYNAMIC_FLCN_PRIV_LEVEL is not set.
 */
void
osFlcnPrivLevelLwrrTaskReset(void)
{
#ifdef UPROC_RISCV
    PRM_RTOS_TCB_PVT pPrivTcbLwrrent = lwrtosTaskPrivateRegister->pvObject;
#endif // UPROC_RISCV

    LwU8 privLevelExt = DRF_VAL(_RTOS_TCB_PVT, _PRIV_LEVEL, _EXT_DEFAULT,
                                pPrivTcbLwrrent->privLevel);
    LwU8 privLevelCsb = DRF_VAL(_RTOS_TCB_PVT, _PRIV_LEVEL, _CSB_DEFAULT,
                                pPrivTcbLwrrent->privLevel);

    pPrivTcbLwrrent->privLevel = FLD_SET_DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL,
        _EXT_LWRRENT, privLevelExt, pPrivTcbLwrrent->privLevel);
    pPrivTcbLwrrent->privLevel = FLD_SET_DRF_NUM(_RTOS_TCB_PVT, _PRIV_LEVEL,
        _CSB_LWRRENT, privLevelCsb, pPrivTcbLwrrent->privLevel);

    vApplicationFlcnPrivLevelSet(privLevelExt, privLevelCsb);
}
#endif //defined(DYNAMIC_FLCN_PRIV_LEVEL)

/*!
 * @brief      Initialize LWOS related data.
 *
 * @return     FLCN_OK     On success
 * @return     Others      Bubbles up errors
 */
FLCN_STATUS
lwosInit_IMPL(void)
{
    FLCN_STATUS status;

    // Must create and prepare the scheduler before any interactions with it.
    status = lwrtosTaskSchedulerInitialize();
    if (FLCN_OK != status)
    {
        goto lwosInit_exit;
    }

#ifdef UPROC_FALCON
#ifndef EXCLUDE_LWOSDEBUG
    // Fill the ISR stack with predetermined pattern before using it.
    osDebugISRStackInit();

#ifndef HW_STACK_LIMIT_MONITORING
    // Fill the ESR stack top with predetermined pattern before using it.
    osDebugESRStackInit();
#endif // HW_STACK_LIMIT_MONITORING
#endif // EXCLUDE_LWOSDEBUG

    status = lwosInitOvl();
    if (FLCN_OK != status)
    {
        goto lwosInit_exit;
    }
#endif // UPROC_FALCON
lwosInit_exit:
    return status;
}

/*!
 * @brief      Maps VA to PA and stores in pOffset.
 *
 * @param[in]  pVAddr   Pointer to virtual address for which physical memory
 *                      offset is mapped.
 * @param[out] pOffset  Pointer to store mapped physical memory offset.
 *
 * @return     FLCN_OK     On success
 * @return     FLCN_ERR_OUT_OF_RANGE If the address is out of the resident range.
 */
#ifdef UPROC_RISCV
FLCN_STATUS
lwosResVAddrToPhysMemOffsetMap
(
    void  *pVAddr,
    LwU32 *pOffset
)
{
    FLCN_STATUS status = FLCN_OK;

    // Check whether the address is within TASK_SHARED_DATA_DMEM_RES
    if (LWOS_SECTION_CONTAINS_VA(UPROC_SHARED_DMEM_SECTION_NAME, pVAddr))
    {
        *pOffset = (LwUPtr)(LWOS_SHARED_MEM_VA_TO_PA(pVAddr) - LW_RISCV_AMAP_DMEM_START);
    }
#ifdef UPROC_SECTION_dmem_osHeap_REF
    // Check whether the address is within OS_HEAP
    else if (LWOS_SECTION_CONTAINS_VA(dmem_osHeap, pVAddr))
    {
        *pOffset = (LwUPtr)(LWOS_SECTION_VA_TO_PA(dmem_osHeap, pVAddr) - LW_RISCV_AMAP_DMEM_START);
    }
#endif // UPROC_SECTION_dmem_osHeap_REF
#ifdef UPROC_SECTION_dmem_lpwr_REF
    // Check whether the address is within LPWR
    else if (LWOS_SECTION_CONTAINS_VA(dmem_lpwr, pVAddr))
    {
        *pOffset = (LwUPtr)(LWOS_SECTION_VA_TO_PA(dmem_lpwr, pVAddr) - LW_RISCV_AMAP_DMEM_START);
    }
#endif // UPROC_SECTION_dmem_lpwr_REF
    else
    {
        status = FLCN_ERR_OUT_OF_RANGE;
    }

    return status;
}
#else // UPROC_RISCV

/*!
 * @brief      Maps VA to PA and stores in pOffset.
 *
 * @param[in]  pVAddr   Pointer to virtual address for which physical memory
 *                      offset is mapped.
 * @param[out] pOffset  Pointer to store mapped physical memory offset.
 *
 * @return     FLCN_OK     On success
 * @return     FLCN_ERR_OUT_OF_RANGE If the address is out of the resident range.
 */
FLCN_STATUS
lwosResVAddrToPhysMemOffsetMap
(
    void  *pVAddr,
    LwU32 *pOffset
)
{
    FLCN_STATUS status = FLCN_OK;

    // Check whether the address is within resident dmem range.
    if (LW_PTR_TO_LWUPTR(pVAddr) < (LwUPtr)_end_physical_dmem)
    {
        // On Falcon, VA is directly mapped to PA offset.
        *pOffset = (LwU32)LW_PTR_TO_LWUPTR(pVAddr);
    }
    else
    {
        status = FLCN_ERR_OUT_OF_RANGE;
    }

    return status;
}
#endif // UPROC_RISCV

/*!
 * @brief      Tasks' top-level event exelwtion error handler.
 *
 *             Tasks can fail (raise an error) while exelwting various events
 *             (signals, timer callbacks, RPCs, ...)). These errors can be
 *             either handled locally within the respective task, relayed back
 *             to requester, or when nothing else is possible propagated to the
 *             task's main loop and handed over to this interface to raise a
 *             critical error notification. Such error notification will trigger
 *             predefined recovery procedure that may vary based on the target
 *             product.
 *
 * @param[in]  status  Task's event exelwtion status.
 *
 * @note       This interface is NOP in case status == FLCN_OK.
 * @note       This interface is NOP for non-PMU clients (not covered by the
 *             current PMU resilience initiatives).
 */
void
lwosTaskEventErrorHandler_IMPL
(
    FLCN_STATUS status
)
{
#ifdef PMU_RTOS
    if (status != FLCN_OK)
    {
        // NJ-TODO-CB
        OS_HALT();
    }
#endif // PMU_RTOS
}

/* ------------------------ Private Functions ------------------------------- */
