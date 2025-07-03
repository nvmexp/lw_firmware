/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef UNIT_DISPATCH_H
#define UNIT_DISPATCH_H

/*!
 * @file   unit_dispatch.h
 * @brief  Defines the generic data-structures required to dispatch work-items
 *         to unit-tasks for processing.
 *
 * Nearly all unit-tasks are implemented using an event-queue that is used
 * to dispatch work-items to that task for processing.  Queuing an item in
 * the task's event-queue will cause the scheduler to move the task into the
 * "ready" state.  Any strategy may be used to queue work-items in these
 * queues.  In general however, unit-tasks need to be fed commands from SEC2
 * command queues and notified of interrupt events that require special
 * attention. This file defines the generic data structures used by most
 * tasks to receive commands. Generic interrupt notification is not yet
 * defined here.  Specific mechanisms for dispatching command and interrupts
 * are used on a per-task basis are defined outside this file.
 */

#include <lwtypes.h>
#include "rmsec2cmdif.h"

/*!
 * Identifier used to represent the signal dispatch wrapper
 */
#define  DISP2UNIT_EVT_SIGNAL   (0x00)

/*!
 * Identifier used to represent the command dispatch wrapper (DISP2UNIT_CMD)
 *
 * @see DISP2UNIT_CMD
 */
#define  DISP2UNIT_EVT_COMMAND  (0x01)

/*!
 * Identifier used to represent the GPCCS bootstrapping command from PMU
 *
 * @see DISP2UNIT_CMD
 */
#define  DISP2UNIT_EVT_COMMAND_BOOTSTRAP_GPCCS_FOR_GPCRG  (0x02)

/*!
 * Defines the maximum number of event-types per unit-task
 */
#define MAX_EVT_PER_UNIT       8

/*!
 * Represents a wrapper that is used encapsulate all the information required
 * to dispatch a SEC2 command to a unit-task for processing.
 */
typedef struct
{
    /*!
     * @brief Unique-identifier representing this type of dispatch wrapper.
     *
     * This field is intended to store a unique-id that represents the type of
     * work-item that is being dispatched.  Typically, @a DISP2UNIT_EVT_COMMAND
     * is stored here indicating that a command has been dispatched to the unit-
     * task.
     */
    LwU8              eventType;

    /*!
     * @brief The command queue from which the command originated.
     */
    LwU8              cmdQueueId;

    /*!
     * @brief Pointer to the command structure (in the command queue)
     */
    RM_FLCN_CMD_SEC2 *pCmd;
}DISP2UNIT_CMD;

#endif // UNIT_DISPATCH_H


