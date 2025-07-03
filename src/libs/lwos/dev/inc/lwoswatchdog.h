/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file       lwoswatchdog.h
 *
 * @brief      Header containing definitions for debugging and abstractions for
 *             tasks.
 */

#ifndef LWOSWATCHDOG_H
#define LWOSWATCHDOG_H

/* ------------------------- System Includes -------------------------------- */
#include "flcnretval.h"
#include "lwtypes.h"
#include "lwuproc.h"

/* ------------------------- Enum Definitions ------------------------------- */
/*!
 * @brief      Enumeration of the various statuses that a task can have.
 *
 * @details    The statuses are not exclusive and multiple bits can be set at
 *             once. The goal of the individual API calls is to preserve the
 *             status bits where possible and let the CheckStatus function
 *             inspect all of them and clear uneccessary bits.
 *
 * @note       The status values are unitless and are to be stored in an LwU8
 *             type.
 */
typedef enum
{
    /*!
     * @brief      Set by the Start API call to indicate that a task has started
     *             work on an item.
     */
    LWOS_WATCHDOG_ITEM_STARTED   = 0U,

    /*!
     * @brief      Set by the Complete API call to indicate that a task has
     *             completed work on an item.
     *
     * @details    This is to allow the watchdog task to differentiate between
     *             tasks not starting and tasks who have finished but have not
     *             had a chance to start another item.
     */
    LWOS_WATCHDOG_ITEM_COMPLETED = 1U,

    /*!
     * @brief      Set by the CheckStatus API call to indicate that this task
     *             has missed its deadline.
     */
    LWOS_WATCHDOG_ITEM_TIMEOUT   = 2U,

    /*!
     * @brief      Set by the CheckStatus API call to indicate that a task has
     *             pending work items, but has not started processing any.
     */
    LWOS_WATCHDOG_TASK_WAITING   = 3U,
} LWOS_WATCHDOG_STATUS;

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @struct     WATCHDOG_TASK_STATE
 *
 * @brief      Structure to track monitored Watchdog data.
 *
 * Member                  | Description
 * ------------------------|----------
 * LwU64 lastItemStartTime | @copydoc WATCHDOG_TASK_STATE::lastItemStartTime
 * LwU32 taskTimeoutNs     | @copydoc WATCHDOG_TASK_STATE::taskTimeoutNs
 * LwU8 itemsInQueue       | @copydoc WATCHDOG_TASK_STATE::itemsInQueue
 * LwU8 rsvd[2]            | @copydoc WATCHDOG_TASK_STATE::rsvd
 */
typedef struct
{
    /*!
     * @var        WATCHDOG_TASK_STATE::lastItemStartTime
     *
     * @brief      Timestamp indicating the last time a task indicated that it
     *             was starting a task item.
     *
     * @note       The unit is nanoseconds.
     */
    LwU64 lastItemStartTime;

    /*!
     * @var        WATCHDOG_TASK_STATE::taskTimeoutNs
     *
     * @brief      Constant value initalized during WatchdogInit that indicates
     *             the maximum amount of time a task can work on an item.
     *
     * @note       The unit is nanoseconds.
     */
    LwU32 taskTimeoutNs;

    /*!
     * @var        WATCHDOG_TASK_STATE::itemsInQueue
     *
     * @brief      Counter indicating the number of work items in a task's
     *             queue.
     *
     * @note       The assumption is that no queue shall have a max size larger
     *             than 255 elements. Additionally, the count is unitless.
     */
    LwU8  itemsInQueue;

    /*!
     * @var        WATCHDOG_TASK_STATE::taskStatus
     *
     * @brief      Bitfield indicating the status of the current work item.
     *
     * @note       The bitfield is unitless.
     */
    LwU8  taskStatus;

    /*!
     * @var        WATCHDOG_TASK_STATE::rsvd
     *
     * @brief      Due to alignment, extra bytes remain for future features.
     *
     * @note       The space reservation is unitless.
     */
    LwU8  rsvd[2];
} WATCHDOG_TASK_STATE;

/* ------------------------ Function Prototypes ----------------------------- */
FLCN_STATUS lwosWatchdogSetParameters(WATCHDOG_TASK_STATE *pTaskInfo, LwU32 activeTaskMask)
    GCC_ATTRIB_SECTION("imem_init", "lwosWatchdogSetParameters");
FLCN_STATUS lwosWatchdogAddItem(const LwU8 taskId)
    GCC_ATTRIB_SECTION("imem_resident", "lwosWatchdogAddItem");

#define lwosWatchdogAddItemFromISR MOCKABLE(lwosWatchdogAddItemFromISR)
FLCN_STATUS lwosWatchdogAddItemFromISR_IMPL(const LwU8 taskId)
    GCC_ATTRIB_SECTION("imem_resident", "lwosWatchdogAddItemFromISR_IMPL");

FLCN_STATUS lwosWatchdogAttachAndStartItem(void)
    GCC_ATTRIB_SECTION("imem_resident", "lwosWatchdogAttachAndStartItem");
FLCN_STATUS lwosWatchdogAttachAndCompleteItem(void)
    GCC_ATTRIB_SECTION("imem_resident", "lwosWatchdogAttachAndCompleteItem");
FLCN_STATUS lwosWatchdogCheckStatus(LwU32 *pFailingTaskMask)
    GCC_ATTRIB_SECTION("imem_watchdog", "lwosWatchdogCheckStatus");
FLCN_STATUS lwosWatchdogGetTaskIndex(const LwU32 activeMask, const LwU8 taskId, LwU8 *pTaskIdx)
    GCC_ATTRIB_SECTION("imem_resident", "lwosWatchdogGetTaskIndex");

#define lwosWatchdogCallbackExpired MOCKABLE(lwosWatchdogCallbackExpired)
void lwosWatchdogCallbackExpired(void)
    GCC_ATTRIB_SECTION("imem_resident", "lwosWatchdogCallbackExpired");

/* ------------------------- Defines ---------------------------------------- */

//
// Wrapper defines for the Watchdog API that automatically uses the current task
// ID where possible. If the watchdog is not enabled, all the defines are NOPs.
//
#ifdef WATCHDOG_ENABLE
/*!
 * @brief      Wrapper for @ref lwosWatchdogAddItem.
 */
#define LWOS_WATCHDOG_ADD_ITEM(taskId) (void)lwosWatchdogAddItem(taskId)
/*!
 * @brief      Wrapper for @ref lwosWatchdogAddItemFromISR.
 */
#define LWOS_WATCHDOG_ADD_ITEM_FROM_ISR(taskId) (void)lwosWatchdogAddItemFromISR(taskId)
/*!
 * @brief      Wrapper for @ref lwosWatchdogAttachAndStartItem.
 */
#define LWOS_WATCHDOG_START_ITEM() (void)lwosWatchdogAttachAndStartItem()
/*!
 * @brief      Wrapper for @ref lwosWatchdogAttachAndCompleteItem.
 */
#define LWOS_WATCHDOG_COMPLETE_ITEM() (void)lwosWatchdogAttachAndCompleteItem()
#else
/*!
 * @brief      Functionally NOP as the watchdog is disabled.
 */
#define LWOS_WATCHDOG_ADD_ITEM(taskId)
/*!
 * @brief      Functionally NOP as the watchdog is disabled.
 */
#define LWOS_WATCHDOG_ADD_ITEM_FROM_ISR(taskId)
/*!
 * @brief      Functionally NOP as the watchdog is disabled.
 */
#define LWOS_WATCHDOG_START_ITEM()
/*!
 * @brief      Functionally NOP as the watchdog is disabled.
 */
#define LWOS_WATCHDOG_COMPLETE_ITEM()
#endif // WATCHDOG_ENABLE

#endif // LWOSWATCHDOG_H
