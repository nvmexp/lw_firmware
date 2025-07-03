/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef UNIT_API_H
#define UNIT_API_H

/*!
 * @file   unit_api.h
 * @brief  Defines the generic data-structures required to dispatch work-items
 *         to unit-tasks for processing.
 *
 * Nearly all unit-tasks are implemented using an event-queue that is used
 * to dispatch work-items to that task for processing.  Queuing an item in
 * the task's event-queue will cause the scheduler to move the task into the
 * "ready" state.  Any strategy may be used to queue work-items in these
 * queues.  In general however, unit-tasks need to be fed commands from the PMU
 * command queues and notified of interrupt events that require special
 * attention. This file defines the generic data structures used by most
 * tasks to receive commands. Generic interrupt notification is not yet
 * defined here.  Specific mechanisms for dispatching command and interrupts
 * are used on a per-task basis are are defined outside this file.
 */

#include <lwtypes.h>
#include "boardobj/boardobjgrp.h"
#include "rmpmucmdif.h"
#include "lwostmrcallback.h"

/*!
 * Event types:
 *
 *  DISP2UNIT_EVT_SIGNAL
 *      Identifier used to represent the signal dispatch wrapper
 *
 *  DISP2UNIT_EVT_RM_RPC
 *      RM RPC dispatch wrapper (@see DISP2UNIT_RM_RPC)
 *
 *  DISP2UNIT_EVT_OS_TMR_CALLBACK
 *      Generic RTOS timer callback notification
 *
 *  DISP2UNIT_EVT__COUNT
 *      Some tasks enhance this enum to define a safe point to start new
 *      values for the events.
 */
#define DISP2UNIT_EVT_SIGNAL           (0x00U)
#define DISP2UNIT_EVT_RM_RPC           (0x01U)
#define DISP2UNIT_EVT_OS_TMR_CALLBACK  (0x02U)
#define DISP2UNIT_EVT__COUNT           (0x03U)

typedef struct
{
    /*!
     * @brief Unique-identifier representing this type of dispatch wrapper.
     *
     * This field is intended to store a unique-id that represents the type of
     * work-item that is being dispatched.  Typically, @a DISP2UNIT_EVT_RM_RPC
     * is stored here indicating that a command has been dispatched to the unit-
     * task.
     */
    LwU8    eventType;
    LwU8    unitId;
} DISP2UNIT_HDR;

/*!
 * @brief   Information about where a command is located in an FbQueue
 */
typedef struct
{
    /*!
     * @brief   Element index in @ref DISP2UNIT_RM_RPC::cmdQueueId for the command.
     */
    LwU8    elementIndex;

    /*!
     * @brief   Whether the function for sweeping the queue tail pointer should be skipped.
     * It should be always LW_FALSE and set to LW_TRUE only for the special case(s).
     */
    LwBool  bSweepSkip;

    /*!
     * @brief   Size of the data in the FbQueue
     */
    LwU16   elementSize;

    /*!
     * @brief   Size of the scratch buffer available for this command.
     */
    LwU32   scratchSize;

    /*!
     * @brief   Pointer to scratch buffer available for this command.
     */
    void   *pScratch;
} DISP2UNIT_RM_RPC_FBQ_PTCB;

/*!
 * Represents a wrapper that is used encapsulate all the information required
 * to dispatch a PMU command to a unit-task for processing.
 */
typedef struct
{
    /*!
     * @brief   Unique-identifier representing this type of dispatch wrapper.
     *
     * This field is intended to store a unique-id that represents the type of
     * work-item that is being dispatched. Typically a DISP2UNIT_EVT_RM_RPC is
     * stored here indicating that a RPC has been dispatched to the unit-task.
     */
    DISP2UNIT_HDR               hdr;

    /*!
     * @brief   The command queue from which the command originated.
     */
    LwU8                        cmdQueueId;

#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
    /*!
     * @brief   Contains information about the command's location in the
     *          FbQueues.
     *
     * @details This is necessary for using per-task command buffers, because
     *          the receiving task needs to know from where to copy data.
     */
    DISP2UNIT_RM_RPC_FBQ_PTCB   fbqPtcb;
#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)

    /*!
     * @brief   Pointer to the RPC structure (in the command queue or local copy)
     */
    RM_FLCN_CMD_PMU            *pRpc;
} DISP2UNIT_RM_RPC;

/*!
 * Generic RTOS timer callback notification structure.
 */
typedef struct
{
    DISP2UNIT_HDR       hdr;
    OS_TMR_CALLBACK    *pCallback;
} DISP2UNIT_OS_TMR_CALLBACK;

/* ------------------------- Inline Functions ------------------------------- */
/*!
 * @brief   Returns @ref DISP2UNIT_RM_RPC::fbqPtcb for the given dispatch
 *          structure, if valid
 *
 * @param[in]   pDispatch   Dispatch structure for the command
 *
 * @return  pDispatch's @ref DISP2UNIT_RM_RPC::fbqPtcb member if valid
 * @return  NULL if PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER is not enabled.
 */
static inline DISP2UNIT_RM_RPC_FBQ_PTCB *
disp2unitCmdFbqPtcbGet
(
    DISP2UNIT_RM_RPC *pDispatch
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
    return &pDispatch->fbqPtcb;
#else // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
    return NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
}

/*!
 * @copydoc disp2unitCmdFbqPtcbGet
 *
 * @note    Used to retrieve a const @ref DISP2UNIT_RM_RPC_FBQ_PTCB from a
 *          const dispatch structure.
 */
static inline const DISP2UNIT_RM_RPC_FBQ_PTCB *
disp2unitCmdFbqPtcbGetConst
(
    const DISP2UNIT_RM_RPC *pDispatch
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
    return &pDispatch->fbqPtcb;
#else // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
    return NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_PER_TASK_COMMAND_BUFFER)
}

#endif // UNIT_API_H

