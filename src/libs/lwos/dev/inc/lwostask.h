/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSTASK_H
#define LWOSTASK_H

/*!
 * @file    lwostask.h
 * @copydoc lwostask.c
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"
#include "lwctassert.h"
#include "lwos_dma.h"
#include "lwrtos.h"
#include "lwos_utility.h"
#include "lwos_ovl.h"
#include "falcon.h"
#include "osptimer.h"
#include "lw_rtos_extension.h"
#include "lwoswatchdog.h"
#include "lwostmrcallback.h"
#include "lwos_instrumentation.h"
#include "lwos_ustreamer.h"
#include "lwos_task_sync.h"

/* ------------------------ Application includes ---------------------------- */
#if defined(SEC2_CSB_ACCESS)
#include "dev_sec_csb.h"
#elif defined(SOE_CSB_ACCESS)
#include "dev_soe_csb.h"
#elif defined(GSPLITE_CSB_ACCESS)
#include "dev_gsp_csb.h"
#else // defined(SEC2_CSB_ACCESS)
#include "dev_pwr_falcon_csb.h"
#endif // defined(SEC2_CSB_ACCESS)

#ifdef DMEM_VA_SUPPORTED
#include "dev_falcon_v4.h"
#endif // DMEM_VA_SUPPORTED

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief      Statically declare the private TCB for a specific task at compile
 *             time.
 *
 * @details    Through static allocation of the private TCB for each task, we
 *             have easier callwlated DMEM usage, and isolate all to a single
 *             section.
 *
 * @param      name  The name of the task.
 */
#define OSTASK_CREATE_TCB_PVT_DECLARE(name)                                    \
    GCC_ATTRIB_SECTION("dmem_residentLwos", "LwosTcbPvt_" #name)               \
    static struct {                                                            \
        RM_RTOS_TCB_PVT super;                                                 \
        LwU8            ext[OSTASK_ATTRIBUTE_##name##_MAX_OVERLAYS_IMEM_LS +   \
                            OSTASK_ATTRIBUTE_##name##_MAX_OVERLAYS_IMEM_HS +   \
                            OSTASK_ATTRIBUTE_##name##_MAX_OVERLAYS_DMEM];      \
    } LwosTcbPvt_##name

/*
 * @brief      Macro to more neatly ilwoke @ref lwosTaskCreate with the task
 *             specific parameters that are defined at compile time.
 *
 * @note       Need to update /chips_a/pmu_sw/build/Tasks.pm to specify what
 *             tasks can use the FPU
 *
 * @param      status              Variable to store the status returned by
 *                                 lwosTaskCreate.
 * @param      falcon              Name of the microcontroller the task is being
 *                                 created for.
 * @param      name                The name of the task.
 */
#define OSTASK_CREATE(status, falcon, name)                                    \
    do {                                                                       \
        OSTASK_CREATE_TCB_PVT_DECLARE(name);                                   \
        ct_assert(OSTASK_ATTRIBUTE_##name##_MAX_OVERLAYS_IMEM_LS <=            \
                  RM_FALC_MAX_ATTACHED_OVERLAYS_IMEM);                         \
        status = lwosTaskCreate(                                               \
            &OSTASK_ATTRIBUTE_##name##_TCB,                                    \
            &(LwosTcbPvt_##name.super),                                        \
            (LwrtosTaskFunction)OS_LABEL(OSTASK_ATTRIBUTE_##name##_ENTRY_FN),  \
            RM_##falcon##_TASK_ID_##name,                                      \
            OSTASK_ATTRIBUTE_##name##_STACK_SIZE / sizeof(LwUPtr),        \
            OSTASK_ATTRIBUTE_##name##_USING_FPU,                               \
            OSTASK_ATTRIBUTE_##name##_PRIORITY + lwrtosTASK_PRIORITY_IDLE,     \
            OSTASK_ATTRIBUTE_##name##_PRIV_LEVEL_EXT,                          \
            OSTASK_ATTRIBUTE_##name##_PRIV_LEVEL_CSB,                          \
            OSTASK_ATTRIBUTE_##name##_MAX_OVERLAYS_IMEM_LS,                    \
            OSTASK_ATTRIBUTE_##name##_MAX_OVERLAYS_IMEM_HS,                    \
            OSTASK_ATTRIBUTE_##name##_OVERLAY_IMEM,                            \
            OSTASK_ATTRIBUTE_##name##_MAX_OVERLAYS_DMEM,                       \
            OSTASK_ATTRIBUTE_##name##_OVERLAY_STACK,                           \
            OSTASK_ATTRIBUTE_##name##_OVERLAY_DMEM,                            \
            OSTASK_ATTRIBUTE_##name##_TASK_IMEM_PAGING);                       \
    } while (LW_FALSE)

/*!
 * Creates a set of new worker threads via @ref lwosTaskCreate using the worker
 * thread attributes defined in the worker thread definitions in Tasks.pm
 *
 * @param[out] status     The FLCN_STATUS variable to hold the exelwtion status.
 * @param[in]  falcon     The name of falcon. Possible values = { PMU, DPU,
 *                        SEC2, SOE }.
 * @param[in]  wkrThdCnt  The number of worker threads to create. In the future,
 *                        to allow for worker thread pools, we could pass in
 *                        another parameter which would be the pool number. The
 *                        macro relies that macros for the worker thread
 *                        attributes would be generated in the following format:
 *                        OSTASK_ATTRIBUTE_WKRTHD_<attribute>.
 */
#define OSWKRTHD_CREATE(status, falcon, wkrThdCnt)                             \
    do {                                                                       \
        GCC_ATTRIB_SECTION("dmem_residentLwos", "LwosTcbPvt_WKRTHD")           \
        static struct {                                                        \
            RM_RTOS_TCB_PVT super;                                             \
            LwU8            ext[OSTASK_ATTRIBUTE_WKRTHD_MAX_OVERLAYS_IMEM_LS + \
                                OSTASK_ATTRIBUTE_WKRTHD_MAX_OVERLAYS_IMEM_HS]; \
        } LwosTcbPvt_WTHREAD[wkrThdCnt];                                       \
        ct_assert((OSTASK_ATTRIBUTE_WKRTHD_MAX_OVERLAYS_IMEM_LS + 0) <=        \
                  RM_FALC_MAX_ATTACHED_OVERLAYS_IMEM);                         \
        int i = 0;                                                             \
        for (i = 0; i < wkrThdCnt; i++) {                                      \
            status = lwosTaskCreate(                                           \
                &OSTASK_ATTRIBUTE_WKRTHD_TCB_ARRAY[i],                         \
                &(LwosTcbPvt_WTHREAD[i].super),                                \
                PTR16(OSTASK_ATTRIBUTE_WKRTHD_ENTRY_FN),                       \
                RM_##falcon##_TASK_ID_WKRTHD,                                  \
                OSTASK_ATTRIBUTE_WKRTHD_STACK_SIZE / sizeof(LwUPtr),      \
                OSTASK_ATTRIBUTE_WKRTHD_USING_FPU,                             \
                OSTASK_ATTRIBUTE_WKRTHD_PRIORITY + lwrtosTASK_PRIORITY_IDLE,   \
                OSTASK_ATTRIBUTE_WKRTHD_PRIV_LEVEL_EXT,                        \
                OSTASK_ATTRIBUTE_WKRTHD_PRIV_LEVEL_CSB,                        \
                OSTASK_ATTRIBUTE_WKRTHD_MAX_OVERLAYS_IMEM_LS,                  \
                OSTASK_ATTRIBUTE_WKRTHD_MAX_OVERLAYS_IMEM_HS,                  \
                OSTASK_ATTRIBUTE_WKRTHD_OVERLAY_IMEM, 0, 0, 0,                 \
                OSTASK_ATTRIBUTE_WKRTHD_TASK_IMEM_PAGING);                     \
            if (FLCN_OK != status) { break; }                                  \
        }                                                                      \
    } while (LW_FALSE)

/*!
 * @brief      Macro for generating a task's autogenerated TCB define.
 *
 * @param      name  The name of the task.
 */
#define OSTASK_TCB(name)                                                       \
    OSTASK_ATTRIBUTE_##name##_TCB

/*!
 * @brief      Macro for generating a task's autogenerated IMEM overlay index
 *             define.
 *
 * @param      name  The name of the task.
 */
#define OSTASK_OVL_IMEM(name)                                                  \
    OSTASK_ATTRIBUTE_##name##_OVERLAY_IMEM

/*!
 * @brief      Macro for generating a task's autogenerated priority define.
 *
 * @param      name  The name of the task.
 */
#define OSTASK_PRIORITY(name)                                                  \
    OSTASK_ATTRIBUTE_##name##_PRIORITY

/*!
 * @brief      Macro for generating a task's autogenerated EXT HW priv level
 *             define.
 *
 * @param      name  The name of the task.
 */
#define OSTASK_PRIV_LEVEL_EXT(name)                                            \
    OSTASK_ATTRIBUTE_##name##_PRIV_LEVEL_EXT

/*!
 * @brief      Macro for generating a task's autogenerated CSB HW priv level
 *             define.
 *
 * @param      name  The name of the task.
 */
#define OSTASK_PRIV_LEVEL_CSB(name)                                            \
    OSTASK_ATTRIBUTE_##name##_PRIV_LEVEL_CSB

/*!
 * @brief      Macro for generating a task's autogenerated ID define.
 *
 * @param      name  The name of the task.
 */
#define LWOSTASK_ID(falcon, name)                                              \
    RM_##falcon##_TASK_ID_##name

#ifdef TASK_QUEUE_MAP

/*!
 * @brief      Macro for generating a task's queue ID.
 *
 * @param      name  The name of the task.
 */
#define LWOS_QUEUE_ID(falcon, name)                                            \
    falcon##_QUEUE_ID_##name

/*!
 * @brief      Macro for retrieving the task's queue handle.
 *
 * @param      name  The name of the task.
 */
#define LWOS_QUEUE(falcon, name)                                               \
    UcodeQueueIdToQueueHandleMap[LWOS_QUEUE_ID(falcon, name)]

#endif // TASK_QUEUE_MAP

/*!
 * @brief  Helper macro to setup canary in lwos task loop. Not meant to be used
 *         anywhere else.
 */
#if defined(IS_SSP_ENABLED) && defined(IS_SSP_ENABLED_PER_TASK)
#define LWOS_TASK_LOOP_SETUP_STACK_CANARY() lwosTaskSetupCanary()
#else
#define LWOS_TASK_LOOP_SETUP_STACK_CANARY() lwosNOP()
#endif

/*!
 * @brief   Used to generate the name of a command buffer
 *
 * @param[in]   core    Name of the core
 * @param[in]   task    Name of the task for the buffer
 *
 * @return  Command command buffer name
 */
#define LWOS_TASK_CMD_BUFFER(core, task)                                       \
    (core##_TASK_CMD_BUFFER_##task)

/*!
 * @brief   Generates definition of a static per-task command buffer.
 *
 * @param[in]   core    Name of the core
 * @param[in]   task    Name of the task for the buffer
 * @param[in]   section Section in which to place the buffer
 */
#define LWOS_TASK_CMD_BUFFER_DEFINE(core, task, section)                       \
    static struct                                                              \
    {                                                                          \
        LwU8 fbqBuffer[RM_##core##_FBQ_CMD_ELEMENT_SIZE];                      \
        LwU8 scratchBuffer[OSTASK_ATTRIBUTE_##task##_CMD_SCRATCH_SIZE];        \
    } LWOS_TASK_CMD_BUFFER(core, task)                                         \
        GCC_ATTRIB_SECTION(section, "cmdBuffer")

/*!
 * @brief   Generates definition of a static per-task command buffer. Scratch
 * buffer size is equal to the size of TASK_SURFACE_UNION_<task>, which is
 * expected to be defined upon macro usage.
 *
 * @param[in]   core    Name of the core
 * @param[in]   task    Name of the task for the buffer
 * @param[in]   section Section in which to place the buffer
 */
#define LWOS_TASK_CMD_BUFFER_DEFINE_SCRATCH_SIZE(core, task, section)          \
    static struct                                                              \
    {                                                                          \
        LwU8 fbqBuffer[RM_##core##_FBQ_CMD_ELEMENT_SIZE];                      \
        LwU8 scratchBuffer[sizeof(TASK_SURFACE_UNION_##task)];                 \
    } LWOS_TASK_CMD_BUFFER(core, task)                                         \
        GCC_ATTRIB_SECTION(section, "cmdBuffer")

/*!
 * @brief   Retrieves a structure containing the command buffer's pointers and
 *          sizes.
 *
 * @param[in]   core    Name of the core
 * @param[in]   task    Name of the task for the buffer
 *
 * @return  Pointer to a @ref LWOS_TASK_CMD_BUFFER_PTRS structure
 */
#define LWOS_TASK_CMD_BUFFER_PTRS_GET(core, task)                              \
    (&(LWOS_TASK_CMD_BUFFER_PTRS)                                              \
    {                                                                          \
        .pFbqBuffer =                                                          \
            &LWOS_TASK_CMD_BUFFER(core, task).fbqBuffer[0],                    \
        .pScratchBuffer =                                                      \
            &LWOS_TASK_CMD_BUFFER(core, task).scratchBuffer[0],                \
        .fbqBufferSize =                                                       \
            sizeof(LWOS_TASK_CMD_BUFFER(core, task).fbqBuffer),                \
        .scratchBufferSize =                                                   \
            sizeof(LWOS_TASK_CMD_BUFFER(core, task).scratchBuffer),            \
    })

/*!
 * @brief   Represents an invalid/unused per-task command buffer
 */
#define LWOS_TASK_CMD_BUFFER_ILWALID                                           \
    ((LWOS_TASK_CMD_BUFFER_PTRS *)NULL)
/*!
 * @brief   Macro wrapping the code to copy in a command to a buffer
 *
 * @param[in]   _status     Return status of the copy-in
 * @param[in]   _pRequest   The request pointer received by the task loop
 * @param[out]  _pCmdBuf    If provided, @ref LWOS_TASK_CMD_BUFFER_PTRS
 *                          containing command buffer pointer information.
 */
#define LWOS_TASK_CMD_COPY_IN(_status, _pRequest, _pCmdBuf)                    \
    do                                                                         \
    {                                                                          \
        if ((_pCmdBuf) != LWOS_TASK_CMD_BUFFER_ILWALID)                        \
        {                                                                      \
            (_status) = lwosTaskCmdCopyIn(                                     \
                (_pRequest),                                                   \
                (_pCmdBuf)->pFbqBuffer,                                        \
                (_pCmdBuf)->fbqBufferSize,                                     \
                (_pCmdBuf)->pScratchBuffer,                                    \
                (_pCmdBuf)->scratchBufferSize);                                \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            (_status) = FLCN_OK;                                               \
        }                                                                      \
    }                                                                          \
    while (LW_FALSE)

/*!
 * @brief          Start macro with timer callback handling built in of a
 *                 infinite loop getting request from task queue. Must be used
 *                 in pair with @ref LWOS_TASK_LOOP_END wrapping client code to
 *                 handle the received request.
 *
 * @details        In addition, it starts watchdog, task profiling and sets up
 *                 stack canary if configured.
 *
 * @param[in]      _pQueue    The queue that the task is waiting on
 * @param[in, out] _pRequest  Buffer for the first request in the queue, which
 *                            will be populated on macro exit
 * @param[in, out] _status    Name of the variable to hold an exelwtion status
 * @param[out]     _pCmdBuf   If provided, @ref LWOS_TASK_CMD_BUFFER_PTRS
 *                            containing command buffer pointer information.
 */
#define LWOS_TASK_LOOP(_pQueue, _pRequest, _status, _pCmdBuf)                  \
    lwosTaskInitDone();                                                        \
    do {                                                                       \
        if (OS_QUEUE_WAIT_FOREVER((_pQueue), (_pRequest)))                     \
        {                                                                      \
            FLCN_STATUS _status;                                               \
            LWOS_USTREAMER_LOG_COMPACT_LWR_TASK(                               \
                ustreamerDefaultQueueId,                                       \
                LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_EXEC_PROFILE_BEGIN);       \
            LWOS_INSTRUMENTATION_LOG_LWR_TASK(                                 \
                LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_EXEC_PROFILE_BEGIN);       \
            LWOS_WATCHDOG_START_ITEM();                                        \
            LWOS_TASK_LOOP_SETUP_STACK_CANARY();                               \
            if ((_pRequest)->hdr.eventType == DISP2UNIT_EVT_OS_TMR_CALLBACK)   \
            {                                                                  \
                _status = osTmrCallbackExelwte(                                \
                    ((DISP2UNIT_OS_TMR_CALLBACK *)(_pRequest))->pCallback);    \
            }                                                                  \
            else                                                               \
            {                                                                  \
                LWOS_TASK_CMD_COPY_IN((_status), (_pRequest), (_pCmdBuf));     \
                if ((_status) == FLCN_OK)                                      \
                {

/*!
 * @brief          Start macro similar to @ref LWOS_TASK_LOOP but with no timer
 *                 callback handling built in.
 *
 * @param[in]      _pQueue    The queue that the task is waiting on
 * @param[in, out] _pRequest  Buffer for the first request in the queue, which
 *                            will be populated on macro exit
 * @param[in, out] _status    Name of the variable to hold an exelwtion status
 * @param[out]     _pCmdBuf   If provided, @ref LWOS_TASK_CMD_BUFFER_PTRS
 *                            containing command buffer pointer information.
 */
#define LWOS_TASK_LOOP_NO_CALLBACKS(_pQueue, _pRequest, _status, _pCmdBuf)     \
    lwosTaskInitDone();                                                        \
    do {                                                                       \
        if (OS_QUEUE_WAIT_FOREVER((_pQueue), (_pRequest)))                     \
        {                                                                      \
            FLCN_STATUS _status;                                               \
            LWOS_USTREAMER_LOG_COMPACT_LWR_TASK(                               \
                ustreamerDefaultQueueId,                                       \
                LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_EXEC_PROFILE_BEGIN);       \
            LWOS_INSTRUMENTATION_LOG_LWR_TASK(                                 \
                LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_EXEC_PROFILE_BEGIN);       \
            LWOS_WATCHDOG_START_ITEM();                                        \
            LWOS_TASK_LOOP_SETUP_STACK_CANARY();                               \
            {                                                                  \
                LWOS_TASK_CMD_COPY_IN((_status), (_pRequest), (_pCmdBuf));     \
                if ((_status) == FLCN_OK)                                      \
                {

/*!
 * @brief      Terminate macro that must be used in pair with either @ref
 *             LWOS_TASK_LOOP or @ref LWOS_TASK_LOOP_NO_CALLBACKS
 *
 * @param[in]  _status  Name of the variable that holds an exelwtion status
 */
#define LWOS_TASK_LOOP_END(_status)                                            \
                }                                                              \
            }                                                                  \
            lwosTaskEventErrorHandler(_status);                                \
            LWOS_WATCHDOG_COMPLETE_ITEM();                                     \
            LWOS_USTREAMER_LOG_COMPACT_LWR_TASK(                               \
                ustreamerDefaultQueueId,                                       \
                LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_EXEC_PROFILE_END);         \
            LWOS_INSTRUMENTATION_LOG_LWR_TASK(                                 \
                LW2080_CTRL_FLCN_LWOS_INST_EVT_TASK_EXEC_PROFILE_END);         \
        }                                                                      \
    } while (LW_TRUE)

#if defined(DYNAMIC_FLCN_PRIV_LEVEL)
/*!
 * @brief      Start macro for changing task privilege level, while at the same
 *             time saving current privilege level. This macro *must* be used in
 *             pair with @ref OS_TASK_SET_PRIV_LEVEL_END.
 *
 * @param[in]  _privLevelExt  - new privilege level for Ext
 * @param[in]  _privLevelCsb  - new privilege level for Csb
 */
#define OS_TASK_SET_PRIV_LEVEL_BEGIN(_privLevelExt, _privLevelCsb)             \
    do {                                                                       \
        LwU8 _lwrPrivLevelExt;                                                 \
        LwU8 _lwrPrivLevelCsb;                                                 \
                                                                               \
        osFlcnPrivLevelLwrrTaskGet(&_lwrPrivLevelExt, &_lwrPrivLevelCsb);      \
        osFlcnPrivLevelLwrrTaskSet((_privLevelExt), (_privLevelCsb));          \
        CHECK_SCOPE_BEGIN(OS_TASK_SET_PRIV_LEVEL);                             \
        {

/*!
 * @brief      Terminate macro that *must* be used in pair with @ref
 *             OS_TASK_SET_PRIV_LEVEL_BEGIN. This macro simply restores
 *             privilege levels to to one before the temporary change.
 */
#define OS_TASK_SET_PRIV_LEVEL_END()                                           \
        }                                                                      \
        CHECK_SCOPE_END(OS_TASK_SET_PRIV_LEVEL);                               \
        osFlcnPrivLevelLwrrTaskSet(_lwrPrivLevelExt, _lwrPrivLevelCsb);        \
    } while (LW_FALSE)

#else // defined(DYNAMIC_FLCN_PRIV_LEVEL)

/*!
 * @brief      Stub implementation of OS_TASK_SET_PRIV_LEVEL_BEGIN
 */
#define OS_TASK_SET_PRIV_LEVEL_BEGIN(_privLevelExt, _privLevelCsb)             \
    do {                                                                       \
        CHECK_SCOPE_BEGIN(OS_TASK_SET_PRIV_LEVEL);                             \
        {

/*!
 * @brief      Stub implementation of OS_TASK_SET_PRIV_LEVEL_END
 */
#define OS_TASK_SET_PRIV_LEVEL_END()                                           \
        }                                                                      \
        CHECK_SCOPE_END(OS_TASK_SET_PRIV_LEVEL);                               \
    } while (LW_FALSE)

#endif // defined(DYNAMIC_FLCN_PRIV_LEVEL)

/*!
 * @brief      Get the DMEM physical address corresponding to a DMEM virtual
 *             address.
 *
 * @param[in]  ptr   The VA whose PA we want
 *
 * @return     the PA
 *
 * @note       The DMEM blocks may be paged in or out during context
 *             switches. So, the PA should be found and used within a critical
 *             section, to make sure that the returned PA remains valid till
 *             it's used.
 */
#ifndef UPROC_RISCV
static inline LwU32 osDmemAddressVa2PaGet(void *ptr)
    GCC_ATTRIB_ALWAYSINLINE();

static inline LwU32 osDmemAddressVa2PaGet
(
    void *ptr
)
{
    LwU32 dmemPa;
    LwU32 dmemVa;

    dmemVa = (LwU32)LW_PTR_TO_LWUPTR(ptr);

#ifdef DMEM_VA_SUPPORTED
    LwU32 dmtag;
    LwU32 vaBound;

    vaBound = DRF_VAL(_PFALCON, _FALCON_DMVACTL, _BOUND,
                      lwuc_ldx_i(CSB_REG(_DMVACTL), 0));

    if (dmemVa >= DMEM_IDX_TO_ADDR(vaBound))
    {
        falc_dmtag(&dmtag, dmemVa);
        dmemPa = DMEM_IDX_TO_ADDR(DMTAG_IDX_GET(dmtag));

        //
        // If this block is not paged into DMEM, the address bits dmtag
        // is 0. Accessing block 0 will give incorrect results.
        // Catch this case with a HALT. Write the VA into the mailbox register
        // to help with debugging.
        //
        if (dmemPa == 0U)
        {
            lwuc_stx(CSB_REG(_MAILBOX0), dmemVa);
            lwrtosHALT();
        }

        dmemPa |= dmemVa & (DMEM_BLOCK_SIZE - 1U);
    }
    else
#endif // DMEM_VA_SUPPORTED
    {
        dmemPa = dmemVa;
    }
    return dmemPa;
}
#endif // UPROC_RISCV

/* ------------------------ Types definitions ------------------------------- */
/*!
 * @brief   Information on the location of per-task command buffers.
 */
typedef struct
{
    /*!
     * @brief   Pointer to buffer for holding data from the FbQueue.
     */
    void *pFbqBuffer;

    /*!
     * @brief   Pointer to buffer that is available for "scratch" space for
     *          command.
     */
    void *pScratchBuffer;

    /*!
     * @brief   Size of @ref LWOS_TASK_CMD_BUFFER_PTRS::pFbqBuffer buffer .
     */
    LwLength fbqBufferSize;

    /*!
     * @brief   Size of @ref LWOS_TASK_CMD_BUFFER_PTRS::pScratchBuffer buffer.
     */
    LwLength scratchBufferSize;
} LWOS_TASK_CMD_BUFFER_PTRS;

/* ------------------------ External definitions ---------------------------- */
#ifndef UPROC_RISCV
/*!
 * Points to private TCB of lwrrently running task.
 */
extern PRM_RTOS_TCB_PVT pPrivTcbLwrrent;
#endif // UPROC_RISCV

/*!
 * Handle to the Idle task.
 */
extern LwrtosTaskHandle OsTaskIdle;

/*!
 * Value used for stack smashing protection.
 */
#ifdef IS_SSP_ENABLED
extern void * __stack_chk_guard;
#endif // IS_SSP_ENABLED

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

#define lwosTaskCreate(...) MOCKABLE(lwosTaskCreate)(__VA_ARGS__)
FLCN_STATUS lwosTaskCreate(LwrtosTaskHandle *pxTaskHandle,
                           PRM_RTOS_TCB_PVT pTcbPvt,
                           LwrtosTaskFunction pvTaskCode,
                           LwU8 taskID,
                           LwU16 usStackDepth,
                           LwBool bUsingFPU,
                           LwU32 uxPriority,
                           LwU8 privLevelExt,
                           LwU8 privLevelCsb,
                           LwU8 ovlCntImemLS,
                           LwU8 ovlCntImemHS,
                           LwU8 ovlIdxImem,
                           LwU8 ovlCntDmem,
                           LwU8 ovlIdxStack,
                           LwU8 ovlIdxDmem,
                           LwBool bEnableImemOnDemandPaging)
    GCC_ATTRIB_SECTION("imem_init", "lwosTaskCreate");
#ifdef IS_SSP_ENABLED
void        lwosTaskSetupCanary(void)
    GCC_ATTRIB_SECTION("imem_resident", "lwosTaskSetupCanary");
#endif // IS_SSP_ENABLED
LwS32       osTaskLoad(LwrtosTaskHandle pxTask, LwU32 *pxLoadTimeTicks);

#define osTaskIdGet(...) MOCKABLE(osTaskIdGet)(__VA_ARGS__)
LwU8        osTaskIdGet(void);

#if defined(DMEM_VA_SUPPORTED) && defined(HS_OVERLAYS_ENABLED)
void        lwosOvlImemLoadAllResidentHS(void)
#ifdef HS_UCODE_ENCRYPTION
    GCC_ATTRIB_SECTION("imem_resident", "lwosOvlImemLoadAllResidentHS");
#else // HS_UCODE_ENCRYPTION
    GCC_ATTRIB_SECTION("imem_init", "lwosOvlImemLoadAllResidentHS");
#endif // HS_UCODE_ENCRYPTION
#endif //defined(DMEM_VA_SUPPORTED) && defined(HS_OVERLAYS_ENABLED)

#define lwosTaskEventErrorHandler(...) MOCKABLE(lwosTaskEventErrorHandler)(__VA_ARGS__)
void        lwosTaskEventErrorHandler(FLCN_STATUS status)
    GCC_ATTRIB_SECTION("imem_resident", "lwosTaskEventErrorHandler");

#if defined(DYNAMIC_FLCN_PRIV_LEVEL)
void        osFlcnPrivLevelSetFromLwrrTask(void)
    GCC_ATTRIB_SECTION("imem_resident", "osFlcnPrivLevelSetFromLwrrTask");
void        osFlcnPrivLevelReset(void)
    GCC_ATTRIB_SECTION("imem_resident", "osFlcnPrivLevelReset");

#define     osFlcnPrivLevelLwrrTaskSet(...) MOCKABLE(osFlcnPrivLevelLwrrTaskSet)(__VA_ARGS__)
void        osFlcnPrivLevelLwrrTaskSet(LwU8 newPrivLevelExt, LwU8 newPrivLevelCsb)
    GCC_ATTRIB_SECTION("imem_resident", "osFlcnPrivLevelLwrrTaskSet");

#define     osFlcnPrivLevelLwrrTaskGet(...) MOCKABLE(osFlcnPrivLevelLwrrTaskGet)(__VA_ARGS__)
void        osFlcnPrivLevelLwrrTaskGet(LwU8 *pPrivLevelExt, LwU8 *pPrivLevelCsb)
    GCC_ATTRIB_SECTION("imem_resident", "osFlcnPrivLevelLwrrTaskGet");

void        osFlcnPrivLevelLwrrTaskReset(void)
    GCC_ATTRIB_SECTION("imem_resident", "osFlcnPrivLevelLwrrTaskReset");

extern void vApplicationFlcnPrivLevelSet(LwU8 privLevelExt, LwU8 privLevelCsb);
extern void vApplicationFlcnPrivLevelReset(void);
#else
#define     osFlcnPrivLevelSetFromLwrrTask()                             lwosNOP()
#define     osFlcnPrivLevelReset()                                       lwosNOP()
#define     osFlcnPrivLevelLwrrTaskSet(newPrivLevelExt, newPrivLevelCsb) lwosNOP()
#define     osFlcnPrivLevelLwrrTaskGet(pPrivLevelExt, pPrivLevelCsb)     lwosNOP()
#define     osFlcnPrivLevelLwrrTaskReset(pPrivLevelExt, pPrivLevelCsb)   lwosNOP()
#endif //defined(DYNAMIC_FLCN_PRIV_LEVEL)

#define lwosInit(...) MOCKABLE(lwosInit)(__VA_ARGS__)
FLCN_STATUS lwosInit(void)
    GCC_ATTRIB_SECTION("imem_init", "lwosInit");
FLCN_STATUS lwosResVAddrToPhysMemOffsetMap(void *pVAddr, LwU32 *pOffset)
    GCC_ATTRIB_SECTION("imem_resident", "lwosResVAddrToPhysMemOffsetMap");

#if !defined(GSPLITE_RTOS) || !defined(UPROC_RISCV)
void vApplicationIdleTaskCreate(LwrtosTaskFunction pvTaskCode, LwU32 uxPriority)
    GCC_ATTRIB_SECTION("imem_init", "vApplicationIdleTaskCreate");
#endif // !defined(GSPLITE_RTOS) || !defined(UPROC_RISCV)

extern LwU16 vApplicationIdleTaskStackSize(void)
    GCC_ATTRIB_SECTION("imem_init", "vApplicationIdleTaskStackSize");

/*!
 * @brief   If a request is a command, then copies the command data into a
 *          buffer.
 *
 * @param[in]   pRequest    Request from which to copy in (if a command request)
 * @param[out]  pCmdBuf     Buffer pointer information to be used for command
 *
 * @return  FLCN_OK Success
 * @return  Others  Failure to copy in
 */
FLCN_STATUS lwosTaskCmdCopyIn(void *pRequest, void *pFbqBuffer, LwLength fbqBufferSize, void *pScratchBuffer, LwLength scratchBufferSize)
    GCC_ATTRIB_SECTION("imem_resident", "lwosTaskCmdCopyIn");

#define lwosTaskCriticalExit     lwrtosTaskCriticalExit
#define lwosTaskSchedulerResume  lwrtosTaskSchedulerResume
#define lwosTaskCriticalEnter    lwrtosTaskCriticalEnter
#define lwosTaskSchedulerSuspend lwrtosTaskSchedulerSuspend

#endif // LWOSTASK_H
